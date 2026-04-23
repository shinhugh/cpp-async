#include "async/async.h"
#include "async/future.h"

#include <functional>

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

  return 0;
}
