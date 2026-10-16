#pragma once
#include <cstdint>
#include <string>
#include <vector>

// =============================================================================
// Command log  —  the recorded stream of player commands
// =============================================================================
//
// OP2 is lockstep deterministic: only player *commands* are exchanged, never
// world snapshots. A recorded command stream + a fixed seed therefore replays
// bit-identically. That makes a cmdlog the gold-standard regression fixture —
// replay it and assert the per-tick state checksum matches.
//
// We use the portable JSONL fixture format (one command per line, hand-editable
// and diffable), which is the validated format from the spec. The original's
// binary .cmdlog layout is only hypothesised (192-byte slots), so we don't rely
// on it yet.
//
// File I/O lives here for loading fixtures — note the per-tick SIMULATION never
// touches the disk; only this test harness does. See Remake/docs/CMDLOG_REPLAY.md.

namespace op2::sim {

// First record of every log: a start-game sentinel (carries GameStartInfo).
inline constexpr std::uint32_t kStartGameMarker = 0xFFFFFFFEu;

// One command issued at a given tick. `op` is the CommandPacket opcode
// (messageType); `data` is the type-specific payload (decoded as bytes — the
// harness routes it, it does not interpret the meaning).
struct CommandPacket {
    std::uint32_t tick = 0;        // timestamp: the tick at which it was issued
    std::uint32_t op = 0;          // opcode / messageType
    std::int32_t player = -1;      // issuing player, or -1 if unspecified
    std::vector<std::uint8_t> data;
};

// Human-readable opcode name ("ctMoMove", "<START_GAME>", or "op_0xNN").
std::string opcodeName(std::uint32_t op);

// Opcode id for a name, or -1 if unknown.
int opcodeFromName(const std::string& name);

// A loaded command log plus its sequencing helpers.
class Cmdlog {
public:
    std::vector<CommandPacket> packets;

    // Read the portable JSONL format. Throws std::runtime_error on parse error
    // or if the file cannot be opened. Lines that are blank, start with '#', or
    // are the {"type":"meta",...} header are skipped.
    static Cmdlog readJsonl(const std::string& path);

    // Write the portable JSONL format (a meta header line, then one packet/line).
    void writeJsonl(const std::string& path,
                    const std::string& source = "synthetic") const;
};

} // namespace op2::sim
