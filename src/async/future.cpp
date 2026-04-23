#include "future.h"

#include "promise_future_state.h"
#include "promise.h"
#include "thread_local_task_context.h"

#include "telemetry/living_span.h"

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

async::Future<void> async::Future<void>::RequireAll(
  std::vector<Future<void>>& futures)
{
  std::shared_ptr<impl::PromiseFutureState<void>> state
    = std::make_shared<impl::PromiseFutureState<void>>();

  std::shared_ptr<std::unordered_set<size_t>> unfulfilledFutures
    = std::make_shared<std::unordered_set<size_t>>();
  std::shared_ptr<std::mutex> mergeMutex = std::make_shared<std::mutex>();

  for (size_t i = 0; i < futures.size(); i++)
  {
    unfulfilledFutures->insert(i);
  }

  for (size_t i = 0; i < futures.size(); i++)
  {
    impl::PromiseFutureState<void>& childFutureState = *futures.at(i).m_state;

    std::unique_lock childFutureLock{ childFutureState.m_mutex };

    if (childFutureState.m_fulfilled)
    {
      childFutureLock.unlock();

      bool shouldFulfill;
      {
        std::lock_guard mergeLock{ *mergeMutex };
        unfulfilledFutures->erase(i);
        shouldFulfill = unfulfilledFutures->empty();
      }

      if (shouldFulfill)
      {
        impl::Promise<void>{ state }.Fulfill();
      }

      continue;
    }

    childFutureState.m_onFulfillCallbacks.push_back([
      state,
      unfulfilledFutures,
      mergeMutex,
      i]()
      {
        bool shouldFulfill;
        {
          std::lock_guard mergeLock{ *mergeMutex };
          unfulfilledFutures->erase(i);
          shouldFulfill = unfulfilledFutures->empty();
        }

        if (shouldFulfill)
        {
          impl::Promise<void>{ state }.Fulfill();
        }
      });
  }

  return Future{ state };
}

// -----------------------------------------------------------------------------

async::Future<size_t> async::Future<void>::RequireOne(
  std::vector<Future<void>>& /*futures*/)
{
  std::shared_ptr<impl::PromiseFutureState<size_t>> state;

  // TODO: Implement

  return Future<size_t>{ state };
}

// -----------------------------------------------------------------------------

async::Future<void>::Future(
  const std::shared_ptr<impl::PromiseFutureState<void>>& state)
  : m_state(state)
{
}

// -----------------------------------------------------------------------------

void async::Future<void>::Await()
{
  impl::ThreadLocalCoroutineTaskContext* context
    = impl::GetThreadLocalCoroutineTaskContext();

  std::unique_lock futureLock{ m_state->m_mutex };

  if (m_state->m_fulfilled)
  {
    return;
  }

  if (!context)
  {
    bool threadReady = false;
    std::mutex threadReadyMutex;
    std::condition_variable threadReadyCv;

    m_state->m_onFulfillCallbacks.push_back([
      &threadReady,
      &threadReadyMutex,
      &threadReadyCv]()
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
    m_state->m_onFulfillCallbacks.push_back(
      std::move(context->m_requeueCallback));

    futureLock.unlock();

    std::unique_ptr<telemetry::LivingSpan> span{ std::move(context->m_span) };
    context->m_span.reset();
    std::function<void()> yieldCallback = std::move(context->m_yieldCallback);
    context->m_yieldCallback = {};

    yieldCallback();

    context->m_yieldCallback = std::move(yieldCallback);
    context->m_span = std::move(span);
  }
}
