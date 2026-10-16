// Translation-unit anchor for the op2sim static library + compile-time checks.
// The sim classes themselves are header-only (small, hot, inline-friendly).

#include "op2/sim/clock.h"
#include "op2/sim/prng.h"

namespace op2::sim {

const char* version() noexcept { return "op2sim 0.1"; }

// Per-mark gate sanity, verified at compile time: triggers fire on 3,7,11,...
static_assert(!Clock::isMarkBoundary(0), "mark gate");
static_assert(!Clock::isMarkBoundary(1), "mark gate");
static_assert(!Clock::isMarkBoundary(2), "mark gate");
static_assert(Clock::isMarkBoundary(3), "mark gate");
static_assert(!Clock::isMarkBoundary(4), "mark gate");
static_assert(Clock::isMarkBoundary(7), "mark gate");
static_assert(Clock::isMarkBoundary(11), "mark gate");

} // namespace op2::sim
