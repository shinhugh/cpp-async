#pragma once

#include <functional>

// -----------------------------------------------------------------------------

namespace async::impl
{

// -----------------------------------------------------------------------------

struct CoroutineContext
{
  std::function<void()> m_queueCallback;
  std::function<void()> m_yieldCallback;
};

// -----------------------------------------------------------------------------

CoroutineContext *GetThreadLocalCoroutineContext();

void SetThreadLocalCoroutineContext(
    CoroutineContext *);

void UnsetThreadLocalCoroutineContext();

// -----------------------------------------------------------------------------

}  // namespace async::impl
