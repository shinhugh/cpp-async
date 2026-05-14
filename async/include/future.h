#pragma once

#include "promise_future_state.h"

#include <memory>

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

template <typename T>
class Future
{
public:

  Future(
      const std::shared_ptr<impl::PromiseFutureState<T>> &);

  const T &Await();

private:

  const std::shared_ptr<impl::PromiseFutureState<T>> m_state;
};

// -----------------------------------------------------------------------------

template <>
class Future<void>
{
public:

  Future(
      const std::shared_ptr<impl::PromiseFutureState<void>> &);

  void Await();

private:

  const std::shared_ptr<impl::PromiseFutureState<void>> m_state;
};

// -----------------------------------------------------------------------------

}  // namespace async

// -----------------------------------------------------------------------------

#include "coroutine_context.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T>::Future(
    const std::shared_ptr<impl::PromiseFutureState<T>> &state)
    : m_state(state)
{
}

// -----------------------------------------------------------------------------

template <typename T>
const T &async::Future<T>::Await()
{
  impl::CoroutineContext *context = impl::GetThreadLocalCoroutineContext();

  std::unique_lock futureLock{m_state->m_mutex};

  if (m_state->m_fulfilled)
  {
    return *m_state->m_result;
  }

  if (context)
  {
    m_state->m_onFulfillCallbacks.push_back(
        [
            queueCallback = context->m_queueCallback](
            const T &)
        {
          queueCallback();
        });

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
            &taskComplete, &taskCompleteMutex, &taskCompleteCv](
            const T &)
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

  return *m_state->m_result;
}
