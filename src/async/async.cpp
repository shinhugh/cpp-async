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
std::mutex async::impl::g_readyContinuationsMutex;
std::condition_variable_any async::impl::g_allTasksCv;

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
  // TODO: Implement
}

// -----------------------------------------------------------------------------

void async::YieldFor(std::chrono::steady_clock::duration /*duration*/)
{
  // TODO: Implement
}

// -----------------------------------------------------------------------------

void async::YieldUntil(std::chrono::steady_clock::time_point /*timePoint*/)
{
  // TODO: Implement
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
        impl::g_readyContinuationsMutex);
    }

    void unlock()
    {
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

      while (impl::g_completeThreads.empty()
        && impl::g_readyContinuations.empty())
      {
        if (impl::g_threadCount == 0 && impl::g_coroutineCount == 0)
        {
          exit = true;
          break;
        }
        impl::g_allTasksCv.wait(lock);
      }

      if (exit)
      {
        break;
      }

      completeThreads.swap(impl::g_completeThreads);
      impl::g_threadCount -= completeThreads.size();
      readyContinuations.swap(impl::g_readyContinuations);
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
