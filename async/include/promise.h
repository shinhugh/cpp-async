#pragma once

#include "promise_future_state.h"

#include <memory>

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

template <typename T>
class Promise
{
public:

  Promise(
      const std::shared_ptr<impl::PromiseFutureState<T>> &);

  void Fulfill(
      T &&);

private:

  const std::shared_ptr<impl::PromiseFutureState<T>> m_state;
};

// -----------------------------------------------------------------------------

template <>
class Promise<void>
{
public:

  Promise(
      const std::shared_ptr<impl::PromiseFutureState<void>> &);

  void Fulfill();

private:

  const std::shared_ptr<impl::PromiseFutureState<void>> m_state;
};

// -----------------------------------------------------------------------------

}  // namespace async

// -----------------------------------------------------------------------------

#include <functional>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

template <typename T>
async::Promise<T>::Promise(
    const std::shared_ptr<impl::PromiseFutureState<T>> &state)
    : m_state(state)
{
}

// -----------------------------------------------------------------------------

template <typename T>
void async::Promise<T>::Fulfill(
    T &&result)
{
  std::vector<std::function<void(const T &)>> onFulfillCallbacks;
  {
    std::lock_guard lock{m_state->m_mutex};
    if (m_state->m_fulfilled)
    {
      return;
    }
    m_state->m_result = std::move(result);
    m_state->m_fulfilled = true;
    onFulfillCallbacks.swap(m_state->m_onFulfillCallbacks);
  }

  for (std::function<void(const T &)> &onFulfillCallback : onFulfillCallbacks)
  {
    onFulfillCallback(*m_state->m_result);
  }
}
