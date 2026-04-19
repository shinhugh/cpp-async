#pragma once

#include <optional>
#include <string>

// -----------------------------------------------------------------------------

namespace telemetry
{

// -----------------------------------------------------------------------------

struct Span
{
  std::string m_traceId;
  std::string m_spanId;
  std::optional<std::string> m_parentSpanId;
};

// -----------------------------------------------------------------------------

} // telemetry
