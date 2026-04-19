#!/bin/bash

# Must be run from project's root directory

rm -rf build
mkdir -p build

mkdir -p build/main
clang++ src/main/main.cpp -c -o build/main/main.o -Isrc -D PLATFORM_POSIX -g -Wall -Wextra

mkdir -p build/test
clang++ src/test/test.cpp -c -o build/test/test.o -Isrc -D PLATFORM_POSIX -g -Wall -Wextra

clang++          \
  build/main/*.o \
  -o build/main.out

clang++          \
  build/test/*.o \
  -o build/test.out
