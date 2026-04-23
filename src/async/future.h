#pragma once

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

template <typename T>
class Future
{
public:
  Future();
  const T& Await();
};

// -----------------------------------------------------------------------------

template <>
class Future<void>
{
public:
  Future();
  void Await();
};

// -----------------------------------------------------------------------------

} // async

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T>::Future()
{
}

// -----------------------------------------------------------------------------

template <typename T>
const T& async::Future<T>::Await()
{
  // TODO: Implement
  void* x = nullptr;
  return *static_cast<T*>(x);
}
