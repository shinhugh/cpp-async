#!/bin/bash

# Must be run from project's root directory

rm -rf build
mkdir -p build

mkdir -p build/async
clang++ src/async/async.cpp -c -o build/async/async.o -Isrc -D PLATFORM_POSIX -g -Wall -Wextra

mkdir -p build/main
clang++ src/main/main.cpp -c -o build/main/main.o -Isrc -D PLATFORM_POSIX -g -Wall -Wextra

mkdir -p build/telemetry
clang++ src/telemetry/living_span.cpp -c -o build/telemetry/living_span.o -Isrc -D PLATFORM_POSIX -g -Wall -Wextra

mkdir -p build/test
clang++ src/test/test.cpp -c -o build/test/test.o -Isrc -D PLATFORM_POSIX -g -Wall -Wextra

clang++               \
  build/async/*.o     \
  build/main/*.o      \
  build/telemetry/*.o \
  -o build/main.out

clang++               \
  build/async/*.o     \
  build/telemetry/*.o \
  build/test/*.o      \
  -o build/test.out
