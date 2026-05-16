#include "async/async.h"

#include <functional>

// -----------------------------------------------------------------------------

static int Application(
    int argc, char *argv[]);

// -----------------------------------------------------------------------------

int main(
    int argc, char *argv[])
{
  return async::RunApplication(Application, argc, argv);
}

// -----------------------------------------------------------------------------

static int Application(
    int /*argc*/, char * /*argv*/[])
{
  // Entry point for application logic
  return 0;
}
