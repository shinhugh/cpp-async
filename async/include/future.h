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
  bool taskComplete = false;
  std::mutex taskCompleteMutex;
  std::condition_variable taskCompleteCv;

  {
    std::lock_guard lock{m_state->m_mutex};

    if (m_state->m_fulfilled)
    {
      return *m_state->m_result;
    }

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
  }

  {
    std::unique_lock lock{taskCompleteMutex};
    while (!taskComplete)
    {
      taskCompleteCv.wait(lock);
    }
  }

  return *m_state->m_result;
}
