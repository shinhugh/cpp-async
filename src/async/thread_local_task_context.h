#pragma once

#include "telemetry/living_span.h"

#include <functional>
#include <memory>

// -----------------------------------------------------------------------------

namespace async::impl
{

// -----------------------------------------------------------------------------

struct ThreadLocalThreadTaskContext
{
  std::unique_ptr<telemetry::LivingSpan> m_span;
};

// -----------------------------------------------------------------------------

struct ThreadLocalCoroutineTaskContext
{
  std::function<void()> m_requeueCallback;
  std::function<void()> m_yieldCallback;
  std::unique_ptr<telemetry::LivingSpan> m_span;
};

// -----------------------------------------------------------------------------

ThreadLocalThreadTaskContext* GetThreadLocalThreadTaskContext();
ThreadLocalThreadTaskContext* CreateThreadLocalThreadTaskContext();
void DestroyThreadLocalThreadTaskContext();
ThreadLocalCoroutineTaskContext* GetThreadLocalCoroutineTaskContext();
ThreadLocalCoroutineTaskContext* CreateThreadLocalCoroutineTaskContext();
void DestroyThreadLocalCoroutineTaskContext();

// -----------------------------------------------------------------------------

} // async::impl
