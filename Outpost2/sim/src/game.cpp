#include "op2/sim/game.h"

#include "op2/sim/checksum.h"
#include "op2/sim/combat.h"
#include "op2/sim/command.h"
#include "op2/sim/movement.h"

namespace op2::sim {

std::vector<CommandPacket> Game::step() {
    world_.setTick(clock_.tick());
    std::vector<CommandPacket> rejected;
    for (const CommandPacket& p : queued_) {
        if (!applyCommand(world_, p)) rejected.push_back(p);
    }
    queued_.clear();
    CombatContext c{world_, map_, prng_, clock_.tick()};
    world_.processUnits([&](UnitId id, Unit& u) {
        // One object's turn, in the exe's order: command handling and the weapon
        // (ProcessForGameCycle), then the heartbeat and the action
        // (ProcessTimers). Spawning can move the slot array, so the unit is
        // fetched by handle after each part.
        switch (u.kind) {
            case UnitKind::Weapon: shotTurn(c, id); return;
            case UnitKind::Other:
                if (u.typeId == kExplosionType) effectTurn(c, id);
                return;
            default: break;
        }
        if (deathTurn(c, id)) return;
        combatCommandAndWeapon(c, id);
        spontaneousExplode(c, id); // slot +0xC, structures, every 1179th tick
        combatHeartbeat(c, id);
        chaseCheck(c, id);
        if (Unit* v = world_.get(id)) moveUnit(world_, map_, id, *v);
    });
    clock_.advance();
    return rejected;
}

std::uint32_t Game::checksum() const {
    // The map's cell types are state too once structures are razed and rubble placed.
    Checksummer cs;
    cs.addU32(checksumSimState(clock_, prng_, world_));
    for (const Tile& t : map_.tiles()) cs.addU32(static_cast<std::uint32_t>(t.cellType));
    return cs.value();
}

// --- save / load -------------------------------------------------------------

namespace {

constexpr std::uint32_t kMagic = 0x4732504F; // "OP2G"
constexpr std::uint32_t kVersion = 2;         // v2: the map's cell types

void putU32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

struct Reader {
    const std::vector<std::uint8_t>& b;
    std::size_t pos = 0;
    bool ok = true;
    std::uint32_t u32() {
        if (b.size() - pos < 4) { ok = false; return 0; } // pos never passes size
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(b[pos + i]) << (8 * i);
        pos += 4;
        return v;
    }
    // `n` bytes, refusing a length the buffer doesn't hold.
    bool take(std::size_t n, std::vector<std::uint8_t>* out) {
        if (!ok || n > b.size() - pos) { ok = false; return false; }
        out->assign(b.begin() + static_cast<std::ptrdiff_t>(pos), b.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
        return true;
    }
};

} // namespace

std::vector<std::uint8_t> Game::serialize() const {
    std::vector<std::uint8_t> b;
    putU32(b, kMagic);
    putU32(b, kVersion);
    putU32(b, clock_.tick());
    putU32(b, prng_.state().s0);
    putU32(b, prng_.state().s1);
    putU32(b, static_cast<std::uint32_t>(queued_.size()));
    for (const CommandPacket& p : queued_) {
        putU32(b, p.tick);
        putU32(b, p.op);
        putU32(b, static_cast<std::uint32_t>(p.player));
        putU32(b, static_cast<std::uint32_t>(p.data.size()));
        b.insert(b.end(), p.data.begin(), p.data.end());
    }
    const std::vector<std::uint8_t> w = world_.serialize();
    putU32(b, static_cast<std::uint32_t>(w.size()));
    b.insert(b.end(), w.begin(), w.end());
    // The map's cell types (razing and rubble change them); its graphics are input.
    putU32(b, static_cast<std::uint32_t>(map_.width()));
    putU32(b, static_cast<std::uint32_t>(map_.height()));
    for (const Tile& t : map_.tiles()) b.push_back(static_cast<std::uint8_t>(t.cellType));
    return b;
}

bool Game::deserialize(const std::vector<std::uint8_t>& bytes) {
    Reader r{bytes};
    if (r.u32() != kMagic || r.u32() != kVersion) return false;
    const std::uint32_t tick = r.u32();
    const Prng::State rng{r.u32(), r.u32()};
    const std::uint32_t nq = r.u32();
    if (!r.ok || nq > bytes.size() / 16) return false; // each packet is at least 16 bytes
    std::vector<CommandPacket> queued(nq);
    for (CommandPacket& p : queued) {
        p.tick = r.u32();
        p.op = r.u32();
        p.player = static_cast<std::int32_t>(r.u32());
        if (!r.take(r.u32(), &p.data)) return false;
    }
    std::vector<std::uint8_t> w;
    if (!r.take(r.u32(), &w)) return false;
    // The cell types must fit the map this game was given (the same map file).
    const std::uint32_t mw = r.u32(), mh = r.u32();
    std::vector<std::uint8_t> cells;
    if (!r.ok || mw != static_cast<std::uint32_t>(map_.width()) || mh != static_cast<std::uint32_t>(map_.height()) ||
        !r.take(static_cast<std::size_t>(mw) * mh, &cells) || r.pos != bytes.size())
        return false;
    for (std::uint8_t cell : cells) if (cell > 31) return false;
    World world;
    world = world_; // keep the unit types; the slots come from the save
    if (!world.deserialize(w)) return false;
    for (std::size_t i = 0; i < cells.size(); ++i)
        map_.at(static_cast<int>(i % mw), static_cast<int>(i / mw)).cellType = static_cast<CellType>(cells[i]);
    clock_.setTick(tick);
    prng_.setState(rng);
    queued_ = std::move(queued);
    world_ = std::move(world);
    world_.setTick(tick);
    return true;
}

} // namespace op2::sim
