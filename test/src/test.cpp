#include "async/async.h"
#include "async/future.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <iostream>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

static int Application(
    int argc, char *argv[]);

static void TestAsync();

// -----------------------------------------------------------------------------

int main(
    int argc, char *argv[])
{
  return async::RunApplication(Application, argc, argv, 4);
}

// -----------------------------------------------------------------------------

static int Application(
    int, char *[])
{
  TestAsync();

  return 0;
}

// -----------------------------------------------------------------------------

static void TestAsync()
{
  std::cout << "coroutine:     start" << std::endl;

  // ---------------------------------------------------------------------------

  bool addComma;
  std::vector<async::Future<void>> voidFutures;
  std::vector<async::Future<int>> intFutures;
  int result;
  std::vector<int> results;
  size_t index;
  std::pair<size_t, int> indexAndResult;

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  result = async::RunOnCurrentContext<int>(
      [](
          async::Promise<int> promise)
      {
        std::cout << "coroutine:     scope: start" << std::endl;
        async::YieldFor(std::chrono::milliseconds(50));
        promise.Fulfill(0);
        std::cout << "coroutine:     scope: finish" << std::endl;
      }).Await();

  std::cout << "coroutine:     result: " << result << std::endl;

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  async::RunOnCurrentContext<void>(
      [](
          async::Promise<void> promise)
      {
        std::cout << "coroutine:     scope: start" << std::endl;
        async::YieldFor(std::chrono::milliseconds(50));
        promise.Fulfill();
        std::cout << "coroutine:     scope: finish" << std::endl;
      })
      .Await();

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  result = async::RunOnNewCoroutine<int>(
      [](
          async::Promise<int> promise)
      {
        std::cout << "  coroutine:   start" << std::endl;

        int result;

        async::YieldFor(std::chrono::milliseconds(50));

        result = async::RunOnNewCoroutine<int>(
            []()
            {
              std::cout << "    coroutine: start" << std::endl;
              async::YieldFor(std::chrono::milliseconds(50));
              std::cout << "    coroutine: finish" << std::endl;
              return 1;
            }).Await();

        std::cout << "  coroutine:   result: " << result << std::endl;

        async::YieldFor(std::chrono::milliseconds(50));

        result = async::RunOnNewThread<int>(
            []()
            {
              std::cout << "    thread:    start" << std::endl;
              async::YieldFor(std::chrono::milliseconds(50));
              std::cout << "    thread:    finish" << std::endl;
              return 2;
            }).Await();

        std::cout << "  coroutine:   result: " << result << std::endl;

        async::YieldFor(std::chrono::milliseconds(50));

        promise.Fulfill(0);
        std::cout << "  coroutine:   finish" << std::endl;
      }).Await();

  std::cout << "coroutine:     result: " << result << std::endl;

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  async::RunOnNewCoroutine<void>(
      [](
          async::Promise<void> promise)
      {
        std::cout << "  coroutine:   start" << std::endl;

        async::YieldFor(std::chrono::milliseconds(50));

        async::RunOnNewCoroutine<void>(
            []()
            {
              std::cout << "    coroutine: start" << std::endl;
              async::YieldFor(std::chrono::milliseconds(50));
              std::cout << "    coroutine: finish" << std::endl;
            })
            .Await();

        async::YieldFor(std::chrono::milliseconds(50));

        async::RunOnNewThread<void>(
            []()
            {
              std::cout << "    thread:    start" << std::endl;
              async::YieldFor(std::chrono::milliseconds(50));
              std::cout << "    thread:    finish" << std::endl;
            })
            .Await();

        async::YieldFor(std::chrono::milliseconds(50));

        promise.Fulfill();
        std::cout << "  coroutine:   finish" << std::endl;
      })
      .Await();

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  result = async::RunOnNewThread<int>(
      [](
          async::Promise<int> promise)
      {
        std::cout << "  coroutine:   start" << std::endl;

        int result;

        async::YieldFor(std::chrono::milliseconds(50));

        result = async::RunOnNewCoroutine<int>(
            []()
            {
              std::cout << "    coroutine: start" << std::endl;
              async::YieldFor(std::chrono::milliseconds(50));
              std::cout << "    coroutine: finish" << std::endl;
              return 1;
            }).Await();

        std::cout << "  coroutine:   result: " << result << std::endl;

        async::YieldFor(std::chrono::milliseconds(50));

        result = async::RunOnNewThread<int>(
            []()
            {
              std::cout << "    thread:    start" << std::endl;
              async::YieldFor(std::chrono::milliseconds(50));
              std::cout << "    thread:    finish" << std::endl;
              return 2;
            }).Await();

        std::cout << "  coroutine:   result: " << result << std::endl;

        async::YieldFor(std::chrono::milliseconds(50));

        promise.Fulfill(0);
        std::cout << "  coroutine:   finish" << std::endl;
      }).Await();

  std::cout << "coroutine:     result: " << result << std::endl;

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  async::RunOnNewThread<void>(
      [](
          async::Promise<void> promise)
      {
        std::cout << "  coroutine:   start" << std::endl;

        async::YieldFor(std::chrono::milliseconds(50));

        async::RunOnNewCoroutine<void>(
            []()
            {
              std::cout << "    coroutine: start" << std::endl;
              async::YieldFor(std::chrono::milliseconds(50));
              std::cout << "    coroutine: finish" << std::endl;
            })
            .Await();

        async::YieldFor(std::chrono::milliseconds(50));

        async::RunOnNewThread<void>(
            []()
            {
              std::cout << "    thread:    start" << std::endl;
              async::YieldFor(std::chrono::milliseconds(50));
              std::cout << "    thread:    finish" << std::endl;
            })
            .Await();

        async::YieldFor(std::chrono::milliseconds(50));

        promise.Fulfill();
        std::cout << "  coroutine:   finish" << std::endl;
      })
      .Await();

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  intFutures.push_back(
      async::RunOnNewCoroutine<int>(
          []()
          {
            std::cout << "  coroutine:   start" << std::endl;
            async::YieldFor(std::chrono::milliseconds(50));
            std::cout << "  coroutine:   finish" << std::endl;
            return 1;
          }));

  async::YieldFor(std::chrono::milliseconds(20));

  intFutures.push_back(
      async::RunOnNewThread<int>(
          []()
          {
            std::cout << "  thread:      start" << std::endl;
            async::YieldFor(std::chrono::milliseconds(50));
            std::cout << "  thread:      finish" << std::endl;
            return 2;
          }));

  results = async::Future<int>::RequireAll(intFutures).Await();
  intFutures.clear();
  std::cout << "coroutine:     result:";
  addComma = false;
  for (int r : results)
  {
    if (addComma)
    {
      std::cout << ',';
    }
    std::cout << ' ' << r;
    addComma = true;
  }
  std::cout << std::endl;

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  voidFutures.push_back(
      async::RunOnNewCoroutine<void>(
          []()
          {
            std::cout << "  coroutine:   start" << std::endl;
            async::YieldFor(std::chrono::milliseconds(50));
            std::cout << "  coroutine:   finish" << std::endl;
          }));

  async::YieldFor(std::chrono::milliseconds(20));

  voidFutures.push_back(
      async::RunOnNewThread<void>(
          []()
          {
            std::cout << "  thread:      start" << std::endl;
            async::YieldFor(std::chrono::milliseconds(50));
            std::cout << "  thread:      finish" << std::endl;
          }));

  async::Future<void>::RequireAll(voidFutures).Await();
  voidFutures.clear();

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  intFutures.push_back(
      async::RunOnNewCoroutine<int>(
          []()
          {
            std::cout << "  coroutine:   start" << std::endl;
            async::YieldFor(std::chrono::milliseconds(50));
            std::cout << "  coroutine:   finish" << std::endl;
            return 1;
          }));

  async::YieldFor(std::chrono::milliseconds(20));

  intFutures.push_back(
      async::RunOnNewThread<int>(
          []()
          {
            std::cout << "  thread:      start" << std::endl;
            async::YieldFor(std::chrono::milliseconds(50));
            std::cout << "  thread:      finish" << std::endl;
            return 2;
          }));

  indexAndResult = async::Future<int>::RequireOne(intFutures).Await();
  intFutures.clear();
  std::cout << "coroutine:     result: " << indexAndResult.first << ", "
            << indexAndResult.second << std::endl;

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  voidFutures.push_back(
      async::RunOnNewCoroutine<void>(
          []()
          {
            std::cout << "  coroutine:   start" << std::endl;
            async::YieldFor(std::chrono::milliseconds(50));
            std::cout << "  coroutine:   finish" << std::endl;
          }));

  async::YieldFor(std::chrono::milliseconds(20));

  voidFutures.push_back(
      async::RunOnNewThread<void>(
          []()
          {
            std::cout << "  thread:      start" << std::endl;
            async::YieldFor(std::chrono::milliseconds(50));
            std::cout << "  thread:      finish" << std::endl;
          }));

  index = async::Future<void>::RequireOne(voidFutures).Await();
  voidFutures.clear();
  std::cout << "coroutine:     result: " << index << std::endl;

  // ---------------------------------------------------------------------------

  async::YieldUntil(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(50));

  // ---------------------------------------------------------------------------

  std::cout << std::endl;

  std::cout << "coroutine:     finish" << std::endl;
}
