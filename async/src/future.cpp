#include "future.h"

#include "promise_future_state.h"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

// -----------------------------------------------------------------------------

async::Future<void>::Future(
    const std::shared_ptr<impl::PromiseFutureState<void>> &state)
    : m_state(state)
{
}

// -----------------------------------------------------------------------------

void async::Future<void>::Await()
{
  bool taskComplete = false;
  std::mutex taskCompleteMutex;
  std::condition_variable taskCompleteCv;

  {
    std::lock_guard lock{m_state->m_mutex};

    if (m_state->m_fulfilled)
    {
      return;
    }

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
  }

  {
    std::unique_lock lock{taskCompleteMutex};
    while (!taskComplete)
    {
      taskCompleteCv.wait(lock);
    }
  }
}
