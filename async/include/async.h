#pragma once

#include "future.h"
#include "promise.h"

#include <functional>

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

template <typename T>
Future<T> RunOnCurrentContext(
    const std::function<void(Promise<T>)> &);

template <>
Future<void> RunOnCurrentContext(
    const std::function<void(Promise<void>)> &);

int RunApplication(
    std::function<int(int, char *[])> &&application, int argc, char *argv[]);

// -----------------------------------------------------------------------------

}  // namespace async

// -----------------------------------------------------------------------------

#include "promise_future_state.h"

#include <memory>

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunOnCurrentContext(
    const std::function<void(Promise<T>)> &task)
{
  std::shared_ptr<impl::PromiseFutureState<T>> promiseFutureState =
      std::make_shared<impl::PromiseFutureState<T>>();

  task(Promise{promiseFutureState});

  return Future{promiseFutureState};
}
