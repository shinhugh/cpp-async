#include "async.h"

#include "future.h"
#include "promise_future_state.h"
#include "promise.h"
#include "thread_local_task_context.h"

#include "telemetry/living_span.h"
#include "telemetry/span.h"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

std::unordered_map<std::thread::id, std::thread> async::impl::g_activeThreads;
std::mutex async::impl::g_activeThreadsMutex;
std::vector<std::thread> async::impl::g_completeThreads;
std::mutex async::impl::g_completeThreadsMutex;
std::condition_variable_any async::impl::g_allTasksCv;

// -----------------------------------------------------------------------------

std::optional<telemetry::Span> async::GetActiveSpan()
{
  // TODO: Implement
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
async::Future<void> async::RunTaskOnNewCoroutine(
  std::function<void()>&& /*task*/)
{
  std::shared_ptr<impl::PromiseFutureState<void>> promiseFutureState
    = std::make_shared<impl::PromiseFutureState<void>>();

  // TODO: Implement

  return Future<void>{ promiseFutureState };
}

// -----------------------------------------------------------------------------

int async::ExecuteProgram(std::function<int()>&& /*program*/)
{
  // TODO: Implement
  return 1;
}
