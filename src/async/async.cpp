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
#include <unordered_map>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

static size_t DestroyCompleteThreads();
static size_t RunReadyCoroutines();

// -----------------------------------------------------------------------------

std::unordered_map<std::thread::id, std::thread> async::impl::g_activeThreads;
std::mutex async::impl::g_activeThreadsMutex;
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

  std::shared_ptr<bool> threadAddedToActivePool = std::make_shared<bool>(false);
  std::shared_ptr<std::mutex> threadAddedToActivePoolMutex
    = std::make_shared<std::mutex>();
  std::shared_ptr<std::condition_variable> threadAddedToActivePoolCv
    = std::make_shared<std::condition_variable>();

  std::thread childThread{ [
    task = std::move(task),
    promise = impl::Promise{ promiseFutureState },
    span = GetActiveSpan(),
    threadAddedToActivePool,
    threadAddedToActivePoolMutex,
    threadAddedToActivePoolCv]() mutable
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
        std::unique_lock lock{ *threadAddedToActivePoolMutex };
        while (!*threadAddedToActivePool)
        {
          threadAddedToActivePoolCv->wait(lock);
        }
      }

      {
        std::scoped_lock lock{
          impl::g_activeThreadsMutex,
          impl::g_completeThreadsMutex };
        std::thread completeThread
          = std::move(impl::g_activeThreads.at(std::this_thread::get_id()));
        impl::g_activeThreads.erase(std::this_thread::get_id());
        impl::g_completeThreads.push_back(std::move(completeThread));
      }
      impl::g_allTasksCv.notify_one();
    } };

  {
    std::lock_guard lock{ impl::g_activeThreadsMutex };
    impl::g_activeThreads.emplace(childThread.get_id(), std::move(childThread));
  }
  impl::g_allTasksCv.notify_one();

  {
    std::lock_guard lock{ *threadAddedToActivePoolMutex };
    *threadAddedToActivePool = true;
  }
  threadAddedToActivePoolCv->notify_one();

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
        impl::g_activeThreadsMutex,
        impl::g_completeThreadsMutex,
        impl::g_coroutineCountMutex,
        impl::g_readyContinuationsMutex);
    }

    void unlock()
    {
      impl::g_readyContinuationsMutex.unlock();
      impl::g_coroutineCountMutex.unlock();
      impl::g_completeThreadsMutex.unlock();
      impl::g_activeThreadsMutex.unlock();
    }
  };

  Future<int> future = RunTaskOnNewCoroutine<int>(std::move(program));

  bool quit = false;
  while (!quit)
  {
    DestroyCompleteThreads();
    RunReadyCoroutines();

    AllTasksLock lock;
    while (impl::g_completeThreads.empty()
      && impl::g_readyContinuations.empty())
    {
      if (impl::g_activeThreads.empty() && impl::g_coroutineCount == 0)
      {
        quit = true;
        break;
      }
      impl::g_allTasksCv.wait(lock);
    }
  }

  return future.Await();
}

// -----------------------------------------------------------------------------

static size_t DestroyCompleteThreads()
{
  std::vector<std::thread> completeThreads;
  {
    std::lock_guard lock{ async::impl::g_completeThreadsMutex };
    completeThreads.swap(async::impl::g_completeThreads);
  }
  async::impl::g_allTasksCv.notify_one();

  for (std::thread& thread : completeThreads)
  {
    thread.join();
  }

  return completeThreads.size();
}

// -----------------------------------------------------------------------------

static size_t RunReadyCoroutines()
{
  std::vector<std::shared_ptr<boost::context::continuation>> readyContinuations;
  {
    std::lock_guard lock{ async::impl::g_readyContinuationsMutex };
    readyContinuations.swap(async::impl::g_readyContinuations);
  }
  async::impl::g_allTasksCv.notify_one();

  async::impl::ThreadLocalCoroutineTaskContext* context
    = async::impl::CreateThreadLocalCoroutineTaskContext();

  for (std::shared_ptr<boost::context::continuation>& readyContinuation
    : readyContinuations)
  {
    // TODO: It's possible that m_requeueCallback gets invoked and this
    //       continuation is resumed by another thread before this thread yields
    //       this session and updates the continuation.
    context->m_requeueCallback = [readyContinuation]()
      {
        {
          std::lock_guard lock{ async::impl::g_readyContinuationsMutex };
          async::impl::g_readyContinuations.push_back(readyContinuation);
        }
        async::impl::g_allTasksCv.notify_one();
      };

    *readyContinuation = readyContinuation->resume();
  }

  async::impl::DestroyThreadLocalCoroutineTaskContext();

  return readyContinuations.size();
}
