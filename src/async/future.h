#pragma once

#include "promise_future_state.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

template <typename T>
class Future
{
public:
  static Future<std::vector<T>> RequireAll(std::vector<Future>&);
  static Future<std::pair<size_t, T>> RequireOne(std::vector<Future>&);

public:
  Future(const std::shared_ptr<impl::PromiseFutureState<T>>&);
  const T& Await();

private:
  const std::shared_ptr<impl::PromiseFutureState<T>> m_state;
};

// -----------------------------------------------------------------------------

template <>
class Future<void>
{
public:
  static Future RequireAll(std::vector<Future>&);
  static Future<size_t> RequireOne(std::vector<Future>&);

public:
  Future(const std::shared_ptr<impl::PromiseFutureState<void>>&);
  void Await();

private:
  const std::shared_ptr<impl::PromiseFutureState<void>> m_state;
};

// -----------------------------------------------------------------------------

} // async

// -----------------------------------------------------------------------------

#include "thread_local_task_context.h"

#include "telemetry/living_span.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>

// -----------------------------------------------------------------------------

template <typename T>
async::Future<std::vector<T>> async::Future<T>::RequireAll(
  std::vector<Future<T>>& /*futures*/)
{
  std::shared_ptr<impl::PromiseFutureState<std::vector<T>>> state;

  // TODO: Implement

  return Future<std::vector<T>>{ state };
}

// -----------------------------------------------------------------------------

template <typename T>
async::Future<std::pair<size_t, T>> async::Future<T>::RequireOne(
  std::vector<Future<T>>& /*futures*/)
{
  std::shared_ptr<impl::PromiseFutureState<std::pair<size_t, T>>> state;

  // TODO: Implement

  return Future<std::pair<size_t, T>>{ state };
}

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T>::Future(
  const std::shared_ptr<impl::PromiseFutureState<T>>& state)
  : m_state(state)
{
}

// -----------------------------------------------------------------------------

template <typename T>
const T& async::Future<T>::Await()
{
  impl::ThreadLocalCoroutineTaskContext* context
    = impl::GetThreadLocalCoroutineTaskContext();

  std::unique_lock futureLock{ m_state->m_mutex };

  if (m_state->m_fulfilled)
  {
    return *m_state->m_result;
  }

  if (!context)
  {
    bool threadReady = false;
    std::mutex threadReadyMutex;
    std::condition_variable threadReadyCv;

    m_state->m_onFulfillCallbacks.push_back([
      &threadReady,
      &threadReadyMutex,
      &threadReadyCv](const T&)
      {
        {
          std::lock_guard threadReadyLock{ threadReadyMutex };
          threadReady = true;
        }
        threadReadyCv.notify_one();
      });

    futureLock.unlock();

    std::unique_lock threadReadyLock{ threadReadyMutex };
    while (!threadReady)
    {
      threadReadyCv.wait(threadReadyLock);
    }
  }

  else
  {
    m_state->m_onFulfillCallbacks.push_back([
      requeueCallback = std::move(context->m_requeueCallback)](
      const T&)
      {
        requeueCallback();
      });

    futureLock.unlock();

    std::unique_ptr<telemetry::LivingSpan> span{ std::move(context->m_span) };
    context->m_span.reset();
    std::function<void()> yieldCallback = std::move(context->m_yieldCallback);
    context->m_yieldCallback = {};

    yieldCallback();

    context->m_yieldCallback = std::move(yieldCallback);
    context->m_span = std::move(span);
  }

  return *m_state->m_result;
}
