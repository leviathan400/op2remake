#pragma once
#include <cstdint>
#include <vector>

#include "op2/sim/clock.h"
#include "op2/sim/cmdlog.h"
#include "op2/sim/map.h"
#include "op2/sim/prng.h"
#include "op2/sim/world.h"

// =============================================================================
// Game  —  the whole deterministic simulation in one object
// =============================================================================
//
// Clock + gameplay RNG + World + map: everything a tick reads or writes. The
// headless tools (op2inspect) and, later, the app's game loop drive it; nothing
// else ticks the sim.
//
// One tick (`step`): the command packets queued for it are applied, then every
// object runs in processing order (World::processUnits), then the clock
// advances. In OP2 each cycle runs the colony updates, then the command
// packets, then ProcessUnits; an order given at tick T shows in T's state and
// the unit acts on it in cycle T+1. A unit's turn is its movement
// (movement.h); combat and the economy plug in beside it as they arrive.
//
// Save/restore covers the clock, the RNG, the queued packets and the World.
// The map is not saved: it is input (reloaded from the scenario), and the sim
// doesn't change terrain yet.

namespace op2::sim {

class Game {
public:
    explicit Game(std::uint32_t seed = 1) : prng_(seed) {}

    // Seed the gameplay RNG (SetSeed). For setting up a game before its first tick.
    void setSeed(std::uint32_t seed) { prng_.setSeed(seed); }
    // Start a new game on the loaded World and map: tick 0, the RNG seeded, no
    // queued packets. The World and map are left to the caller (mission setup).
    void restart(std::uint32_t seed) {
        clock_.reset();
        prng_.setSeed(seed);
        queued_.clear();
    }

    // Queue a command packet; it is applied at the start of the next step.
    void queue(CommandPacket p) { queued_.push_back(std::move(p)); }
    const std::vector<CommandPacket>& queued() const { return queued_; }

    // Run one tick. Returns the packets the command layer rejected (unknown
    // opcode, malformed payload, stale handle).
    std::vector<CommandPacket> step();

    std::uint32_t tick() const { return clock_.tick(); }
    const Clock& clock() const { return clock_; }
    const Prng& prng() const { return prng_; }
    World& world() { return world_; }
    const World& world() const { return world_; }
    Map& map() { return map_; }
    const Map& map() const { return map_; }

    // Whole-sim fingerprint (checksumSimState).
    std::uint32_t checksum() const;

    std::vector<std::uint8_t> serialize() const;
    // Restores the clock, RNG, queue and World; leaves the map and the World's
    // unit types as they are (both are rebuilt from the scenario's inputs).
    bool deserialize(const std::vector<std::uint8_t>& bytes);

private:
    Clock clock_;
    Prng prng_;
    World world_;
    Map map_;
    std::vector<CommandPacket> queued_;
};

} // namespace op2::sim
