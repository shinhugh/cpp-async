#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <type_traits>
#include <vector>

// -----------------------------------------------------------------------------

namespace async::impl
{

// -----------------------------------------------------------------------------

template <
    typename T,
    typename std::enable_if<!std::is_reference<T>::value, bool>::type = true>
struct PromiseFutureState
{
  std::mutex m_mutex;
  bool m_fulfilled = false;
  std::optional<T> m_result;
  std::vector<std::function<void(const T &)>> m_onFulfillCallbacks;
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

}  // namespace async::impl
