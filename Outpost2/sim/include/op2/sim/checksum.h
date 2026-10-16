#pragma once
#include <cstddef>
#include <cstdint>

// =============================================================================
// State checksum  —  the sim's determinism fingerprint
// =============================================================================
//
// Purpose: a cheap, deterministic, order-sensitive hash of all sim-relevant
// state. Two uses, both central to Phase 1:
//   1. Lockstep divergence detection — peers compare checksums; a mismatch
//      means their simulations have drifted apart.
//   2. Replay regression — re-running a recorded command stream must reproduce
//      the same per-tick checksum, proving determinism.
//
// Two layers live here:
//   * hashKernel()  — a BIT-EXACT port of the engine kernel FUN_0040C050.
//   * Checksummer   — OUR streaming accumulator (same XOR+rotate step), which
//                     the sim folds its state into.
//
// NOTE on fidelity: this is NOT meant to reproduce the original engine's full
// ChecksumGame VALUE bit-for-bit. That would require mirroring its exact struct
// byte layouts (Player prefix 0x4BC, unit window at +0x10, the 15-DWORD digest,
// volatile-flag masking, the "last trigger wins" quirk, ...), which a clean-room
// clone deliberately does not. We keep the kernel faithful so original-parity
// stays *possible* later, but our own checksum only needs to be internally
// consistent. See Remake/docs/STATE_CHECKSUM.md for the original's full digest.

namespace op2::sim {

// Bit-exact port of FUN_0040C050.
//
// Non-standard CRC variant: XOR each little-endian u32 into the accumulator,
// then right-rotate by 1. Any 0..3 trailing bytes are folded by a prologue that
// runs BEFORE the dword loop. A result of 0 is mapped to 1 (engine convention:
// 0 means "no checksum yet"). Endian-stable: u32s are read little-endian
// regardless of host byte order.
//
// (The original took an unused `prev` first argument; the kernel body ignores
// it — the accumulator always starts at 0 — so we omit it here.)
std::uint32_t hashKernel(std::uint32_t byteCount, const void* data) noexcept;

// Streaming state accumulator. Fold in sim state one field at a time, then read
// value(). Uses the same XOR + rotate-right-1 step as the engine kernel.
class Checksummer {
public:
    void addU32(std::uint32_t v) noexcept {
        const std::uint32_t x = h_ ^ v;
        h_ = (x >> 1) | ((x & 1u) << 31); // right-rotate by 1
    }

    void addI32(std::int32_t v) noexcept { addU32(static_cast<std::uint32_t>(v)); }

    // Folds full little-endian u32s, then any trailing bytes (also little-endian).
    void addBytes(const void* data, std::size_t n) noexcept;

    // Final value, with the engine's zero->1 convention.
    std::uint32_t value() const noexcept { return h_ ? h_ : 1u; }

    void reset() noexcept { h_ = 0; }

private:
    std::uint32_t h_ = 0;
};

// Forward declarations — full definitions in clock.h / prng.h.
class Clock;
class Prng;

// Checksum of the deterministic CORE state (clock tick + PRNG state). This is
// the current whole-sim fingerprint; it grows as subsystems (units, players,
// map, ...) are added.
std::uint32_t checksumCoreState(const Clock& clock, const Prng& prng) noexcept;

} // namespace op2::sim
