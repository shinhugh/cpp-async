#include "async.h"

#include "future.h"

#include <functional>

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunTaskOnNewThread(std::function<void()>&& /*task*/)
{
  // TODO: Implement
  return Future<void>{};
}

// -----------------------------------------------------------------------------

template <>
async::Future<void> async::RunTaskOnNewCoroutine(
  std::function<void()>&& /*task*/)
{
  // TODO: Implement
  return Future<void>{};
}

// -----------------------------------------------------------------------------

int async::ExecuteProgram(std::function<int()>&& /*program*/)
{
  // TODO: Implement
  return 1;
}
