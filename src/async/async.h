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
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

namespace async::impl
{

// -----------------------------------------------------------------------------

extern std::unordered_map<std::thread::id, std::thread> g_activeThreads;
extern std::mutex g_activeThreadsMutex;
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

      T result = task();

      context->m_span.reset();
      impl::DestroyThreadLocalThreadTaskContext();

      promise.Fulfill(std::move(result));

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
