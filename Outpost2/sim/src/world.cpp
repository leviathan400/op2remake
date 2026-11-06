#include "op2/sim/world.h"

#include <algorithm>
#include <type_traits>

namespace op2::sim {

namespace {
constexpr std::uint32_t kRingMask = World::kRingSize - 1;
} // namespace

UnitId World::spawn(const Unit& protoRef) {
    const Unit proto = protoRef; // it may live in slots_, which can grow below
    // MapObject allocation, 0x439A10.
    if (count_ == kMaxObjects) return {};
    ++count_;
    std::uint32_t pos = ringHead_;
    for (; pos != ringTail_; pos = (pos + 1) & kRingMask) {
        const Slot& f = slots_[ring_[pos]];
        // `freed + 0x80 >= tick` (signed) skips it, as does a held reference.
        const bool tooRecent = static_cast<std::int32_t>(f.freedTick + kReuseDelay) >=
                               static_cast<std::int32_t>(tick_);
        if (!tooRecent && f.refs == 0) break;
    }
    std::uint32_t idx;
    if (pos != ringTail_) {
        idx = ring_[pos];
        for (std::uint32_t j = pos; j != ringHead_;) { // close the gap: entries before it move up
            const std::uint32_t prev = (j - 1) & kRingMask;
            ring_[j] = ring_[prev];
            j = prev;
        }
        ringHead_ = (ringHead_ + 1) % kMaxObjects; // idiv by 0x3FF @0x439AE6, not the ring's 0x400
    } else {
        // A fresh index. The exe fails here without undoing ++count (0x439B19).
        if (last_ == kMaxObjects) return {};
        idx = ++last_;
        slots_.emplace_back();
    }
    Slot& s = slots_[idx];
    s.unit = proto;
    s.generation += 1;   // first use: 0->1; reuse: bump so old handles go stale
    s.live = 1;
    ++liveCount_;

    // In front of the first object at or below its row (0x439B53).
    const auto at = std::find_if(order_.begin(), order_.end(),
                                 [&](std::uint32_t o) { return slots_[o].unit.py >= s.unit.py; });
    const std::size_t orderPos = static_cast<std::size_t>(at - order_.begin());
    order_.insert(at, idx);
    if (processing_ && orderPos <= cursor_) ++cursor_; // keep the cursor on the current object
    return UnitId{idx, s.generation};
}

void World::destroy(UnitId id) {
    if (!id.valid() || id.index >= slots_.size()) return;
    Slot& s = slots_[id.index];
    if (!s.live || s.generation != id.generation) return; // stale handle
    // MapObject::~MapObject, 0x439C20: onto the ring with the tick it died.
    s.live = 0;
    s.freedTick = tick_;
    --count_;
    ring_[ringTail_] = id.index;
    ringTail_ = (ringTail_ + 1) & kRingMask;
    --liveCount_;

    const auto at = std::find(order_.begin(), order_.end(), id.index);
    const std::size_t pos = static_cast<std::size_t>(at - order_.begin());
    order_.erase(at);
    if (processing_) {
        if (pos < cursor_) --cursor_;
        else if (pos == cursor_) removedCurrent_ = true; // the next object slid into place
    }
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

void World::addRef(UnitId id) {
    if (id.index != 0 && id.index < slots_.size()) ++slots_[id.index].refs;
}

void World::releaseRef(UnitId id) {
    if (id.index != 0 && id.index < slots_.size() && slots_[id.index].refs > 0) --slots_[id.index].refs;
}

std::uint8_t World::refs(UnitId id) const {
    return id.index < slots_.size() ? slots_[id.index].refs : 0;
}

bool World::holdsTile(const Unit& b, int x, int y) const {
    if (b.kind != UnitKind::Building) return false;
    const UnitType& t = type(b.typeId);
    const int tx = structureTopLeft(b.px, t.fpX), ty = structureTopLeft(b.py, t.fpY);
    if (x < tx || x >= tx + t.fpX || y < ty || y >= ty + t.fpY) return false;
    const int dock = (b.flags & UnitFlag::IsEden) ? t.dockEden : t.dockPlymouth;
    return !(dock > 0 && y == ty + t.fpY - 1 && x == tx + dock - 1);
}

std::int32_t World::hitPoints(const Unit& u) const {
    return std::max<std::int32_t>(0, type(u.typeId).maxHitPoints - u.damage);
}

void World::sortOrder() {
    std::stable_sort(order_.begin(), order_.end(), [&](std::uint32_t a, std::uint32_t b) {
        return slots_[a].unit.py < slots_[b].unit.py;
    });
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
    slots_.assign(1, Slot{}); // index 0, never handed out
    ring_.fill(0);
    ringHead_ = ringTail_ = last_ = count_ = 0;
    tick_ = 0;
    order_.clear();
    types_.clear();
    weaponTypes_.clear();
    humanMask_ = 0;
    liveCount_ = 0;
    cursor_ = 0;
    processing_ = removedCurrent_ = false;
}

void World::foldInto(Checksummer& cs) const {
    // The slot layout is state too: which slot is live, its generation and the
    // free-list order decide what a UnitId resolves to and where the next spawn
    // lands. Two worlds with the same units in different slots must differ.
    cs.addI32(liveCount_);
    cs.addU32(static_cast<std::uint32_t>(slots_.size()));
    for (const Slot& s : slots_) {
        cs.addU32(s.generation);
        cs.addU32(s.freedTick);
        cs.addU32(static_cast<std::uint32_t>(s.live) | (static_cast<std::uint32_t>(s.refs) << 8));
    }
    // The allocator decides which index (and so which heartbeat) the next object gets.
    cs.addU32(ringHead_);
    cs.addU32(ringTail_);
    cs.addU32(last_);
    cs.addU32(count_);
    for (std::uint32_t p = ringHead_; p != ringTail_; p = (p + 1) & kRingMask) cs.addU32(ring_[p]);
    // The processing order decides who wins a contested tile.
    for (std::uint32_t o : order_) cs.addU32(o);
    cs.addU32(humanMask_);
    forEach([&](UnitId, const Unit& u) {
        forEachUnitField(u, [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, UnitId>) { cs.addU32(v.index); cs.addU32(v.generation); }
            else if constexpr (std::is_same_v<T, std::array<std::uint8_t, 300>>) cs.addBytes(v.data(), v.size());
            else if constexpr (std::is_enum_v<T>) cs.addU32(static_cast<std::uint32_t>(v));
            else cs.addU32(static_cast<std::uint32_t>(v));
        });
    });
}

void World::setWeaponType(std::uint8_t id, const WeaponType& w) {
    if (id >= weaponTypes_.size()) weaponTypes_.resize(id + 1u);
    weaponTypes_[id] = w;
}

const WeaponType* World::weaponType(std::uint8_t id) const {
    return id < weaponTypes_.size() ? &weaponTypes_[id] : nullptr;
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
// v2 adds Unit.kind; v3: pixel position + sub-pixel, damage instead of hit
// points, turret, processing order; v4: OP2's index allocator (freed-index
// ring, last index, object count, per-slot free tick and references); v5: the
// driving state (goal, route, step in progress); v6: the route is a planned
// path (pathfind.h), not a line; v7: combat state, the human-players mask, and
// every Unit field through forEachUnitField.
constexpr std::uint32_t kVersion = 7;

// Each field little-endian at its own width (enums as a byte, handles as two
// u32s, the route as its 300 bytes), in forEachUnitField's order.
void writeUnit(Writer& w, const Unit& u) {
    forEachUnitField(u, [&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, UnitId>) { w.u32(v.index); w.u32(v.generation); }
        else if constexpr (std::is_same_v<T, std::array<std::uint8_t, 300>>) { for (std::uint8_t b : v) w.u8(b); }
        else if constexpr (std::is_enum_v<T> || sizeof(T) == 1) w.u8(static_cast<std::uint8_t>(v));
        else if constexpr (sizeof(T) == 2) w.u16(static_cast<std::uint16_t>(v));
        else w.u32(static_cast<std::uint32_t>(v));
    });
}
void readUnit(Reader& r, Unit& u) {
    forEachUnitField(u, [&](auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, UnitId>) { v.index = r.u32(); v.generation = r.u32(); }
        else if constexpr (std::is_same_v<T, std::array<std::uint8_t, 300>>) { for (std::uint8_t& b : v) b = r.u8(); }
        else if constexpr (std::is_enum_v<T> || sizeof(T) == 1) v = static_cast<T>(r.u8());
        else if constexpr (sizeof(T) == 2) v = static_cast<T>(r.u16());
        else v = static_cast<T>(r.u32());
    });
}

// Bytes per slot on disk: live u8, generation u32, free tick u32, refs u8, then the unit.
std::size_t slotBytes() {
    static const std::size_t n = [] {
        std::vector<std::uint8_t> b;
        Writer w{b};
        writeUnit(w, Unit{});
        return b.size() + 10;
    }();
    return n;
}

} // namespace

std::vector<std::uint8_t> World::serialize() const {
    std::vector<std::uint8_t> bytes;
    Writer w{bytes};
    w.u32(kMagic);
    w.u32(kVersion);
    w.u32(static_cast<std::uint32_t>(slots_.size())); // last + 1
    for (const Slot& s : slots_) {
        w.u8(s.live);
        w.u32(s.generation);
        w.u32(s.freedTick);
        w.u8(s.refs);
        writeUnit(w, s.unit);
    }
    // Only the ring's live range [head, tail) is ever read, so only it is saved.
    w.u32(ringHead_);
    w.u32(ringTail_);
    w.u32(count_);
    for (std::uint32_t p = ringHead_; p != ringTail_; p = (p + 1) & kRingMask) w.u32(ring_[p]);
    for (std::uint32_t o : order_) w.u32(o); // exactly liveCount entries
    w.u8(humanMask_);
    return bytes;
}

bool World::deserialize(const std::vector<std::uint8_t>& bytes) {
    Reader r{bytes.data(), bytes.data() + bytes.size()};
    if (r.u32() != kMagic) return false;
    if (r.u32() != kVersion) return false;
    const std::uint32_t n = r.u32();
    // Slot 0 always exists; at most kMaxObjects indices after it. A count the
    // buffer can't hold is rejected before allocating for it.
    if (!r.ok || n == 0 || n > kMaxObjects + 1 ||
        n > static_cast<std::size_t>(r.end - r.p) / slotBytes()) return false;

    std::vector<Slot> slots(n);
    int live = 0;
    for (std::uint32_t i = 0; i < n; ++i) {
        slots[i].live = r.u8();
        slots[i].generation = r.u32();
        slots[i].freedTick = r.u32();
        slots[i].refs = r.u8();
        readUnit(r, slots[i].unit);
        if (slots[i].live > 1) return false;
        if (i == 0 && slots[i].live) return false; // index 0 is never handed out
        // A live slot needs a generation a handle can carry (0 = null handle).
        if (slots[i].live && slots[i].generation == 0) return false;
        if (static_cast<std::uint8_t>(slots[i].unit.kind) > static_cast<std::uint8_t>(UnitKind::Other)) return false;
        // A route fits its 300 bytes (600 steps) and its cursor stays inside it.
        if (slots[i].unit.planLen > 600 || slots[i].unit.planPos > slots[i].unit.planLen) return false;
        if (slots[i].live) ++live;
    }
    // The freed-index ring: every entry a dead index that exists, none twice,
    // or a spawn would take a live unit's index or one past the slots.
    const std::uint32_t head = r.u32(), tail = r.u32(), count = r.u32();
    if (!r.ok || head >= kMaxObjects || tail >= kRingSize) return false;
    // The exe's count runs ahead of the live objects only by its failed-spawn leak.
    if (count < static_cast<std::uint32_t>(live) || count > kMaxObjects) return false;
    std::array<std::uint32_t, kRingSize> ring{};
    std::vector<std::uint8_t> listed(n, 0);
    for (std::uint32_t p = head; p != tail; p = (p + 1) & kRingMask) {
        const std::uint32_t f = r.u32();
        if (!r.ok || f == 0 || f >= n || slots[f].live || listed[f]) return false;
        listed[f] = 1;
        ring[p] = f;
    }
    // The processing order lists every live slot exactly once.
    std::vector<std::uint32_t> order(static_cast<std::size_t>(live));
    for (std::uint32_t& o : order) {
        o = r.u32();
        if (!r.ok || o >= n || !slots[o].live || listed[o]) return false;
        listed[o] = 1;
    }
    const std::uint8_t human = r.u8();
    if (!r.ok || r.p != r.end) return false; // trailing bytes: not a buffer we wrote

    slots_ = std::move(slots);
    ring_ = ring;
    ringHead_ = head;
    ringTail_ = tail;
    last_ = n - 1;
    count_ = count;
    order_ = std::move(order);
    humanMask_ = human;
    liveCount_ = live;
    cursor_ = 0;
    processing_ = removedCurrent_ = false;
    return true;
}

} // namespace op2::sim
