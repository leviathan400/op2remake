#pragma once
#include <cstdint>

// =============================================================================
// Sim clock  —  the single tick counter that drives ALL time-based game logic
// =============================================================================
//
// One monotonic tick counter. Render frames and wall-clock time are decoupled
// and live OUTSIDE the sim (in platform/render). Nothing in the sim may read a
// real clock — only this tick.
//
// Engine facts (Outpost2.exe):
//   - tick is incremented in FUN_00489EA0, once per advanced sim step.
//   - TethysGame::Time() == tick >> 2.  One "mark" = 4 ticks.
//   - The trigger pass FUN_00403230 begins with `if ((tick & 3) != 3) return;`
//     so mission triggers evaluate ONCE PER MARK — on the last tick of each
//     group of four (ticks 3, 7, 11, ...). Firing triggers every tick would
//     produce 4x the callbacks and subtly break missions.
//
// See Remake/docs/GAME_LOOP.md and RNG_AND_DETERMINISM.md.

namespace op2::sim {

class Clock {
public:
    static constexpr std::uint32_t kTicksPerMark = 4;

    void reset(std::uint32_t tick = 0) noexcept { tick_ = tick; }

    // Advance exactly one sim tick. Returns the new tick value.
    std::uint32_t advance() noexcept { return ++tick_; }

    std::uint32_t tick() const noexcept { return tick_; }      // raw clock
    std::uint32_t time() const noexcept { return tick_ >> 2; } // mark (TethysGame::Time)

    // The per-mark gate. True on ticks 3, 7, 11, ... — when the trigger pass runs.
    bool isMarkBoundary() const noexcept { return isMarkBoundary(tick_); }

    static constexpr bool isMarkBoundary(std::uint32_t tick) noexcept {
        return (tick & (kTicksPerMark - 1)) == (kTicksPerMark - 1);
    }

    // tick is part of the save/replay state — restore it verbatim on load.
    void setTick(std::uint32_t tick) noexcept { tick_ = tick; }

private:
    std::uint32_t tick_ = 0;
};

} // namespace op2::sim
