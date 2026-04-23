#pragma once

#include "promise_future_state.h"

#include <memory>

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

template <typename T>
class Future
{
public:
  Future(const std::shared_ptr<impl::PromiseFutureState<T>>&);
  const T& Await();

private:
  const std::shared_ptr<impl::PromiseFutureState<T>> m_state;
};

// -----------------------------------------------------------------------------

template <>
class Future<void>
{
public:
  Future(const std::shared_ptr<impl::PromiseFutureState<void>>&);
  void Await();

private:
  const std::shared_ptr<impl::PromiseFutureState<void>> m_state;
};

// -----------------------------------------------------------------------------

} // async

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T>::Future(
  const std::shared_ptr<impl::PromiseFutureState<T>>& state)
  : m_state(state)
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
