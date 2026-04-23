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

#include <functional>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

template <typename T>
async::impl::Promise<T>::Promise(
  const std::shared_ptr<PromiseFutureState<T>>& state)
  : m_state(state)
{
}

// -----------------------------------------------------------------------------

template <typename T>
void async::impl::Promise<T>::Fulfill(T&& result)
{
  T resultTemp = std::move(result);

  std::vector<std::function<void(const T&)>> onFulfillCallbacks;
  {
    std::lock_guard lock{ m_state->m_mutex };
    m_state->m_result = resultTemp;
    m_state->m_fulfilled = true;
    onFulfillCallbacks.swap(m_state->m_onFulfillCallbacks);
  }

  for (std::function<void(const T&)>& onFulfillCallback : onFulfillCallbacks)
  {
    onFulfillCallback(resultTemp);
  }
}
