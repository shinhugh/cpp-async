#!/bin/bash

# Must be run from project's root directory

mkdir -p build/_include/async
cp async/include/* build/_include/async/

mkdir -p build/async
clang++ async/src/async.cpp   -c -o build/async/async.o   -Ibuild/_include -Ibuild/_include/async -D PLATFORM_POSIX -g -Wall -Wextra
clang++ async/src/future.cpp  -c -o build/async/future.o  -Ibuild/_include -Ibuild/_include/async -D PLATFORM_POSIX -g -Wall -Wextra
clang++ async/src/promise.cpp -c -o build/async/promise.o -Ibuild/_include -Ibuild/_include/async -D PLATFORM_POSIX -g -Wall -Wextra
llvm-ar rcs build/libasync.a build/async/*.o

mkdir -p build/main
clang++ main/src/main.cpp -c -o build/main/main.o -Ibuild/_include -Ibuild/_include/main -D PLATFORM_POSIX -g -Wall -Wextra

mkdir -p build/test
clang++ test/src/test.cpp -c -o build/test/test.o -Ibuild/_include -Ibuild/_include/test -D PLATFORM_POSIX -g -Wall -Wextra

mkdir -p build
clang++          \
  build/main/*.o \
  -Lbuild        \
  -lasync        \
  -o build/main.out

mkdir -p build
clang++          \
  build/test/*.o \
  -Lbuild        \
  -lasync        \
  -o build/test.out
