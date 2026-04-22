#include "async/async.h"
#include "async/future.h"

#include <functional>
#include <vector>

// -----------------------------------------------------------------------------

int Program();

// -----------------------------------------------------------------------------

int main()
{
  return async::ExecuteProgram(Program);
}

// -----------------------------------------------------------------------------

int Program()
{
  std::vector<async::Future<void>> voidFutures;
  std::vector<async::Future<int>> intFutures;

  async::RunTaskOnNewThread<void>([]()
    {
      async::RunTaskOnNewThread<void>([]() {}).Await();
      async::RunTaskOnNewCoroutine<void>([]() {}).Await();
    }).Await();

  async::RunTaskOnNewThread<int>([]()
    {
      async::RunTaskOnNewThread<int>([]() { return 1; }).Await();
      async::RunTaskOnNewCoroutine<int>([]() { return 2; }).Await();
      return 3;
    }).Await();

  async::RunTaskOnNewCoroutine<void>([]()
    {
      async::RunTaskOnNewThread<void>([]() {}).Await();
      async::RunTaskOnNewCoroutine<void>([]() {}).Await();
    }).Await();

  async::RunTaskOnNewCoroutine<int>([]()
    {
      async::RunTaskOnNewThread<int>([]() { return 1; }).Await();
      async::RunTaskOnNewCoroutine<int>([]() { return 2; }).Await();
      return 3;
    }).Await();

  voidFutures.push_back(async::RunTaskOnNewThread<void>([]() {}));
  voidFutures.push_back(async::RunTaskOnNewCoroutine<void>([]() {}));
  async::Future<void>::RequireAll(voidFutures).Await();
  voidFutures.clear();

  intFutures.push_back(async::RunTaskOnNewThread<int>([]() { return 1; }));
  intFutures.push_back(async::RunTaskOnNewCoroutine<int>([]() { return 2; }));
  async::Future<int>::RequireAll(intFutures).Await();
  intFutures.clear();

  voidFutures.push_back(async::RunTaskOnNewThread<void>([]() {}));
  voidFutures.push_back(async::RunTaskOnNewCoroutine<void>([]() {}));
  async::Future<void>::RequireOne(voidFutures).Await();
  voidFutures.clear();

  intFutures.push_back(async::RunTaskOnNewThread<int>([]() { return 1; }));
  intFutures.push_back(async::RunTaskOnNewCoroutine<int>([]() { return 2; }));
  async::Future<int>::RequireOne(intFutures).Await();
  intFutures.clear();

  return 0;
}
