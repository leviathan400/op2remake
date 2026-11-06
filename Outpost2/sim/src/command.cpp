#include "op2/sim/command.h"

#include "op2/sim/combat.h"
#include "op2/sim/movement.h"

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

CommandPacket makeMoveCommand(std::uint32_t tick, int player, UnitId target, int tileX, int tileY) {
    CommandPacket p;
    p.tick = tick;
    p.player = player;
    p.op = Opcode::ctMoMove;
    putU32(p.data, target.index);
    putU32(p.data, target.generation);
    putU32(p.data, static_cast<std::uint32_t>(tileX));
    putU32(p.data, static_cast<std::uint32_t>(tileY));
    return p;
}

CommandPacket makeStopCommand(std::uint32_t tick, int player, UnitId target) {
    CommandPacket p;
    p.tick = tick;
    p.player = player;
    p.op = Opcode::ctMoStop;
    putU32(p.data, target.index);
    putU32(p.data, target.generation);
    return p;
}

CommandPacket makeAttackCommand(std::uint32_t tick, int player, UnitId attacker, UnitId victim) {
    CommandPacket p;
    p.tick = tick;
    p.player = player;
    p.op = Opcode::ctMoAttackObj;
    putU32(p.data, attacker.index);
    putU32(p.data, attacker.generation);
    putU32(p.data, victim.index);
    putU32(p.data, victim.generation);
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
        case Opcode::ctMoMove:
        case Opcode::ctMoStop: {
            UnitId id;
            std::uint32_t x = 0, y = 0;
            if (!decodeUnitTarget(pkt, &id)) return false;
            if (pkt.op == Opcode::ctMoMove && (!getU32(pkt.data, 8, &x) || !getU32(pkt.data, 12, &y))) return false;
            Unit* u = world.get(id);
            if (!u || u->kind != UnitKind::Vehicle) return false; // only vehicles drive
            if (pkt.op == Opcode::ctMoMove) orderMove(*u, static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
            else                            orderStop(*u);
            return true;
        }
        case Opcode::ctMoAttackObj: {
            UnitId id, victim;
            if (!decodeUnitTarget(pkt, &id)) return false;
            if (!getU32(pkt.data, 8, &victim.index) || !getU32(pkt.data, 12, &victim.generation)) return false;
            Unit* u = world.get(id);
            const Unit* v = world.get(victim);
            // An armed vehicle, at a live unit other than itself.
            if (!u || u->kind != UnitKind::Vehicle || u->weapon == 0 || (u->flags & UnitFlag::IsDead)) return false;
            if (!v || id == victim || (v->flags & UnitFlag::IsDead)) return false;
            orderAttack(world, *u, victim);
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
