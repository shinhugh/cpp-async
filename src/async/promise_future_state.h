#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <vector>

// -----------------------------------------------------------------------------

namespace async::impl
{

// -----------------------------------------------------------------------------

template <typename T>
struct PromiseFutureState
{
  std::mutex m_mutex;
  bool m_fulfilled = false;
  std::optional<T> m_result;
  std::vector<std::function<void(const T&)>> m_onFulfillCallbacks;
};

// -----------------------------------------------------------------------------

template <>
struct PromiseFutureState<void>
{
  std::mutex m_mutex;
  bool m_fulfilled = false;
  std::vector<std::function<void()>> m_onFulfillCallbacks;
};

// -----------------------------------------------------------------------------

} // async::impl
