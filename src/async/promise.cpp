#include "promise.h"

#include "promise_future_state.h"

#include <memory>

// -----------------------------------------------------------------------------

async::impl::Promise<void>::Promise(
  const std::shared_ptr<PromiseFutureState<void>>& state)
  : m_state(state)
{
}

// -----------------------------------------------------------------------------

void async::impl::Promise<void>::Fulfill()
{
}
