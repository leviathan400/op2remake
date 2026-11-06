#pragma once
#include <cstdint>
#include <string>

#include "op2/sim/map.h"
#include "op2/sim/prng.h"
#include "op2/sim/world.h"

// =============================================================================
// Combat — turrets, target search, shots, damage, death
// =============================================================================
//
// Learned from our op2kb (docs/engine/COMBAT.md, WEAPONS.md, the decompile)
// and op2-rs's measured findings (harness/oracle/FINDINGS.md "Turrets and
// weapon fire", "Combat"; notes/findings-turret.md, findings-combat.md);
// reimplemented here. One object's turn runs in the exe's order (ProcessUnits
// 0x0043AD30): its command handling (ProcessForGameCycle), then its weapon
// (UnitTypeInfo:Weapon.Process 0x004A7BC0, at the tail of that), then its
// timers and action (ProcessTimers 0x0043DF60: the 8-tick heartbeat, DoEvent,
// movement).
//
//   * The weapon: reload counts down 1 a tick. With no target it searches on
//     heartbeat ticks only (and idles the turret when nothing is found); a
//     target found is fired at on the next tick. A dead or (lobbed weapons)
//     out-of-range target is dropped; otherwise FireWeapon (0x00484F40) checks
//     range again and creates ONE shot, and the reload restarts (halved for
//     DoubleFire). A dead target is only noticed once the gun has reloaded.
//   * The shot is an object of its own (index, processing order): it slews its
//     parent's turret to the target at the weapon's turret rate (coarse aim),
//     then fires (DoFire: lead, four RNG scatter draws, range clamp), corrects
//     the turret by a unit (fine aim), waits 4 ticks (beams) or flies
//     max(3, d/16+1) steps (projectiles), does its damage (5 ticks after DoFire
//     for a beam, S+2 for a projectile), then lingers for its hit animation.
//   * Damage (0x004A3690) is splash round the impact: vehicles within R take
//     (R-d) * base / R, buildings half the base once per footprint tile within
//     max(R, 26), base = armor * concussion >> 8 + penetration. No ally filter.
//   * Death: dead at once, deleted 5 turns later; a vehicle whose hit points ran
//     out leaves a 26-tick explosion object. A structure starts its destroyed
//     animation (and a visual explosion object) on its next turn and is deleted
//     10 turns after that: then (Building.Explode 0x00483160) its footprint, dock
//     included, becomes bulldozed ground, an Explosion Size 2/3 structure leaves
//     a damaging explosion (0x56 / 0x57, the explosive pass), and each rubble
//     piece takes Rand(tiles left) for its tile and Rand(4) for its graphic,
//     commons first.
//   * Idle tanks retaliate against an attacker that hit them in the last 150
//     ticks; a person's tank hit in the last 12 calls idle allies within 96 px.
//     Attack orders re-check every other tick and chase into range.
//   * Guard Posts run the tanks' weapon process every tick, and on heartbeat
//     ticks their own scan (GuardPost.ScanForTargets 0x0042CF20): retaliate on
//     an attacker within 75 ticks, else search, else sweep the turret
//     ((index ^ tick >> 9) & 3: 1 forward, 3 back), then drop a target out of
//     range. They fire only when enabled; a linked post reaches a tile further
//     (cap 7) and its shots do half as much again.
//
// Not modelled yet: scorch marks (their Rand(3) draw: the RNG stream diverges
// after a vehicle dies on scorchable ground), walls taking damage, the size-1
// structures' Blast (an EMP ring, no hit-point damage), weapons other than Laser,
// Microwave, Rail Gun and Cannon (EMP, Stickyfoam, ESG, Acid, Thor's Hammer,
// Starflare/Supernova), muzzle offsets from the sprite art (projectiles fly
// from the firer's pixel), per-structure destroyed-animation lengths (5 frames,
// as measured for a Common Storage, Agridome and Tokamak), DIRT, allies, tube
// networks (Powered / Linked are set by the caller), the Robot Command Center.

namespace op2::sim {

enum class WeaponKind : std::uint8_t {
    Other = 0, Laser, Microwave, RailGun, Cannon, EMP, Stickyfoam, Acid, ESG, ThorsHammer, EnergyCannon,
    Starflare, Supernova,
    // The explosions a destroyed structure leaves (BLD_EXPL_SML / _MED / _BIG,
    // map_id 0x55-0x57): the explosive damage pass, centred on the structure.
    BuildingExplosionSmall, BuildingExplosionMedium, BuildingExplosionBig,
};

// By weapons.txt Code_Name (LASER, MICROWAVE, RAIL_GUN, CANNON, ...).
WeaponKind weaponKindFromCode(const std::string& code);
// Whether the kind is simulated (Laser, Microwave, Rail Gun, Cannon).
bool weaponSupported(WeaponKind k);

// The armor multiplier for a sheet Armor Class (armorLevelToValue 0x004E1518):
// 0 -> 256, 1 -> 180, 2 -> 130, 3 -> 90, 4 -> 60, 5 -> 40.
std::int32_t armorValue(std::int32_t armorClass) noexcept;

// trunc(64 cos(k * 2pi / 256)), the exe's byte table at 0x004E9B30 (sin =
// cos(k - 64), its twin at 0x004E9C30).
extern const std::int8_t kCos256[256];
inline int cos256(int k) noexcept { return kCos256[k & 255]; }
inline int sin256(int k) noexcept { return kCos256[(k - 64) & 255]; }

// ATan2ToAngle256 (0x0045C630): trunc(atan2(dy, dx) * 128 / pi) as a byte, so
// east is a double-width sector. Exact, in integers.
std::uint8_t bearing(std::int64_t dx, std::int64_t dy) noexcept;

// max(|dx|,|dy|) + min/2, the distance everything in combat uses.
std::int32_t octagonal(std::int32_t dx, std::int32_t dy) noexcept;

// Map.HasLineOfSight (0x00436FA0) between two tiles: a Bresenham walk that
// splits diagonals into two 4-connected steps; a structure's footprint tile or
// a sight-blocking cell (Impassible 1, vents, rubble, walls, cliffs) hides the
// far end, except cliffs (8, 9) within a tile of either end.
bool hasLineOfSight(const World& world, const Map& map, int fromX, int fromY, int toX, int toY);

struct CombatContext {
    World& world;
    Map& map; // a destroyed structure razes its footprint
    Prng& rng;
    std::uint32_t tick;
    bool spontaneousDone = false; // a structure exploded on its own this tick
};

// Building.IsEnabled (0x00483710): powered (the enable bits), not EMPed, not
// developing, damage below 3/4 of its hit points.
bool isEnabled(const World& world, const Unit& u);

// CheckSpontaneouslyExplode (0x00409400), between a structure's command
// handling and its timers on ticks where tick % 1179 == 0: a damaged structure
// with an Explosion Size may blow up (Rand(500) < damage*10/HP + 3).
void spontaneousExplode(CombatContext& c, UnitId id);

// Unit.IsTargetInRange (0x00485100) for a unit target.
bool isTargetInRange(const CombatContext& c, const Unit& u, const Unit& target);

// Give an armed unit an Attack order (ctMoAttackObj): it fires at `target`
// when in range and drives into range otherwise.
void orderAttack(World& world, Unit& u, UnitId target);

// One object's turn (Game::step calls it in processing order). Spawns and
// destroys as it goes; `id` stays valid for live objects.
void combatCommandAndWeapon(CombatContext& c, UnitId id); // Attack order handling + the weapon
void combatHeartbeat(CombatContext& c, UnitId id);        // ProcessTimers' heartbeat part, before the action
void chaseCheck(CombatContext& c, UnitId id);             // an Attack chase stops once in range
void shotTurn(CombatContext& c, UnitId id);
void effectTurn(CombatContext& c, UnitId id);
// A dead unit's turn: counts down, then explodes and is deleted. True if the
// unit is dead (its turn is this and nothing else).
bool deathTurn(CombatContext& c, UnitId id);

// typeId given to explosion objects (kind Other).
inline constexpr std::uint16_t kExplosionType = 0xFFFF;

} // namespace op2::sim
