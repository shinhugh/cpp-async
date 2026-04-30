#include "async.h"

#include "future.h"
#include "promise_future_state.h"
#include "promise.h"
#include "thread_local_task_context.h"

#include "telemetry/living_span.h"
#include "telemetry/span.h"

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
