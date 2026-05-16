#include "async.h"

#include <functional>
#include <utility>

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
