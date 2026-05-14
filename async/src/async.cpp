#include "async.h"

#include "coroutine_context.h"
#include "future.h"
#include "promise.h"
#include "promise_future_state.h"

#include <boost/context/continuation_fcontext.hpp>

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

static bool ProcessTasks();

// -----------------------------------------------------------------------------

std::unordered_map<uint32_t, async::impl::Coroutine> async::impl::g_coroutines;
std::vector<uint32_t> async::impl::g_completeCoroutines;
std::vector<async::impl::Coroutine *> async::impl::g_readyCoroutines;
uint32_t async::impl::g_nextCoroutineId = 0;
std::mutex async::impl::g_tasksMutex;
std::condition_variable async::impl::g_tasksCv;

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

int async::RunApplication(
    std::function<int(int, char *[])> &&application, int argc, char *argv[])
{
  Future<int> future = RunOnNewCoroutine<int>(
      [
          application = std::move(application), argc, argv]()
      {
        return application(argc, argv);
      });

  while (true)
  {
    if (!ProcessTasks())
    {
      break;
    }
  }

  return future.Await();
}

// -----------------------------------------------------------------------------

static bool ProcessTasks()
{
  std::vector<uint32_t> completeCoroutines;
  std::vector<async::impl::Coroutine *> readyCoroutines;

  {
    std::unique_lock lock{async::impl::g_tasksMutex};

    while (true)
    {
      if (async::impl::g_coroutines.empty())
      {
        return false;
      }

      if (!async::impl::g_completeCoroutines.empty() ||
          !async::impl::g_readyCoroutines.empty())
      {
        completeCoroutines.swap(async::impl::g_completeCoroutines);
        readyCoroutines.swap(async::impl::g_readyCoroutines);
        break;
      }

      async::impl::g_tasksCv.wait(lock);
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
