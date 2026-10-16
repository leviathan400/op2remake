#pragma once
#include <cstdint>

#include "op2/sim/cmdlog.h"
#include "op2/sim/world.h"

// =============================================================================
// Command dispatch  —  the ONLY path that mutates the World from outside
// =============================================================================
//
// OP2 is lockstep deterministic: the UI never pokes game state directly. Every
// player action becomes a CommandPacket (cmdlog.h) that is queued, exchanged
// with every peer, and applied by the sim at an agreed tick — identically on
// every machine (the original drains packets in FUN_0040E300; see
// Remake/docs/GAME_LOOP.md). This module is the APPLY side of that contract:
//
//   UI / mission script ──makeXxxCommand()──▶ CommandPacket
//                                                 │   (later: cmdlog record,
//                                                 │    network send)
//                                  applyCommand(world, pkt)
//                                                 ▼
//                                          World mutation
//
// The render/UI layer builds packets with the helpers below and hands them
// over; it must never write World state itself. That one-way rule is what
// makes multiplayer and cmdlog replay possible: a recorded packet stream
// replays bit-identically because packets are the complete set of inputs.
//
// Implemented opcodes so far: ctMoIdle / ctMoUnidle (take a structure off/
// online — the detail panel's IDLE button and the debug click-toggle). The
// rest (move/build/attack/…) arrive with the gameplay sim.

namespace op2::sim {

// CommandPacket opcodes (messageType). The values are OP2's own — the indices
// into cmdlog.cpp's name table (from InternalData/DataStructure
// CommandPacket.txt). Only the ones the remake dispatches are named here.
namespace Opcode {
    constexpr std::uint32_t ctMoIdle   = 18; // take a structure offline (player intent)
    constexpr std::uint32_t ctMoUnidle = 19; // bring it back online
}

// --- packet builders (UI side) -----------------------------------------------
// Payload layout for unit-targeted commands: the target's UnitId as two
// little-endian u32s (index, then generation). OP2 packs bare unit indexes;
// we carry the generation too so a command aimed at a destroyed-and-reused
// slot is detectably stale instead of silently hitting the wrong unit.

// Build a ctMoIdle (idle=true) or ctMoUnidle (idle=false) packet.
CommandPacket makeIdleCommand(std::uint32_t tick, int player, UnitId target,
                              bool idle);

// Decode a unit-targeted payload back into a handle. Returns false (and leaves
// *out untouched) if the payload is malformed/too short.
bool decodeUnitTarget(const CommandPacket& pkt, UnitId* out);

// --- apply (sim side) ---------------------------------------------------------
// Decode + perform one command against the World. Returns false — and changes
// NOTHING — on an unknown opcode, a malformed payload, or a stale handle, so a
// bad packet can never half-apply. (Validation must stay sim-side: in lockstep
// every peer rejects the same packet the same way.)
bool applyCommand(World& world, const CommandPacket& pkt);

// --- debug backdoor -----------------------------------------------------------
// Condition-Disabled is NOT a player command in OP2 — the sim derives it from
// power/workers/scientists/damage/EMP. Until the economy sim computes it, the
// debug UI simulates the condition through this explicit entry point. It lives
// HERE (not in the render layer) so the "only this module mutates the World"
// rule holds without inventing a fake opcode. Delete when the economy sim
// owns the flag. Returns false on a stale handle.
bool debugSetDisabled(World& world, UnitId target, bool disabled);

} // namespace op2::sim
