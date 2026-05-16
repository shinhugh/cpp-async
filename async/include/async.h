#pragma once

#include <functional>

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

int RunApplication(
    std::function<int(int, char *[])> &&application, int argc, char *argv[]);

// -----------------------------------------------------------------------------

}  // namespace async
