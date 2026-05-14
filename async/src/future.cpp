#include "future.h"

#include "coroutine_context.h"
#include "promise.h"
#include "promise_future_state.h"

#include <atomic>
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
    std::vector<Future<void>> &futures)
{
  std::vector<Future<void> *> ptrs;
  for (Future<void> &future : futures)
  {
    ptrs.push_back(&future);
  }
  return RequireAll(std::move(ptrs));
}

// -----------------------------------------------------------------------------

async::Future<void> async::Future<void>::RequireAll(
    std::vector<Future<void> *> &&futures)
{
  std::shared_ptr<impl::PromiseFutureState<void>> state =
      std::make_shared<impl::PromiseFutureState<void>>();

  std::shared_ptr<std::unordered_set<size_t>> unfulfilledFutures =
      std::make_shared<std::unordered_set<size_t>>();
  std::shared_ptr<std::mutex> mergeMutex = std::make_shared<std::mutex>();

  for (size_t i = 0; i < futures.size(); i++)
  {
    unfulfilledFutures->insert(i);
  }

  for (size_t i = 0; i < futures.size(); i++)
  {
    impl::PromiseFutureState<void> &childFutureState = *futures.at(i)->m_state;

    std::unique_lock childFutureLock{childFutureState.m_mutex};

    if (childFutureState.m_fulfilled)
    {
      childFutureLock.unlock();

      bool shouldFulfill;
      {
        std::lock_guard mergeLock{*mergeMutex};
        unfulfilledFutures->erase(i);
        shouldFulfill = unfulfilledFutures->empty();
      }

      if (shouldFulfill)
      {
        Promise{state}.Fulfill();
      }

      continue;
    }

    childFutureState.m_onFulfillCallbacks.push_back(
        [
            state, unfulfilledFutures, mergeMutex, i]()
        {
          bool shouldFulfill;
          {
            std::lock_guard lock{*mergeMutex};
            unfulfilledFutures->erase(i);
            shouldFulfill = unfulfilledFutures->empty();
          }

          if (shouldFulfill)
          {
            Promise{state}.Fulfill();
          }
        });
  }

  return Future{state};
}

// -----------------------------------------------------------------------------

async::Future<size_t> async::Future<void>::RequireOne(
    std::vector<Future<void>> &futures)
{
  std::vector<Future<void> *> ptrs;
  for (Future<void> &future : futures)
  {
    ptrs.push_back(&future);
  }
  return RequireOne(std::move(ptrs));
}

// -----------------------------------------------------------------------------

async::Future<size_t> async::Future<void>::RequireOne(
    std::vector<Future<void> *> &&futures)
{
  std::shared_ptr<impl::PromiseFutureState<size_t>> state =
      std::make_shared<impl::PromiseFutureState<size_t>>();

  std::shared_ptr<std::atomic<bool>> fulfilled =
      std::make_shared<std::atomic<bool>>(false);

  for (size_t i = 0; i < futures.size(); i++)
  {
    impl::PromiseFutureState<void> &childFutureState = *futures.at(i)->m_state;

    std::unique_lock lock{childFutureState.m_mutex};

    if (childFutureState.m_fulfilled)
    {
      lock.unlock();

      if (!fulfilled->exchange(true))
      {
        Promise{state}.Fulfill(size_t{i});
      }

      break;
    }

    childFutureState.m_onFulfillCallbacks.push_back(
        [
            state, fulfilled, i]()
        {
          if (!fulfilled->exchange(true))
          {
            Promise{state}.Fulfill(size_t{i});
          }
        });
  }

  return Future<size_t>{state};
}

// -----------------------------------------------------------------------------

async::Future<void>::Future(
    const std::shared_ptr<impl::PromiseFutureState<void>> &state)
    : m_state(state)
{
}

// -----------------------------------------------------------------------------

void async::Future<void>::Await()
{
  impl::CoroutineContext *context = impl::GetThreadLocalCoroutineContext();

  std::unique_lock futureLock{m_state->m_mutex};

  if (m_state->m_fulfilled)
  {
    return;
  }

  if (context)
  {
    m_state->m_onFulfillCallbacks.push_back(context->m_queueCallback);

    futureLock.unlock();

    context->m_yieldCallback();
  }

  else
  {
    bool taskComplete = false;
    std::mutex taskCompleteMutex;
    std::condition_variable taskCompleteCv;

    m_state->m_onFulfillCallbacks.push_back(
        [
            &taskComplete, &taskCompleteMutex, &taskCompleteCv]()
        {
          {
            std::lock_guard lock{taskCompleteMutex};
            taskComplete = true;
          }
          taskCompleteCv.notify_one();
        });

    futureLock.unlock();

    std::unique_lock taskCompleteLock{taskCompleteMutex};
    while (!taskComplete)
    {
      taskCompleteCv.wait(taskCompleteLock);
    }
  }
}
