#include "async.h"

#include "future.h"
#include "promise.h"
#include "promise_future_state.h"

#include <functional>
#include <memory>
#include <utility>

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunOnCurrentContext(
    const std::function<void(Promise<void>)> &task)
{
  std::shared_ptr<impl::PromiseFutureState<void>> promiseFutureState =
      std::make_shared<impl::PromiseFutureState<void>>();

  task(Promise{promiseFutureState});

  return Future{promiseFutureState};
}

// -----------------------------------------------------------------------------

int async::RunApplication(
    std::function<int(int, char *[])> &&application, int argc, char *argv[])
{
  std::function<int()> task = [
      application = std::move(application), argc, argv]()
  {
    return application(argc, argv);
  };
  return task();
}
