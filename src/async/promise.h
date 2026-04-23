#pragma once

#include "promise_future_state.h"

#include <memory>

// -----------------------------------------------------------------------------

namespace async::impl
{

// -----------------------------------------------------------------------------

template <typename T>
class Promise
{
public:
  Promise(const std::shared_ptr<PromiseFutureState<T>>&);
  void Fulfill(T&&);

private:
  const std::shared_ptr<PromiseFutureState<T>> m_state;
};

// -----------------------------------------------------------------------------

template <>
class Promise<void>
{
public:
  Promise(const std::shared_ptr<PromiseFutureState<void>>&);
  void Fulfill();

private:
  const std::shared_ptr<PromiseFutureState<void>> m_state;
};

// -----------------------------------------------------------------------------

} // async::impl

// -----------------------------------------------------------------------------

template <typename T>
async::impl::Promise<T>::Promise(
  const std::shared_ptr<PromiseFutureState<T>>& state)
  : m_state(state)
{
}

// -----------------------------------------------------------------------------

template <typename T>
void async::impl::Promise<T>::Fulfill(T&& /*result*/)
{
}
