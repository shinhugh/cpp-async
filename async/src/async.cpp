#include "async.h"

#include "coroutine_context.h"
#include "future.h"
#include "promise.h"
#include "promise_future_state.h"

#include <boost/context/continuation_fcontext.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

#define MAX_PROCESSOR_THREAD_COUNT 256

// -----------------------------------------------------------------------------

static bool ProcessTasks();

// -----------------------------------------------------------------------------

std::unordered_map<uint32_t, async::impl::Coroutine> async::impl::g_coroutines;
std::vector<uint32_t> async::impl::g_completeCoroutines;
std::vector<async::impl::Coroutine *> async::impl::g_readyCoroutines;
uint32_t async::impl::g_nextCoroutineId = 0;
std::unordered_map<uint32_t, async::impl::Thread> async::impl::g_threads;
std::vector<uint32_t> async::impl::g_completeThreads;
uint32_t async::impl::g_nextThreadId = 0;
std::recursive_mutex async::impl::g_tasksMutex;
std::condition_variable_any async::impl::g_tasksCv;
static std::vector<
    std::pair<std::chrono::steady_clock::time_point, std::function<void()>>>
    s_timedQueueCallbacksContainer;
static std::priority_queue s_timedQueueCallbacks{
    [](
        const std::pair<
            std::chrono::steady_clock::time_point, std::function<void()>> &a,
        const std::pair<
            std::chrono::steady_clock::time_point, std::function<void()>> &b)
    {
      return a.first > b.first;
    },
    s_timedQueueCallbacksContainer};

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunOnCurrentContext(
    const std::function<void(Promise<void>)> &task)
{
  std::shared_ptr<impl::PromiseFutureState<void>> promiseFutureState =
      std::make_shared<impl::PromiseFutureState<void>>();

  task(Promise{promiseFutureState});

  return Future{promiseFutureState};
}

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunOnNewCoroutine(
    std::function<void(Promise<void>)> &&task)
{
  return RunOnCurrentContext<void>(
      [
          task = std::move(task)](
          Promise<void> promise)
      {
        uint32_t coroutineId;
        impl::Coroutine *coroutine;
        {
          std::lock_guard lock{impl::g_tasksMutex};
          while (
              impl::g_coroutines.find(impl::g_nextCoroutineId) !=
              impl::g_coroutines.end())
          {
            impl::g_nextCoroutineId++;
          }
          coroutineId = impl::g_nextCoroutineId;
          impl::g_nextCoroutineId++;
          coroutine = &impl::g_coroutines[coroutineId];
        };
        impl::g_tasksCv.notify_one();

        coroutine->m_activeMutex = std::make_shared<std::mutex>();

        coroutine->m_continuation = boost::context::callcc(
            [
                task = std::move(task), promise = std::move(promise),
                coroutineId](
                boost::context::continuation &&parentContinuation) mutable
            {
              parentContinuation = parentContinuation.resume();

              impl::CoroutineContext *context =
                  impl::GetThreadLocalCoroutineContext();
              context->m_yieldCallback = [
                  &parentContinuation]()
              {
                parentContinuation = parentContinuation.resume();
              };

              task(std::move(promise));

              {
                std::lock_guard lock{impl::g_tasksMutex};
                impl::g_completeCoroutines.push_back(coroutineId);
              }
              impl::g_tasksCv.notify_one();

              return std::move(parentContinuation);
            });

        coroutine->m_context.m_queueCallback = [
            coroutine]()
        {
          {
            std::lock_guard lock{impl::g_tasksMutex};
            impl::g_readyCoroutines.push_back(coroutine);
          }
          impl::g_tasksCv.notify_one();
        };

        coroutine->m_context.m_queueCallback();
      });
}

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunOnNewCoroutine(
    std::function<void()> &&task)
{
  return RunOnNewCoroutine<void>(
      [
          task = std::move(task)](
          Promise<void> promise)
      {
        task();
        promise.Fulfill();
      });
}

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunOnNewThread(
    std::function<void(Promise<void>)> &&task)
{
  return RunOnCurrentContext<void>(
      [
          task = std::move(task)](
          Promise<void> promise)
      {
        uint32_t threadId;
        impl::Thread *thread;
        {
          std::lock_guard lock{impl::g_tasksMutex};
          while (
              impl::g_threads.find(impl::g_nextThreadId) !=
              impl::g_threads.end())
          {
            impl::g_nextThreadId++;
          }
          threadId = impl::g_nextThreadId;
          impl::g_nextThreadId++;
          thread = &impl::g_threads[threadId];
        };
        impl::g_tasksCv.notify_one();

        thread->m_thread =
            std::thread{[
                    task = std::move(task), promise = std::move(promise),
                    threadId]() mutable
                {
                  task(std::move(promise));

                  {
                    std::lock_guard lock{impl::g_tasksMutex};
                    impl::g_completeThreads.push_back(threadId);
                  }
                  impl::g_tasksCv.notify_one();
                }};
      });
}

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunOnNewThread(
    std::function<void()> &&task)
{
  return RunOnNewThread<void>(
      [
          task = std::move(task)](
          Promise<void> promise)
      {
        task();
        promise.Fulfill();
      });
}

// -----------------------------------------------------------------------------

void async::Yield()
{
  YieldUntil(std::chrono::steady_clock::now());
}

// -----------------------------------------------------------------------------

void async::YieldFor(
    std::chrono::steady_clock::duration duration)
{
  YieldUntil(std::chrono::steady_clock::now() + duration);
}

// -----------------------------------------------------------------------------

void async::YieldUntil(
    std::chrono::steady_clock::time_point timePoint)
{
  impl::CoroutineContext *context = impl::GetThreadLocalCoroutineContext();

  if (context)
  {
    {
      std::lock_guard lock{impl::g_tasksMutex};
      s_timedQueueCallbacks.emplace(timePoint, context->m_queueCallback);
    }
    impl::g_tasksCv.notify_one();

    context->m_yieldCallback();
  }

  else
  {
    std::this_thread::sleep_until(timePoint);
  }
}

// -----------------------------------------------------------------------------

int async::RunApplication(
    std::function<int(int, char *[])> &&application, int argc, char *argv[],
    size_t processorThreadCount)
{
  if (processorThreadCount == 0 ||
      processorThreadCount > MAX_PROCESSOR_THREAD_COUNT)
  {
    return 1;
  }

  Future<int> future = RunOnNewCoroutine<int>(
      [
          application = std::move(application), argc, argv]()
      {
        return application(argc, argv);
      });

  std::vector<std::thread> processorThreads;
  if (processorThreadCount > 1)
  {
    processorThreads.reserve(processorThreadCount - 1);
    for (size_t i = 0; i < processorThreadCount - 1; i++)
    {
      processorThreads.emplace_back(
          []()
          {
            while (true)
            {
              if (!ProcessTasks())
              {
                break;
              }
            }
          });
    }
  }

  while (true)
  {
    if (!ProcessTasks())
    {
      break;
    }
  }

  for (std::thread &processorThread : processorThreads)
  {
    processorThread.join();
  }

  return future.Await();
}

// -----------------------------------------------------------------------------

static bool ProcessTasks()
{
  std::vector<uint32_t> completeCoroutines;
  std::vector<uint32_t> completeThreads;
  std::vector<async::impl::Coroutine *> readyCoroutines;

  {
    std::unique_lock lock{async::impl::g_tasksMutex};

    while (true)
    {
      if (async::impl::g_coroutines.empty() && async::impl::g_threads.empty())
      {
        return false;
      }

      std::chrono::steady_clock::time_point now{
          std::chrono::steady_clock::now()};

      while (
          !s_timedQueueCallbacks.empty() &&
          s_timedQueueCallbacks.top().first <= now)
      {
        s_timedQueueCallbacks.top().second();
        s_timedQueueCallbacks.pop();
      }

      if (!async::impl::g_completeCoroutines.empty() ||
          !async::impl::g_completeThreads.empty() ||
          !async::impl::g_readyCoroutines.empty())
      {
        completeCoroutines.swap(async::impl::g_completeCoroutines);
        completeThreads.swap(async::impl::g_completeThreads);
        readyCoroutines.swap(async::impl::g_readyCoroutines);
        break;
      }

      if (s_timedQueueCallbacks.empty())
      {
        async::impl::g_tasksCv.wait(lock);
      }
      else
      {
        async::impl::g_tasksCv.wait_until(
            lock, s_timedQueueCallbacks.top().first);
      }
    }
  }
  async::impl::g_tasksCv.notify_one();

  for (uint32_t coroutineId : completeCoroutines)
  {
    std::shared_ptr<std::mutex> activeMutex;
    {
      std::lock_guard lock{async::impl::g_tasksMutex};
      activeMutex = async::impl::g_coroutines.at(coroutineId).m_activeMutex;
    }
    {
      std::lock_guard activeLock{*activeMutex};
      std::lock_guard tasksLock{async::impl::g_tasksMutex};
      async::impl::g_coroutines.erase(coroutineId);
    }
  }

  for (uint32_t threadId : completeThreads)
  {
    std::thread *thread;
    {
      std::lock_guard lock{async::impl::g_tasksMutex};
      thread = &async::impl::g_threads.at(threadId).m_thread;
    }
    thread->join();
    {
      std::lock_guard lock{async::impl::g_tasksMutex};
      async::impl::g_threads.erase(threadId);
    }
  }

  async::impl::g_tasksCv.notify_all();

  for (async::impl::Coroutine *coroutine : readyCoroutines)
  {
    async::impl::SetThreadLocalCoroutineContext(&coroutine->m_context);
    {
      std::lock_guard lock{*coroutine->m_activeMutex};
      coroutine->m_continuation = coroutine->m_continuation.resume();
    }
    async::impl::UnsetThreadLocalCoroutineContext();
  }

  return true;
}
