#include "op2/sim/command.h"

namespace op2::sim {

namespace {

// Little-endian u32 append/read — explicit byte order so a packet built on one
// host applies identically on any peer (same rule as World::serialize).
void putU32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>(x));
    v.push_back(static_cast<std::uint8_t>(x >> 8));
    v.push_back(static_cast<std::uint8_t>(x >> 16));
    v.push_back(static_cast<std::uint8_t>(x >> 24));
}

bool getU32(const std::vector<std::uint8_t>& v, std::size_t off, std::uint32_t* out) {
    if (off + 4 > v.size()) return false;
    *out = static_cast<std::uint32_t>(v[off]) |
           (static_cast<std::uint32_t>(v[off + 1]) << 8) |
           (static_cast<std::uint32_t>(v[off + 2]) << 16) |
           (static_cast<std::uint32_t>(v[off + 3]) << 24);
    return true;
}

} // namespace

CommandPacket makeIdleCommand(std::uint32_t tick, int player, UnitId target,
                              bool idle) {
    CommandPacket p;
    p.tick = tick;
    p.player = player;
    p.op = idle ? Opcode::ctMoIdle : Opcode::ctMoUnidle;
    putU32(p.data, target.index);
    putU32(p.data, target.generation);
    return p;
}

bool decodeUnitTarget(const CommandPacket& pkt, UnitId* out) {
    UnitId id;
    if (!getU32(pkt.data, 0, &id.index)) return false;
    if (!getU32(pkt.data, 4, &id.generation)) return false;
    *out = id;
    return true;
}

bool applyCommand(World& world, const CommandPacket& pkt) {
    switch (pkt.op) {
        case Opcode::ctMoIdle:
        case Opcode::ctMoUnidle: {
            UnitId id;
            if (!decodeUnitTarget(pkt, &id)) return false;
            Unit* u = world.get(id);
            if (!u) return false; // stale handle: the unit died before the command landed
            if (pkt.op == Opcode::ctMoIdle) u->flags |= UnitFlag::Idle;
            else                            u->flags &= ~UnitFlag::Idle;
            return true;
        }
        default:
            return false; // not dispatched yet (arrives with the gameplay sim)
    }
}

bool debugSetDisabled(World& world, UnitId target, bool disabled) {
    Unit* u = world.get(target);
    if (!u) return false;
    if (disabled) u->flags |= UnitFlag::Disabled;
    else          u->flags &= ~UnitFlag::Disabled;
    return true;
}

} // namespace op2::sim
