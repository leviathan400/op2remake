#pragma once
#include <cstdint>

// =============================================================================
// Outpost 2 PRNG  —  bit-exact port of g_RandomNumberObject @ 0x0056BE20
// =============================================================================
//
// Dual 32-bit LFSR (Tausworthe-style). Feedback taps at bit positions 13 and 16
// (mask 0x12000). state[0] left-shifts, state[1] right-shifts; the output word
// is state[0] ^ state[1]. GetRand maps to [0, n) with Lemire's bias-free
// (rand * n) >> 32 — no modulo bias.
//
// Source-of-truth bodies in Outpost2.exe:
//   FUN_0046EFB0  SetSeed
//   FUN_0046F000  Rand     (raw 32-bit, no range)
//   FUN_0046F060  GetRand  (step + Lemire range map)
//
// This MUST stay bit-identical forever: it is the spine of lockstep multiplayer
// and replay determinism. Do NOT substitute (rand % n) for the Lemire map — it
// produces different results for small n and silently breaks compatibility.
//
// See Remake/docs/RNG_AND_DETERMINISM.md.

namespace op2::sim {

class Prng {
public:
    // 8-byte state snapshot — the whole RNG. Used for save/load and net resync.
    struct State {
        std::uint32_t s0;
        std::uint32_t s1;
    };

    explicit Prng(std::uint32_t seed = 1) noexcept { setSeed(seed); }

    // FUN_0046EFB0. Splits the 32-bit seed across the two LFSRs (high half ->
    // state0, low half -> state1) so they start out of phase, and substitutes a
    // non-zero constant for seed 0 to avoid the LFSR's one forbidden state.
    void setSeed(std::uint32_t seed) noexcept {
        if (seed == 0) {
            seed = 0x10000001u; // all-zero state is the LFSR death state
        }
        state0_ = (seed >> 16) | (seed & 0xFFFF0000u);
        state1_ = (seed << 16) | (seed & 0x0000FFFFu);
    }

    // FUN_0046F000 — raw unbounded 32-bit value.
    std::uint32_t rand32() noexcept { return step(); }

    // FUN_0046F060 — uniform value in [0, upperBound).
    //
    // NOTE: the engine ALWAYS advances the state, even when upperBound == 0 (the
    // multiply just yields 0). We replicate that so the RNG stream stays
    // bit-identical regardless of the bound. (This differs from the Python
    // reference op2_prng.py, which early-returns without stepping when n == 0.)
    std::uint32_t getRand(std::uint32_t upperBound) noexcept {
        const std::uint64_t r = step();
        return static_cast<std::uint32_t>((r * upperBound) >> 32);
    }

    State state() const noexcept { return State{state0_, state1_}; }
    void setState(State st) noexcept { state0_ = st.s0; state1_ = st.s1; }

private:
    // One PRNG step. Advances both LFSRs and returns the combined output word.
    std::uint32_t step() noexcept {
        std::uint32_t s0 = state0_ << 1;
        if (parity((state0_ & 0x12000u) >> 12)) {
            s0 |= 0x00000001u; // feedback into LSB
        }
        std::uint32_t s1 = state1_ >> 1;
        if (parity((state1_ & 0x12000u) >> 12)) {
            s1 |= 0x80000000u; // feedback into MSB
        }
        state0_ = s0;
        state1_ = s1;
        return s0 ^ s1;
    }

    // Parity (XOR of all bits) — portable replacement for the engine's
    // popcount(...) & 1. Deterministic on every compiler/platform.
    static std::uint32_t parity(std::uint32_t v) noexcept {
        v ^= v >> 16;
        v ^= v >> 8;
        v ^= v >> 4;
        v ^= v >> 2;
        v ^= v >> 1;
        return v & 1u;
    }

    std::uint32_t state0_ = 0;
    std::uint32_t state1_ = 0;
};

} // namespace op2::sim
