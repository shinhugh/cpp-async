#!/bin/bash

# Must be run from project's root directory

mkdir -p build/_include/async
cp async/include/* build/_include/async/

include-what-you-use -Xiwyu --no_fwd_decls -Xiwyu --quoted_includes_first -Ibuild/_include -Ibuild/_include/async -D PLATFORM_POSIX async/src/async.cpp
include-what-you-use -Xiwyu --no_fwd_decls -Xiwyu --quoted_includes_first -Ibuild/_include -Ibuild/_include/async -D PLATFORM_POSIX async/src/coroutine_context.cpp
include-what-you-use -Xiwyu --no_fwd_decls -Xiwyu --quoted_includes_first -Ibuild/_include -Ibuild/_include/async -D PLATFORM_POSIX async/src/future.cpp
include-what-you-use -Xiwyu --no_fwd_decls -Xiwyu --quoted_includes_first -Ibuild/_include -Ibuild/_include/async -D PLATFORM_POSIX async/src/promise.cpp

include-what-you-use -Xiwyu --no_fwd_decls -Xiwyu --quoted_includes_first -Ibuild/_include -Ibuild/_include/main -D PLATFORM_POSIX main/src/main.cpp
