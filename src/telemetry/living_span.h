#pragma once

#include "span.h"

#include <chrono>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------

namespace telemetry
{

// -----------------------------------------------------------------------------

class LivingSpan : public Span
{
public:
  static LivingSpan Create();
  static LivingSpan Create(const std::vector<std::string>& tags);
  static LivingSpan Create(const Span& parentSpan);
  static LivingSpan Create(
    const Span& parentSpan,
    const std::vector<std::string>& tags);

public:
  LivingSpan(const LivingSpan&) = delete;
  LivingSpan(LivingSpan&&);
  ~LivingSpan();
  LivingSpan& operator=(const LivingSpan&) = delete;
  LivingSpan& operator=(LivingSpan&&);

private:
  LivingSpan();

private:
  bool m_alive = false;
  std::chrono::steady_clock::time_point m_startTime;
};

// -----------------------------------------------------------------------------

} // telemetry
