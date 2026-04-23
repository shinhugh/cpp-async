#pragma once

#include "future.h"

#include <functional>

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

template <typename T>
Future<T> RunTaskOnNewThread(std::function<T()>&&);
template <>
Future<void> RunTaskOnNewThread(std::function<void()>&&);
template <typename T>
Future<T> RunTaskOnNewCoroutine(std::function<T()>&&);
template <>
Future<void> RunTaskOnNewCoroutine(std::function<void()>&&);
int ExecuteProgram(std::function<int()>&&);

// -----------------------------------------------------------------------------

} // async

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunTaskOnNewThread(std::function<T()>&& /*task*/)
{
  // TODO: Implement
  return Future<T>{};
}

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunTaskOnNewCoroutine(std::function<T()>&& /*task*/)
{
  // TODO: Implement
  return Future<T>{};
}
