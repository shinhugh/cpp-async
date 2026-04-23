#pragma once

#include "future.h"

#include "telemetry/span.h"

#include <functional>
#include <optional>

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

std::optional<telemetry::Span> GetActiveSpan();
template <typename T>
Future<T> RunTaskOnNewThread(std::function<T()>&&);
template <>
Future<void> RunTaskOnNewThread(std::function<void()>&&);
template <typename T>
Future<T> RunTaskOnNewCoroutine(std::function<T()>&&);
template <>
Future<void> RunTaskOnNewCoroutine(std::function<void()>&&);
int ExecuteProgram(std::function<int()>&&);

// -----------------------------------------------------------------------------

} // async

// -----------------------------------------------------------------------------

#include "promise_future_state.h"

#include <memory>

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunTaskOnNewThread(std::function<T()>&& /*task*/)
{
  std::shared_ptr<impl::PromiseFutureState<T>> promiseFutureState
    = std::make_shared<impl::PromiseFutureState<T>>();

  // TODO: Implement

  return Future<T>{ promiseFutureState };
}

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunTaskOnNewCoroutine(std::function<T()>&& /*task*/)
{
  std::shared_ptr<impl::PromiseFutureState<T>> promiseFutureState
    = std::make_shared<impl::PromiseFutureState<T>>();

  // TODO: Implement

  return Future<T>{ promiseFutureState };
}
