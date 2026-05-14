#include "promise.h"

#include "promise_future_state.h"

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

// -----------------------------------------------------------------------------

async::Promise<void>::Promise(
    const std::shared_ptr<impl::PromiseFutureState<void>> &state)
    : m_state(state)
{
}

// -----------------------------------------------------------------------------

void async::Promise<void>::Fulfill()
{
  std::vector<std::function<void()>> onFulfillCallbacks;
  {
    std::lock_guard lock{m_state->m_mutex};
    if (m_state->m_fulfilled)
    {
      return;
    }
    m_state->m_fulfilled = true;
    onFulfillCallbacks.swap(m_state->m_onFulfillCallbacks);
  }

  for (std::function<void()> &onFulfillCallback : onFulfillCallbacks)
  {
    onFulfillCallback();
  }
}
