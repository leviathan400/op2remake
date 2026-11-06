#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "op2/sim/checksum.h"
#include "op2/sim/coords.h"

// =============================================================================
// World  —  the game-object model (units + buildings)
// =============================================================================
//
// Mirrors how Outpost 2 stores game objects, made deterministic + pointer-free
// so it drops into the firewalled sim core (integer-only, checksummable,
// trivially serialisable for save/load). See the OP2 reference:
// D:\Oupost2Decompile\Remake\docs\GAME_OBJECT_STORAGE.md, and our design note
// D:\opu\op2remake\docs\GAME_OBJECTS.md.
//
// OP2 packs EVERY object type — building, vehicle, weapon — into one fixed-size
// `Unit` struct (0x54 bytes), distinguished by a vtable + a type index into
// `UnitTypeInfo`, and threads them through a global pool + intrusive linked
// lists. We keep the same split:
//   * Unit       — per-INSTANCE state (owner, position, HP, command, cargo, …).
//   * UnitType   — per-TYPE data (footprint, max HP, sprite mapping).
// but reference units by stable integer HANDLES instead of pointers, so the
// whole World serialises as plain bytes (no pointer swizzling like OP2's
// OnSave/OnLoad).

namespace op2::sim {

// Object category — a cheap discriminator (OP2 uses flags.isBuilding/isVehicle).
enum class UnitKind : std::uint8_t { Building = 0, Vehicle = 1, Weapon = 2, Other = 3 };

// Per-instance flag bits (subset of OP2's 32-bit UnitFlags; extend as needed).
namespace UnitFlag {
    constexpr std::uint32_t IsEden        = 1u << 0; // faction (chassis/anim selection)
    constexpr std::uint32_t IsDead        = 1u << 1; // slot pending removal
    constexpr std::uint32_t IsEMPed       = 1u << 2;
    constexpr std::uint32_t IsStickyfoamed= 1u << 3;
    constexpr std::uint32_t LightsOn      = 1u << 4;
    constexpr std::uint32_t Disabled      = 1u << 5; // no power/workers/scientists
    constexpr std::uint32_t Idle          = 1u << 6; // building status: Idle (not operating).
                                                     // Clear = Active. Idle buildings hold a
                                                     // static frame (no ambient/operate anim),
                                                     // mirroring OP2's IsEnabled gate on
                                                     // Unit.DoAmbientAnimation.
    constexpr std::uint32_t OrderPending  = 1u << 7; // an order arrived this cycle; the unit
                                                     // acts on it on its next turn
    constexpr std::uint32_t TurnInPlace   = 1u << 8; // turning before a step (the turn was > 16)
    constexpr std::uint32_t DoubleFire    = 1u << 9; // reload halved (OP2 flag 0x20; the Tiger)
    // A structure has power, workers and scientists (OP2's enable bits 0x2000 /
    // 0x4000 / 0x8000, set by the colony pass; the remake has no colony economy yet,
    // so this stands in for all three).
    constexpr std::uint32_t Powered       = 1u << 10;
    // A structure's tube group has an operational Command Center (TubeGroup +0x10):
    // a Guard Post reaches a tile further and its shots hit 50% harder.
    constexpr std::uint32_t Linked        = 1u << 11;
    constexpr std::uint32_t ShotLinked    = 1u << 12; // a shot from a linked post (OP2 0x80 on the shot)
    constexpr std::uint32_t DeathStarted  = 1u << 13; // a dead structure's destruction animation is running
}

// Stable handle to a unit slot. index + generation: a handle into a freed/reused
// slot resolves to null (generation mismatch). A default UnitId is invalid.
struct UnitId {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;   // 0 = invalid/null handle
    bool valid() const { return generation != 0; }
    bool operator==(const UnitId&) const = default;
};

// One game object. Buildings and vehicles share this flat struct (as in OP2).
// Integer-only + no pointers → deterministic, checksummable, memcpy-serialisable.
// Spatial conventions (pixels, headings, the structure centre rule): coords.h.
struct Unit {
    std::uint16_t typeId = 0;       // index into the UnitType table
    std::uint8_t  owner = 0;        // player 0..6
    UnitKind      kind = UnitKind::Other; // Building / Vehicle / Weapon — drives gameplay
                                   // rules (e.g. a weapon that only damages structures),
                                   // selection, AI. Per-type really; cached per-instance
                                   // until the UnitType table is populated.
    std::uint8_t  rotation = 0;     // chassis heading: 0 = east, 64 = south, clockwise
    std::uint8_t  turret = 0;       // turret heading, same scale (MapObject +0x2D); starts
                                   // at 0 (east) whatever the chassis faces
    // Position: world pixel in the FILE frame — a vehicle's centre (a tile centre
    // when standing), a structure's footprint centre. The tile is derived
    // (pixelToTile / structureTopLeft), never stored beside it.
    std::int32_t  px = 0;
    std::int32_t  py = 0;
    std::uint16_t subX = kSubPixelRest; // 16.16 fraction while stepping, biased 0x8000
    std::uint16_t subY = kSubPixelRest;
    // Damage taken (MapObject +0x1E): OP2 stores damage, not hit points, so a
    // max-HP upgrade raises current HP too. HP = World::hitPoints(unit).
    std::int32_t  damage = 0;
    std::uint8_t  command = 0;      // ctNop / ctMove / ctBuild / …  (action queue head)
    std::uint8_t  action = 0;       // current low-level action
    std::uint16_t cargo = 0;        // TruckCargo enum (what a truck carries)
    std::int32_t  cargoAmount = 0;
    std::int32_t  reloadTimer = 0;
    std::int32_t  actionTimer = 0;
    std::int32_t  frame = 0;        // current animation frame
    std::uint32_t flags = 0;        // UnitFlag bits
    // Driving (movement.h). The route planned to the goal (pathfind.h): up to
    // 600 direction codes, two per byte (low nibble first), as OP2 packs its
    // coarse plan (WaypointBlock +0xD4); planPos is the next step to take.
    std::int32_t  goalX = 0, goalY = 0;
    std::uint16_t planLen = 0, planPos = 0;
    std::array<std::uint8_t, 300> plan{};
    // The step in progress: ticks left (0 = standing on a tile centre), the
    // tile it is stepping into, and the per-axis velocity (16.16 px/tick).
    std::int32_t  stepTicks = 0;
    std::int32_t  destX = 0, destY = 0;
    std::int32_t  vx = 0, vy = 0;
    // Combat (combat.h). An armed unit's weapon (weapon type + 1, 0 = none), the
    // unit its gun is on (MapObject +0x4C), its Attack order's target (+0x48),
    // and who last damaged it and when (+0x26 / +0x28).
    std::uint8_t  weapon = 0;
    UnitId        fireTarget;
    UnitId        orderTarget;
    std::uint32_t lastAttacker = 0; // object index, 0 = none
    std::uint32_t lastHitTick = 0;
    // A shot (kind Weapon): who fired it, at whom, its action state and the
    // impact point; travelling projectiles also fly from start over flightSteps.
    UnitId        shotParent;
    UnitId        shotTarget;
    std::uint8_t  shotAction = 0, shotLastAction = 0, shotDamaged = 0;
    std::int32_t  impactX = 0, impactY = 0;
    std::int32_t  startX = 0, startY = 0;
    std::int32_t  flightSteps = 0, flightT = 0;
};

// Every field of a Unit, in one fixed order, for save/load and the checksum.
template <class U, class F> void forEachUnitField(U& u, F&& f) {
    f(u.typeId); f(u.owner); f(u.kind); f(u.rotation); f(u.turret);
    f(u.px); f(u.py); f(u.subX); f(u.subY); f(u.damage);
    f(u.command); f(u.action); f(u.cargo); f(u.cargoAmount);
    f(u.reloadTimer); f(u.actionTimer); f(u.frame); f(u.flags);
    f(u.goalX); f(u.goalY); f(u.planLen); f(u.planPos); f(u.plan);
    f(u.stepTicks); f(u.destX); f(u.destY); f(u.vx); f(u.vy);
    f(u.weapon); f(u.fireTarget); f(u.orderTarget); f(u.lastAttacker); f(u.lastHitTick);
    f(u.shotParent); f(u.shotTarget); f(u.shotAction); f(u.shotLastAction); f(u.shotDamaged);
    f(u.impactX); f(u.impactY); f(u.startX); f(u.startY); f(u.flightSteps); f(u.flightT);
}

// Per-weapon data, from weapons.txt (combat.h has the behaviour by code name).
struct WeaponType {
    std::uint8_t kind = 0;          // combat.h WeaponKind
    std::int32_t radius = 16;       // Damage Radius, px (UTI +0x248)
    std::int32_t pixelsSkipped = 0; // Pixels Skipped When Firing (+0x24C)
    std::int32_t range = 1;         // Weapon Range in tiles (+0x20)
    std::int32_t turretRate = 1;    // Turret Turn Rate, 1..16 (+0x2C)
    std::int32_t concussion = 0;    // (+0x30)
    std::int32_t penetration = 0;   // (+0x34)
    std::int32_t reload = 0;        // Reload Time (+0x38)
};

// Per-type, shared data. Sim-relevant fields only (sprite groups stay in the
// render-layer catalogs, keyed by the same typeId).
struct UnitType {
    UnitKind kind = UnitKind::Other;
    std::uint8_t fpX = 1;           // footprint width  (tiles)
    std::uint8_t fpY = 1;           // footprint height (tiles)
    std::int32_t maxHitPoints = 0;  // from the stat sheet (building.txt / vehicles.txt)
    std::int32_t armor = 0;         // armor class (damage mitigation), from the sheet
    // Operating requirements (buildings only, from building.txt; 0 for vehicles).
    std::int32_t powerRequired = 0;
    std::int32_t workersRequired = 0;
    std::int32_t scientistsRequired = 0;
    // Driving (vehicles only, from vehicles.txt): Track Type, Move Points, Turn Rate.
    std::uint8_t track = 1;         // movement.h Track (Wheeled by default)
    std::int32_t moveSpeed = 0;
    std::int32_t turnRate = 0;
    // Combat: sight range in tiles (vehicles.txt "Sight range", UTI +0x20; it
    // caps the target search), and whether other players' units pick this
    // structure on their own (building.txt "Can be AutoTargeted", flag 0x800000).
    std::int32_t sight = 0;
    bool autoTarget = false;
    // Structures (building.txt): Explosion Size (UTI +0x24C bits 7-8; 0 none, 1
    // Blast + medium, 2 medium, 3 big) and the rubble left when destroyed
    // (Normal Rubble +0x2A3, Rare Ore Rubble +0x2A1).
    std::uint8_t explosionSize = 0;
    std::uint8_t rubbleCommon = 0;
    std::uint8_t rubbleRare = 0;
    // The dock (building.txt "Docking Loc E" / "Loc P"): a column of the bottom
    // footprint row, from 1, per colony; 0 = none. The dock tile isn't the
    // structure's: vehicles drive onto it, and shots landing there miss it.
    std::uint8_t dockEden = 0;
    std::uint8_t dockPlymouth = 0;
};

// The 8-tick heartbeat: an object does its periodic work (target search,
// DoEvent, idle turret moves) on ticks where (index ^ tick) & 7 == 0
// (0x43DF6C, 0x4A7C03), so objects with different indices act on different ticks.
constexpr bool isHeartbeatTick(std::uint32_t index, std::uint32_t tick) noexcept {
    return ((index ^ tick) & 7u) == 0;
}

// An idle turret's mood for the next 256 ticks: (index ^ tick >> 8) & 15
// (jump table 0x4A7EE4); neighbours differ, so an idle group looks alive.
constexpr std::uint32_t idleMood(std::uint32_t index, std::uint32_t tick) noexcept {
    return (index ^ (tick >> 8)) & 15u;
}

// Object indices (UnitId::index) are handed out the way OP2 does it, decoded
// from Outpost2.exe (MapObject allocation 0x439A10, MapObject::~MapObject
// 0x439C20):
//   * at most 0x3FF objects live at once; index 0 is never used;
//   * a destroyed object's index goes on a ring of freed indices (tail advances
//     mod 0x400) with the tick it was freed;
//   * a new object takes the FIRST ring entry, scanning from the head, that was
//     freed more than 128 ticks ago and that nothing references any more
//     (MapObject +0x32); the entries before it move up one and the head
//     advances mod 0x3FF (not 0x400: an exe quirk kept as is);
//   * with no such entry it takes a fresh index, last + 1 (none past 0x3FF).
// The index sets an object's 8-tick heartbeat (`isHeartbeatTick`), so this
// decides when units built later in a game act.
//
// `forEach` walks slots in index order; `processUnits` and
// `forEachInOrder` walk the PROCESSING ORDER, OP2's object list:
//   * kept sorted by pixel y (top of the map first), and processed in that
//     order each tick, which decides who wins a contested tile;
//   * a new object goes in front of the first object at or below its pixel
//     row (0x439B53): on an equal row the newer one comes first;
//   * after every object has run, the list is re-sorted by pixel y (end of
//     ProcessUnits, 0x43E8F0); a stable sort of the previous order matches the
//     original's list;
//   * an object created while another is being processed joins the list
//     without shifting the one being processed (it isn't processed twice).
// The list is also the draw order (sorted by the tick's final positions).
class World {
public:
    static constexpr std::uint32_t kMaxObjects = 0x3FF; // live objects, and the highest index
    static constexpr std::uint32_t kRingSize = 0x400;   // freed-index ring
    static constexpr std::uint32_t kReuseDelay = 128;   // ticks before a freed index can return

    World() { clear(); }

    // Spawn a copy of `proto` at the current tick; returns a handle to the new
    // live unit, or an invalid handle when no index is available.
    UnitId spawn(const Unit& proto);
    // Mark a unit dead and free its index at the current tick (handle becomes
    // stale). No-op if stale.
    void destroy(UnitId id);

    Unit* get(UnitId id);                 // nullptr if dead or stale handle
    const Unit* get(UnitId id) const;

    // The sim tick, as spawn/destroy see it (OP2 reads its global tick there).
    // The game loop sets it before each tick's work.
    void setTick(std::uint32_t tick) { tick_ = tick; }
    std::uint32_t tick() const { return tick_; }

    // References held on an object index (MapObject +0x32, a byte): a target
    // or guard link to a unit keeps its index from being reused after it dies.
    // By index, so a stale handle to the dead unit still releases.
    void addRef(UnitId id);
    void releaseRef(UnitId id);
    std::uint8_t refs(UnitId id) const;

    // Whether a structure holds the tile in the tile map: its footprint, less its
    // dock tile (by colony, IsEden). Vehicles hold nothing here.
    bool holdsTile(const Unit& b, int x, int y) const;

    // Current hit points: the type's max minus the damage taken, never below 0.
    // (OP2 keeps max HP per player for upgrades; per-type until those arrive.)
    std::int32_t hitPoints(const Unit& u) const;

    // --- processing order ----------------------------------------------------
    // Run `fn(UnitId, Unit&)` for every live unit in processing order, then
    // re-sort the list by pixel y. `fn` may spawn and destroy units; a spawn
    // can move the slot array, so after one re-`get` the unit by its handle.
    template <class Fn> void processUnits(Fn&& fn) {
        processing_ = true;
        for (cursor_ = 0; cursor_ < order_.size();) {
            const std::uint32_t i = order_[cursor_];
            removedCurrent_ = false;
            fn(UnitId{i, slots_[i].generation}, slots_[i].unit);
            if (!removedCurrent_) ++cursor_;
        }
        processing_ = false;
        sortOrder();
    }
    // Visit live units in processing (= draw) order without changing it.
    template <class Fn> void forEachInOrder(Fn&& fn) const {
        for (std::uint32_t i : order_) fn(UnitId{i, slots_[i].generation}, slots_[i].unit);
    }
    // Stable re-sort by pixel y (ProcessUnits' closing sort).
    void sortOrder();
    // The processing order as slot indices (for tests and tools).
    const std::vector<std::uint32_t>& order() const { return order_; }

    // --- per-TYPE data (UnitType), indexed by typeId -----------------------
    // Shared, constant-per-scenario data (kind, footprint, max HP, armor) populated
    // from the stat sheets at load. NOT serialised/checksummed — it's derived input,
    // re-built from the sheets, not evolving game state. A unit reads its stats via
    // `world.type(unit.typeId)`.
    void setType(std::uint16_t typeId, const UnitType& t);
    const UnitType& type(std::uint16_t typeId) const;
    // Weapon types, by id (Unit::weapon is id + 1); same lifetime as the unit types.
    void setWeaponType(std::uint8_t id, const WeaponType& w);
    const WeaponType* weaponType(std::uint8_t id) const;
    const WeaponType* weaponOf(const Unit& u) const { return u.weapon ? weaponType(u.weapon - 1) : nullptr; }

    // Which players are people (PlayerImpl +0x28): some combat rules apply to
    // them only. Game state: saved and checksummed.
    void setHumanPlayers(std::uint8_t mask) { humanMask_ = mask; }
    bool isHuman(std::uint8_t owner) const { return owner < 8 && ((humanMask_ >> owner) & 1); }

    // By object index: the live unit there, or nullptr / an invalid handle.
    Unit* at(std::uint32_t index) { return index < slots_.size() && slots_[index].live ? &slots_[index].unit : nullptr; }
    const Unit* at(std::uint32_t index) const { return const_cast<World*>(this)->at(index); }
    UnitId idAt(std::uint32_t index) const {
        return index < slots_.size() && slots_[index].live ? UnitId{index, slots_[index].generation} : UnitId{};
    }

    int liveCount() const { return liveCount_; }
    int slotCount() const { return static_cast<int>(slots_.size()); }

    // Visit every LIVE unit in slot order. Fn: void(UnitId, Unit&).
    template <class Fn> void forEach(Fn&& fn) {
        for (std::uint32_t i = 0; i < slots_.size(); ++i)
            if (slots_[i].live) fn(UnitId{i, slots_[i].generation}, slots_[i].unit);
    }
    template <class Fn> void forEach(Fn&& fn) const {
        for (std::uint32_t i = 0; i < slots_.size(); ++i)
            if (slots_[i].live) fn(UnitId{i, slots_[i].generation}, slots_[i].unit);
    }

    // Fold the pool into a checksum: the slot layout (generation, live flag,
    // free tick, references), the index allocator, the processing order and
    // every live unit's state. Determinism fingerprint for lockstep + save/load.
    void foldInto(Checksummer& cs) const;

    // --- save / load --------------------------------------------------------
    // Snapshot the whole pool to bytes and restore it. Because units hold no
    // pointers, this is just the slots + allocator + processing order (no
    // OnSave/OnLoad pointer translation). Returns false on a malformed buffer.
    std::vector<std::uint8_t> serialize() const;
    bool deserialize(const std::vector<std::uint8_t>& bytes);

    // Empty world (keeps nothing, types included).
    void clear();

private:
    struct Slot {
        Unit unit;
        std::uint32_t generation = 0; // bumped on each (re)use; live slots >= 1
        std::uint32_t freedTick = 0;  // tick it was last destroyed
        std::uint8_t refs = 0;        // MapObject +0x32
        std::uint8_t live = 0;
    };
    // slots_[i] is object index i; slot 0 is never used, size is last_ + 1.
    std::vector<Slot> slots_;
    std::array<std::uint32_t, kRingSize> ring_{}; // freed indices, [ringHead_, ringTail_)
    std::uint32_t ringHead_ = 0;  // 0x54F840
    std::uint32_t ringTail_ = 0;  // 0x54F83C
    std::uint32_t last_ = 0;      // highest index handed out, 0x54F838
    std::uint32_t count_ = 0;     // the exe's object count, 0x54F834
    std::uint32_t tick_ = 0;
    std::vector<std::uint32_t> order_; // live slots in processing order
    std::vector<UnitType> types_;   // per-typeId; derived from sheets, not serialised
    std::vector<WeaponType> weaponTypes_; // likewise
    std::uint8_t humanMask_ = 0;
    int liveCount_ = 0;
    // processUnits' position in order_; spawn/destroy during it keep it on the
    // object being processed.
    std::size_t cursor_ = 0;
    bool processing_ = false;
    bool removedCurrent_ = false;
};

} // namespace op2::sim
