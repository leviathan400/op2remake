#pragma once
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "op2/sim/cmdlog.h"

// =============================================================================
// Replay  —  the headless tick loop + cmdlog replay harness
// =============================================================================
//
// This is the deterministic outer loop, with NO rendering and NO wall clock. It
// drives ticks in order, dispatches each command on its tick, steps the sim via
// a callback, and samples a state checksum at a fixed cadence. The harness does
// not simulate anything itself — the caller plugs the simulation in through the
// hooks (exactly like the Python reference Replay):
//
//   Replay r(cmdlog.packets);
//   r.onCommand  = [&](const CommandPacket& c){ sim.applyCommand(c); };
//   r.onTick     = [&](std::uint32_t tick)     { sim.step(); };
//   r.checksumFn = [&]                          { return sim.checksum(); };
//   auto result = r.run(/*maxTicks*/ 1000, /*checksumEvery*/ 100);
//
// Re-run the same fixture + seed and the checksum history must be identical;
// that is the determinism proof. Compare two histories with Replay::diff() to
// locate the exact tick where two runs diverge.
//
// See Remake/docs/CMDLOG_REPLAY.md.

namespace op2::sim {

struct ReplayResult {
    // (tick, checksum) samples, in tick order.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> checksums;
};

// A single divergence between two checksum histories.
struct Divergence {
    std::uint32_t tick;
    std::uint32_t a; // checksum from history A (0 if absent)
    std::uint32_t b; // checksum from history B (0 if absent)
};

class Replay {
public:
    explicit Replay(std::vector<CommandPacket> packets);

    // Hooks (any may be left unset).
    std::function<void(const CommandPacket&)> onCommand; // per command, in tick order
    std::function<void(std::uint32_t /*tick*/)> onTick;  // once per tick, after commands
    std::function<std::uint32_t()> checksumFn;           // sampled state fingerprint

    // Run ticks [0, maxTicks). Commands whose tick >= maxTicks are not
    // dispatched. A checksum is sampled whenever tick % checksumEvery == 0
    // (checksumEvery == 0 disables sampling).
    ReplayResult run(std::uint32_t maxTicks, std::uint32_t checksumEvery = 1);

    // Compare two checksum histories; returns the ticks that differ.
    static std::vector<Divergence> diff(
        const std::vector<std::pair<std::uint32_t, std::uint32_t>>& a,
        const std::vector<std::pair<std::uint32_t, std::uint32_t>>& b);

private:
    std::vector<CommandPacket> packets_; // sorted by tick (stable)
};

} // namespace op2::sim
