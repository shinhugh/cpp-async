#include "async.h"

#include "future.h"
#include "promise_future_state.h"

#include "telemetry/span.h"

#include <functional>
#include <memory>
#include <optional>

// -----------------------------------------------------------------------------

std::optional<telemetry::Span> async::GetActiveSpan()
{
  // TODO: Implement
  return std::nullopt;
}

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunTaskOnNewThread(std::function<void()>&& /*task*/)
{
  std::shared_ptr<impl::PromiseFutureState<void>> promiseFutureState
    = std::make_shared<impl::PromiseFutureState<void>>();

  // TODO: Implement

  return Future<void>{ promiseFutureState };
}

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunTaskOnNewCoroutine(
  std::function<void()>&& /*task*/)
{
  std::shared_ptr<impl::PromiseFutureState<void>> promiseFutureState
    = std::make_shared<impl::PromiseFutureState<void>>();

  // TODO: Implement

  return Future<void>{ promiseFutureState };
}

// -----------------------------------------------------------------------------

int async::ExecuteProgram(std::function<int()>&& /*program*/)
{
  // TODO: Implement
  return 1;
}
