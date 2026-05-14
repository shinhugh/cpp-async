#!/bin/bash

# Must be run from project's root directory

include-what-you-use -Xiwyu --no_fwd_decls -Xiwyu --quoted_includes_first -Ibuild/_include -Ibuild/_include/main -D PLATFORM_POSIX main/src/main.cpp
