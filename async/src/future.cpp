#include "future.h"

#include "coroutine_context.h"
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
