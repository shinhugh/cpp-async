#include "async/async.h"

#include <functional>

// -----------------------------------------------------------------------------

int Program(int, char*[]);

// -----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
  return async::ExecuteProgram([argc, argv]()
    {
      return Program(argc, argv);
    });
}

// -----------------------------------------------------------------------------

int Program(int, char*[])
{
  // Add top-level logic here
  return 0;
}
