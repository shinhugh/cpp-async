#include "async.h"

#include "future.h"
#include "promise_future_state.h"
#include "promise.h"
#include "thread_local_task_context.h"

#include "telemetry/living_span.h"
#include "telemetry/span.h"

#include <boost/context/continuation_fcontext.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

size_t async::impl::g_threadCount = 0;
std::mutex async::impl::g_threadCountMutex;
std::vector<std::thread> async::impl::g_completeThreads;
std::mutex async::impl::g_completeThreadsMutex;
size_t async::impl::g_coroutineCount = 0;
std::mutex async::impl::g_coroutineCountMutex;
std::vector<std::shared_ptr<boost::context::continuation>>
  async::impl::g_readyContinuations;
std::recursive_mutex async::impl::g_readyContinuationsMutex;
std::condition_variable_any async::impl::g_allTasksCv;
static std::vector<
  std::pair<
    std::chrono::steady_clock::time_point,
    std::function<void()>>>
  s_timedRequeueCallbacksContainer;
static std::priority_queue s_timedRequeueCallbacks{
  [](
    const std::pair<
      std::chrono::steady_clock::time_point,
      std::function<void()>>&
      a,
    const std::pair<
      std::chrono::steady_clock::time_point,
      std::function<void()>>&
      b)
  {
    return a.first < b.first;
  },
  s_timedRequeueCallbacksContainer };
static std::mutex s_timedRequeueCallbacksMutex;

// -----------------------------------------------------------------------------

std::optional<telemetry::Span> async::GetActiveSpan()
{
  impl::ThreadLocalThreadTaskContext* activeThreadContext
    = impl::GetThreadLocalThreadTaskContext();
  if (activeThreadContext)
  {
    return *activeThreadContext->m_span;
  }

  impl::ThreadLocalCoroutineTaskContext* activeCoroutineContext
    = impl::GetThreadLocalCoroutineTaskContext();
  if (activeCoroutineContext)
  {
    return *activeCoroutineContext->m_span;
  }

  return std::nullopt;
}

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunTaskOnNewThread(std::function<void()>&& task)
{
  std::shared_ptr<impl::PromiseFutureState<void>> promiseFutureState
    = std::make_shared<impl::PromiseFutureState<void>>();

  {
    std::lock_guard lock{ impl::g_threadCountMutex };
    impl::g_threadCount++;
  }
  impl::g_allTasksCv.notify_one();

  std::shared_ptr<std::thread> childThread = std::make_shared<std::thread>();
  *childThread = std::thread{ [
    task = std::move(task),
    promise = impl::Promise{ promiseFutureState },
    span = GetActiveSpan(),
    thisThread = childThread]() mutable
    {
      impl::ThreadLocalThreadTaskContext* context
        = impl::CreateThreadLocalThreadTaskContext();
      context->m_span = std::make_unique<telemetry::LivingSpan>(
        span ?
        telemetry::LivingSpan::Create(*span) :
        telemetry::LivingSpan::Create());

      task();

      context->m_span.reset();
      impl::DestroyThreadLocalThreadTaskContext();

      promise.Fulfill();

      {
        std::lock_guard lock{ impl::g_completeThreadsMutex };
        impl::g_completeThreads.push_back(std::move(*thisThread));
      }
      impl::g_allTasksCv.notify_one();
    } };

  return Future<void>{ promiseFutureState };
}

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunTaskOnNewCoroutine(std::function<void()>&& task)
{
  std::shared_ptr<impl::PromiseFutureState<void>> promiseFutureState
    = std::make_shared<impl::PromiseFutureState<void>>();

  boost::context::continuation childContext = boost::context::callcc([
    task = std::move(task),
    promise = impl::Promise{ promiseFutureState },
    span = GetActiveSpan()](
    boost::context::continuation&& parentContinuation) mutable
    {
      {
        std::lock_guard lock{ impl::g_coroutineCountMutex };
        impl::g_coroutineCount++;
      }
      impl::g_allTasksCv.notify_one();

      parentContinuation = parentContinuation.resume();

      impl::ThreadLocalCoroutineTaskContext* context
        = impl::GetThreadLocalCoroutineTaskContext();

      context->m_yieldCallback = [&parentContinuation]()
        {
          parentContinuation = parentContinuation.resume();
        };
      context->m_span = std::make_unique<telemetry::LivingSpan>(
        span ?
        telemetry::LivingSpan::Create(*span) :
        telemetry::LivingSpan::Create());

      task();

      context->m_span.reset();
      context->m_yieldCallback = {};

      {
        std::lock_guard lock{ impl::g_coroutineCountMutex };
        impl::g_coroutineCount--;
      }
      impl::g_allTasksCv.notify_one();

      promise.Fulfill();

      return std::move(parentContinuation);
    });

  {
    std::lock_guard lock{ impl::g_readyContinuationsMutex };
    impl::g_readyContinuations.push_back(
      std::make_shared<boost::context::continuation>(std::move(childContext)));
  }
  impl::g_allTasksCv.notify_one();

  return Future<void>{ promiseFutureState };
}

// -----------------------------------------------------------------------------

void async::Yield()
{
  YieldUntil(std::chrono::steady_clock::now());
}

// -----------------------------------------------------------------------------

void async::YieldFor(std::chrono::steady_clock::duration duration)
{
  YieldUntil(std::chrono::steady_clock::now() + duration);
}

// -----------------------------------------------------------------------------

void async::YieldUntil(std::chrono::steady_clock::time_point timePoint)
{
  impl::ThreadLocalCoroutineTaskContext* context
    = impl::GetThreadLocalCoroutineTaskContext();

  if (!context)
  {
    std::this_thread::sleep_until(timePoint);
  }

  else
  {
    {
      std::lock_guard lock{ s_timedRequeueCallbacksMutex };
      s_timedRequeueCallbacks.emplace(
        timePoint,
        std::move(context->m_requeueCallback));
    }
    impl::g_allTasksCv.notify_one();

    std::unique_ptr<telemetry::LivingSpan> span{ std::move(context->m_span) };
    context->m_span.reset();
    std::function<void()> yieldCallback = std::move(context->m_yieldCallback);
    context->m_yieldCallback = {};

    yieldCallback();

    context->m_yieldCallback = std::move(yieldCallback);
    context->m_span = std::move(span);
  }
}

// -----------------------------------------------------------------------------

int async::ExecuteProgram(std::function<int()>&& program)
{
  class AllTasksLock
  {
  public:
    AllTasksLock()
    {
      lock();
    }

    AllTasksLock(const AllTasksLock&) = delete;

    AllTasksLock(AllTasksLock&&) = delete;

    ~AllTasksLock()
    {
      unlock();
    }

    AllTasksLock& operator=(const AllTasksLock&) = delete;

    AllTasksLock& operator=(AllTasksLock&&) = delete;

    void lock()
    {
      std::lock(
        impl::g_threadCountMutex,
        impl::g_completeThreadsMutex,
        impl::g_coroutineCountMutex,
        impl::g_readyContinuationsMutex,
        s_timedRequeueCallbacksMutex);
    }

    void unlock()
    {
      s_timedRequeueCallbacksMutex.unlock();
      impl::g_readyContinuationsMutex.unlock();
      impl::g_coroutineCountMutex.unlock();
      impl::g_completeThreadsMutex.unlock();
      impl::g_threadCountMutex.unlock();
    }
  };

  Future<int> future = RunTaskOnNewCoroutine<int>(std::move(program));

  while (true)
  {
    std::vector<std::thread> completeThreads;
    std::vector<std::shared_ptr<boost::context::continuation>>
      readyContinuations;

    bool exit = false;
    {
      AllTasksLock lock;

      while (true)
      {
        if (impl::g_threadCount == 0 && impl::g_coroutineCount == 0)
        {
          exit = true;
          break;
        }

        std::chrono::steady_clock::time_point now{
          std::chrono::steady_clock::now() };

        while (!s_timedRequeueCallbacks.empty()
          && s_timedRequeueCallbacks.top().first <= now)
        {
          s_timedRequeueCallbacks.top().second();
          s_timedRequeueCallbacks.pop();
        }

        if (!impl::g_completeThreads.empty()
          || !impl::g_readyContinuations.empty())
        {
          completeThreads.swap(impl::g_completeThreads);
          impl::g_threadCount -= completeThreads.size();
          readyContinuations.swap(impl::g_readyContinuations);
          break;
        }

        if (s_timedRequeueCallbacks.empty())
        {
          impl::g_allTasksCv.wait(lock);
        }
        else
        {
          impl::g_allTasksCv.wait_until(
            lock,
            s_timedRequeueCallbacks.top().first);
        }
      }
    }
    if (exit)
    {
      break;
    }
    impl::g_allTasksCv.notify_one();

    for (std::thread& completeThread : completeThreads)
    {
      completeThread.join();
    }

    if (readyContinuations.size() > 0)
    {
      impl::ThreadLocalCoroutineTaskContext* context
        = impl::CreateThreadLocalCoroutineTaskContext();

      for (std::shared_ptr<boost::context::continuation>& readyContinuation
        : readyContinuations)
      {
        std::shared_ptr<std::mutex> continuationMutex
          = std::make_shared<std::mutex>();

        context->m_requeueCallback = [readyContinuation, continuationMutex]()
          {
            {
              std::lock_guard lock{ *continuationMutex };
            }

            {
              std::lock_guard lock{ impl::g_readyContinuationsMutex };
              impl::g_readyContinuations.push_back(readyContinuation);
            }
            impl::g_allTasksCv.notify_one();
          };

        {
          std::lock_guard lock{ *continuationMutex };
          *readyContinuation = readyContinuation->resume();
        }

        context->m_requeueCallback = {};
      }

      impl::DestroyThreadLocalCoroutineTaskContext();
    }
  }

  return future.Await();
}
