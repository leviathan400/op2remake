#include "op2/sim/world.h"

#include <cstring>

namespace op2::sim {

UnitId World::spawn(const Unit& proto) {
    std::uint32_t idx;
    if (!freeList_.empty()) {
        idx = freeList_.back();
        freeList_.pop_back();
    } else {
        idx = static_cast<std::uint32_t>(slots_.size());
        slots_.emplace_back();
    }
    Slot& s = slots_[idx];
    s.unit = proto;
    s.generation += 1;   // first use: 0->1; reuse: bump so old handles go stale
    s.live = 1;
    ++liveCount_;
    return UnitId{idx, s.generation};
}

void World::destroy(UnitId id) {
    if (!id.valid() || id.index >= slots_.size()) return;
    Slot& s = slots_[id.index];
    if (!s.live || s.generation != id.generation) return; // stale handle
    s.live = 0;
    freeList_.push_back(id.index);
    --liveCount_;
}

Unit* World::get(UnitId id) {
    if (!id.valid() || id.index >= slots_.size()) return nullptr;
    Slot& s = slots_[id.index];
    if (!s.live || s.generation != id.generation) return nullptr;
    return &s.unit;
}

const Unit* World::get(UnitId id) const {
    return const_cast<World*>(this)->get(id);
}

void World::setType(std::uint16_t typeId, const UnitType& t) {
    if (typeId >= types_.size()) types_.resize(typeId + 1u);
    types_[typeId] = t;
}

const UnitType& World::type(std::uint16_t typeId) const {
    static const UnitType kDefault{};
    return typeId < types_.size() ? types_[typeId] : kDefault;
}

void World::clear() {
    slots_.clear();
    freeList_.clear();
    types_.clear();
    liveCount_ = 0;
}

void World::foldInto(Checksummer& cs) const {
    cs.addI32(liveCount_);
    forEach([&](UnitId, const Unit& u) {
        cs.addU32(u.typeId);
        cs.addU32(static_cast<std::uint32_t>(u.owner)
                  | (static_cast<std::uint32_t>(u.rotation) << 8)
                  | (static_cast<std::uint32_t>(u.kind) << 16));
        cs.addI32(u.tileX);
        cs.addI32(u.tileY);
        cs.addI32(u.hitPoints);
        cs.addU32(static_cast<std::uint32_t>(u.command) | (static_cast<std::uint32_t>(u.action) << 8) |
                  (static_cast<std::uint32_t>(u.cargo) << 16));
        cs.addI32(u.cargoAmount);
        cs.addI32(u.reloadTimer);
        cs.addI32(u.actionTimer);
        cs.addI32(u.frame);
        cs.addU32(u.flags);
    });
}

// --- save / load -------------------------------------------------------------
// Explicit little-endian field writes (no struct padding, portable + stable).

namespace {

struct Writer {
    std::vector<std::uint8_t>& b;
    void u8(std::uint8_t v)  { b.push_back(v); }
    void u16(std::uint16_t v){ b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); }
    void u32(std::uint32_t v){ for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); }
    void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
};

struct Reader {
    const std::uint8_t* p;
    const std::uint8_t* end;
    bool ok = true;
    std::uint8_t  u8()  { if (p + 1 > end) { ok = false; return 0; } return *p++; }
    std::uint16_t u16() { std::uint16_t v = u8(); v |= std::uint16_t(u8()) << 8; return v; }
    std::uint32_t u32() { std::uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= std::uint32_t(u8()) << (8 * i); return v; }
    std::int32_t  i32() { return static_cast<std::int32_t>(u32()); }
};

constexpr std::uint32_t kMagic   = 0x324f5755; // "WUO2"
constexpr std::uint32_t kVersion = 2;          // v2 adds Unit.kind

void writeUnit(Writer& w, const Unit& u) {
    w.u16(u.typeId); w.u8(u.owner); w.u8(static_cast<std::uint8_t>(u.kind)); w.u8(u.rotation);
    w.i32(u.tileX);  w.i32(u.tileY); w.i32(u.hitPoints);
    w.u8(u.command); w.u8(u.action); w.u16(u.cargo); w.i32(u.cargoAmount);
    w.i32(u.reloadTimer); w.i32(u.actionTimer); w.i32(u.frame); w.u32(u.flags);
}
void readUnit(Reader& r, Unit& u) {
    u.typeId = r.u16(); u.owner = r.u8(); u.kind = static_cast<UnitKind>(r.u8()); u.rotation = r.u8();
    u.tileX = r.i32(); u.tileY = r.i32(); u.hitPoints = r.i32();
    u.command = r.u8(); u.action = r.u8(); u.cargo = r.u16(); u.cargoAmount = r.i32();
    u.reloadTimer = r.i32(); u.actionTimer = r.i32(); u.frame = r.i32(); u.flags = r.u32();
}

} // namespace

std::vector<std::uint8_t> World::serialize() const {
    std::vector<std::uint8_t> bytes;
    Writer w{bytes};
    w.u32(kMagic);
    w.u32(kVersion);
    w.u32(static_cast<std::uint32_t>(slots_.size()));
    for (const Slot& s : slots_) {
        w.u8(s.live);
        w.u32(s.generation);
        writeUnit(w, s.unit);
    }
    w.u32(static_cast<std::uint32_t>(freeList_.size()));
    for (std::uint32_t f : freeList_) w.u32(f);
    return bytes;
}

bool World::deserialize(const std::vector<std::uint8_t>& bytes) {
    Reader r{bytes.data(), bytes.data() + bytes.size()};
    if (r.u32() != kMagic) return false;
    if (r.u32() != kVersion) return false;
    const std::uint32_t n = r.u32();
    if (!r.ok || n > (1u << 24)) return false; // sanity cap

    std::vector<Slot> slots(n);
    int live = 0;
    for (std::uint32_t i = 0; i < n; ++i) {
        slots[i].live = r.u8();
        slots[i].generation = r.u32();
        readUnit(r, slots[i].unit);
        if (slots[i].live) ++live;
    }
    const std::uint32_t fn = r.u32();
    if (!r.ok || fn > n) return false;
    std::vector<std::uint32_t> freeList(fn);
    for (std::uint32_t i = 0; i < fn; ++i) freeList[i] = r.u32();
    if (!r.ok) return false;

    slots_ = std::move(slots);
    freeList_ = std::move(freeList);
    liveCount_ = live;
    return true;
}

} // namespace op2::sim
