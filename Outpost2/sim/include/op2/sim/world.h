#pragma once
#include <cstdint>
#include <vector>

#include "op2/sim/checksum.h"

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
}

// One game object. Buildings and vehicles share this flat struct (as in OP2).
// Integer-only + no pointers → deterministic, checksummable, memcpy-serialisable.
struct Unit {
    std::uint16_t typeId = 0;       // index into the UnitType table
    std::uint8_t  owner = 0;        // player 0..6
    UnitKind      kind = UnitKind::Other; // Building / Vehicle / Weapon — drives gameplay
                                   // rules (e.g. a weapon that only damages structures),
                                   // selection, AI. Per-type really; cached per-instance
                                   // until the UnitType table is populated.
    std::uint8_t  rotation = 0;     // 0..255 (>>4 = 16 facings, as OP2)
    std::int32_t  tileX = 0;        // top-left footprint tile (or position) in tiles
    std::int32_t  tileY = 0;
    std::int32_t  hitPoints = 0;    // current HP (0 = destroyed); vs UnitType.maxHitPoints
    std::uint8_t  command = 0;      // ctNop / ctMove / ctBuild / …  (action queue head)
    std::uint8_t  action = 0;       // current low-level action
    std::uint16_t cargo = 0;        // TruckCargo enum (what a truck carries)
    std::int32_t  cargoAmount = 0;
    std::int32_t  reloadTimer = 0;
    std::int32_t  actionTimer = 0;
    std::int32_t  frame = 0;        // current animation frame
    std::uint32_t flags = 0;        // UnitFlag bits
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
};

// Stable handle to a unit slot. index + generation: a handle into a freed/reused
// slot resolves to null (generation mismatch). A default UnitId is invalid.
struct UnitId {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;   // 0 = invalid/null handle
    bool valid() const { return generation != 0; }
    bool operator==(const UnitId&) const = default;
};

// The world's unit pool: slot array + free-list for O(1) spawn/destroy and
// stable handles. Iteration is in slot-index order (deterministic).
class World {
public:
    // Spawn a copy of `proto`; returns a handle to the new live unit.
    UnitId spawn(const Unit& proto);
    // Mark a unit dead and free its slot (handle becomes stale). No-op if stale.
    void destroy(UnitId id);

    Unit* get(UnitId id);                 // nullptr if dead or stale handle
    const Unit* get(UnitId id) const;

    // --- per-TYPE data (UnitType), indexed by typeId -----------------------
    // Shared, constant-per-scenario data (kind, footprint, max HP, armor) populated
    // from the stat sheets at load. NOT serialised/checksummed — it's derived input,
    // re-built from the sheets, not evolving game state. A unit reads its stats via
    // `world.type(unit.typeId)`.
    void setType(std::uint16_t typeId, const UnitType& t);
    const UnitType& type(std::uint16_t typeId) const;

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

    // Fold every live unit's state into a checksum (slot order). Determinism
    // fingerprint for lockstep + save/load verification.
    void foldInto(Checksummer& cs) const;

    // --- save / load --------------------------------------------------------
    // Snapshot the whole pool to bytes and restore it. Because units hold no
    // pointers, this is just the slot array + free-list + counters (no OnSave/
    // OnLoad pointer translation). Returns false on a malformed buffer.
    std::vector<std::uint8_t> serialize() const;
    bool deserialize(const std::vector<std::uint8_t>& bytes);

    void clear();

private:
    struct Slot {
        Unit unit;
        std::uint32_t generation = 0; // bumped on each (re)use; live slots >= 1
        std::uint8_t live = 0;
    };
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> freeList_;
    std::vector<UnitType> types_;   // per-typeId; derived from sheets, not serialised
    int liveCount_ = 0;
};

} // namespace op2::sim
