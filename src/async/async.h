#pragma once

#include "future.h"

#include "telemetry/span.h"

#include <functional>
#include <optional>

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

std::optional<telemetry::Span> GetActiveSpan();
template <typename T>
Future<T> RunTaskOnNewThread(std::function<T()>&&);
template <>
Future<void> RunTaskOnNewThread(std::function<void()>&&);
template <typename T>
Future<T> RunTaskOnNewCoroutine(std::function<T()>&&);
template <>
Future<void> RunTaskOnNewCoroutine(std::function<void()>&&);
int ExecuteProgram(std::function<int()>&&);

// -----------------------------------------------------------------------------

} // async

// -----------------------------------------------------------------------------

#include "promise_future_state.h"
#include "promise.h"
#include "thread_local_task_context.h"

#include "telemetry/living_span.h"

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

namespace async::impl
{

// -----------------------------------------------------------------------------

extern size_t g_threadCount;
extern std::mutex g_threadCountMutex;
extern std::vector<std::thread> g_completeThreads;
extern std::mutex g_completeThreadsMutex;
extern std::condition_variable_any g_allTasksCv;

// -----------------------------------------------------------------------------

} // async::impl

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunTaskOnNewThread(std::function<T()>&& task)
{
  std::shared_ptr<impl::PromiseFutureState<T>> promiseFutureState
    = std::make_shared<impl::PromiseFutureState<T>>();

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

      T result = task();

      context->m_span.reset();
      impl::DestroyThreadLocalThreadTaskContext();

      promise.Fulfill(std::move(result));

      {
        std::lock_guard lock{ impl::g_completeThreadsMutex };
        impl::g_completeThreads.push_back(std::move(*thisThread));
      }
      impl::g_allTasksCv.notify_one();
    } };

  return Future<T>{ promiseFutureState };
}

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunTaskOnNewCoroutine(std::function<T()>&& /*task*/)
{
  std::shared_ptr<impl::PromiseFutureState<T>> promiseFutureState
    = std::make_shared<impl::PromiseFutureState<T>>();

  // TODO: Implement

  return Future<T>{ promiseFutureState };
}
