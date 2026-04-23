#pragma once

#include "telemetry/living_span.h"

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

ThreadLocalThreadTaskContext* GetThreadLocalThreadTaskContext();
ThreadLocalThreadTaskContext* CreateThreadLocalThreadTaskContext();
void DestroyThreadLocalThreadTaskContext();

// -----------------------------------------------------------------------------

} // async::impl
