#include "future.h"

#include "promise_future_state.h"

#include <memory>

// -----------------------------------------------------------------------------

async::Future<void>::Future(
  const std::shared_ptr<impl::PromiseFutureState<void>>& state)
  : m_state(state)
{
}

// -----------------------------------------------------------------------------

void async::Future<void>::Await()
{
  // TODO: Implement
}
