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

#include "promise.h"
#include "thread_local_task_context.h"

#include "telemetry/living_span.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

// -----------------------------------------------------------------------------

template <typename T>
async::Future<std::vector<T>> async::Future<T>::RequireAll(
  std::vector<Future<T>>& futures)
{
  std::shared_ptr<impl::PromiseFutureState<std::vector<T>>> state
    = std::make_shared<impl::PromiseFutureState<std::vector<T>>>();

  std::shared_ptr<std::unordered_set<size_t>> unfulfilledFutures
    = std::make_shared<std::unordered_set<size_t>>();
  std::shared_ptr<std::unordered_map<size_t, T>> results
    = std::make_shared<std::unordered_map<size_t, T>>();
  std::shared_ptr<std::mutex> mergeMutex = std::make_shared<std::mutex>();

  for (size_t i = 0; i < futures.size(); i++)
  {
    unfulfilledFutures->insert(i);
  }

  for (size_t i = 0; i < futures.size(); i++)
  {
    impl::PromiseFutureState<T>& childFutureState = *futures.at(i).m_state;

    std::unique_lock childFutureLock{ childFutureState.m_mutex };

    if (childFutureState.m_fulfilled)
    {
      T result = *childFutureState.m_result;

      childFutureLock.unlock();

      bool shouldFulfill;
      {
        std::lock_guard mergeLock{ *mergeMutex };
        results->emplace(i, std::move(result));
        unfulfilledFutures->erase(i);
        shouldFulfill = unfulfilledFutures->empty();
      }

      if (shouldFulfill)
      {
        std::vector<T> extractedResults;
        for (size_t j = 0; j < results->size(); j++)
        {
          extractedResults.push_back(std::move(results->at(j)));
        }
        impl::Promise<std::vector<T>>{ state }.Fulfill(
          std::move(extractedResults));
      }

      continue;
    }

    childFutureState.m_onFulfillCallbacks.push_back([
      state,
      unfulfilledFutures,
      results,
      mergeMutex,
      i](
      const T& result)
      {
        bool shouldFulfill;
        {
          std::lock_guard mergeLock{ *mergeMutex };
          results->emplace(i, result);
          unfulfilledFutures->erase(i);
          shouldFulfill = unfulfilledFutures->empty();
        }

        if (shouldFulfill)
        {
          std::vector<T> extractedResults;
          for (size_t j = 0; j < results->size(); j++)
          {
            extractedResults.push_back(std::move(results->at(j)));
          }
          impl::Promise<std::vector<T>>{ state }.Fulfill(
            std::move(extractedResults));
        }
      });
  }

  return Future<std::vector<T>>{ state };
}

// -----------------------------------------------------------------------------

template <typename T>
async::Future<std::pair<size_t, T>> async::Future<T>::RequireOne(
  std::vector<Future<T>>& futures)
{
  std::shared_ptr<impl::PromiseFutureState<std::pair<size_t, T>>> state
    = std::make_shared<impl::PromiseFutureState<std::pair<size_t, T>>>();

  std::shared_ptr<std::atomic<bool>> fulfilled
    = std::make_shared<std::atomic<bool>>(false);

  for (size_t i = 0; i < futures.size(); i++)
  {
    impl::PromiseFutureState<T>& childFutureState = *futures.at(i).m_state;

    std::unique_lock childFutureLock{ childFutureState.m_mutex };

    if (childFutureState.m_fulfilled)
    {
      T result = *childFutureState.m_result;

      childFutureLock.unlock();

      if (!fulfilled->exchange(true))
      {
        impl::Promise<std::pair<size_t, T>>{ state }.Fulfill(
          std::make_pair(i, std::move(result)));
      }

      break;
    }

    childFutureState.m_onFulfillCallbacks.push_back([state, fulfilled, i](
      const T& result)
      {
        if (!fulfilled->exchange(true))
        {
          impl::Promise<std::pair<size_t, T>>{ state }.Fulfill(
            std::make_pair(i, result));
        }
      });
  }

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
