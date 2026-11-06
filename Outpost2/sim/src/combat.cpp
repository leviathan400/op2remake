#include "op2/sim/combat.h"

#include <algorithm>
#include <tuple>

#include "op2/sim/command.h"
#include "op2/sim/coords.h"
#include "op2/sim/movement.h"

namespace op2::sim {

const std::int8_t kCos256[256] = {
    64, 63, 63, 63, 63, 63, 63, 63, 62, 62, 62, 61, 61, 60, 60, 59, 59, 58, 57, 57, 56, 55, 54, 54, 53, 52, 51, 50,
    49, 48, 47, 46, 45, 44, 42, 41, 40, 39, 38, 36, 35, 34, 32, 31, 30, 28, 27, 25, 24, 23, 21, 20, 18, 17, 15, 14,
    12, 10, 9, 7, 6, 4, 3, 1, 0, -1, -3, -4, -6, -7, -9, -10, -12, -14, -15, -17, -18, -20, -21, -23, -24, -25,
    -27, -28, -30, -31, -32, -34, -35, -36, -38, -39, -40, -41, -42, -44, -45, -46, -47, -48, -49, -50, -51, -52,
    -53, -54, -54, -55, -56, -57, -57, -58, -59, -59, -60, -60, -61, -61, -62, -62, -62, -63, -63, -63, -63, -63,
    -63, -63, -64, -63, -63, -63, -63, -63, -63, -63, -62, -62, -62, -61, -61, -60, -60, -59, -59, -58, -57, -57,
    -56, -55, -54, -54, -53, -52, -51, -50, -49, -48, -47, -46, -45, -44, -42, -41, -40, -39, -38, -36, -35, -34,
    -32, -31, -30, -28, -27, -25, -24, -23, -21, -20, -18, -17, -15, -14, -12, -10, -9, -7, -6, -4, -3, -1, 0, 1,
    3, 4, 6, 7, 9, 10, 12, 14, 15, 17, 18, 20, 21, 23, 24, 25, 27, 28, 30, 31, 32, 34, 35, 36, 38, 39, 40, 41, 42,
    44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 54, 55, 56, 57, 57, 58, 59, 59, 60, 60, 61, 61, 62, 62, 62, 63, 63,
    63, 63, 63, 63, 63,
};

namespace {

// round(sin(k pi / 128) * 2^30), k = 0..64: the sector boundaries for `bearing`.
constexpr std::int64_t kSin30[65] = {
    0, 26350943, 52686014, 78989349, 105245103, 131437462, 157550647, 183568930,
    209476638, 235258165, 260897982, 286380643, 311690799, 336813204, 361732726, 386434353,
    410903207, 435124548, 459083786, 482766489, 506158392, 529245404, 552013618, 574449320,
    596538995, 618269338, 639627258, 660599890, 681174602, 701339000, 721080937, 740388522,
    759250125, 777654384, 795590213, 813046808, 830013654, 846480531, 862437520, 877875009,
    892783698, 907154608, 920979082, 934248793, 946955747, 959092290, 970651112, 981625251,
    992008094, 1001793390, 1010975242, 1019548121, 1027506862, 1034846671, 1041563127, 1047652185,
    1053110176, 1057933813, 1062120190, 1065666786, 1068571464, 1070832474, 1072448455, 1073418433,
    1073741824,
};

// sin and cos of k * pi / 128 for k = 0..128, scaled by 2^30.
std::int64_t sinK(int k) noexcept { return kSin30[k <= 64 ? k : 128 - k]; }
std::int64_t cosK(int k) noexcept { return k <= 64 ? kSin30[64 - k] : -kSin30[k - 64]; }

int iabs(int v) noexcept { return v < 0 ? -v : v; }

constexpr std::uint8_t kAimCoarse = 0x12, kAimFine = 0x13, kWait = 6, kFlight = 0x0F, kDelete = 0x0B;
constexpr std::uint8_t kCmdAttack = 0x24, kCmdDeath = 0x2F;

bool alive(const Unit* u) { return u && !(u->flags & UnitFlag::IsDead); }
bool targetable(const Unit& u) {
    return (u.kind == UnitKind::Vehicle || u.kind == UnitKind::Building) && !(u.flags & UnitFlag::IsDead);
}
bool isMoving(const Unit& u) { return u.stepTicks > 0 || u.action == Action::Move; }

// The tile a unit stands on for the tile map: a stepping vehicle is already on
// the tile it is stepping into (StartNextTileStep moves its index there).
std::pair<int, int> tileOf(const Unit& u) {
    if (u.kind == UnitKind::Vehicle && u.stepTicks > 0) return {u.destX, u.destY};
    return {pixelToTile(u.px), pixelToTile(u.py)};
}

void setRef(World& w, UnitId& slot, UnitId to) {
    if (slot == to) return;
    if (slot.valid()) w.releaseRef(slot);
    slot = to;
    if (to.valid()) w.addRef(to);
}

void turnStep(std::uint8_t& heading, std::uint8_t want, int rate) {
    // |d| < rate snaps; otherwise a step of `rate`, a half turn the negative way (0x0043E7A9).
    const int d = static_cast<std::int8_t>(static_cast<std::uint8_t>(want - heading));
    if (iabs(d) < rate) heading = want;
    else heading = static_cast<std::uint8_t>(d < 0 ? heading - rate : heading + rate);
}

bool lobbed(WeaponKind k) {
    return k == WeaponKind::Cannon || k == WeaponKind::EMP || k == WeaponKind::Stickyfoam || k == WeaponKind::Acid ||
           k == WeaponKind::ESG || k == WeaponKind::ThorsHammer;
}
bool beam(WeaponKind k) { return k == WeaponKind::Laser || k == WeaponKind::Microwave || k == WeaponKind::EnergyCannon; }
bool buildingExplosion(WeaponKind k) {
    return k == WeaponKind::BuildingExplosionSmall || k == WeaponKind::BuildingExplosionMedium ||
           k == WeaponKind::BuildingExplosionBig;
}

// The weapon's reach in tiles: +1 (capped at 7) for a linked structure (0x00485100, 0x004A6EA0).
int effectiveRange(const Unit& u, const WeaponType& w) {
    int r = w.range;
    if (u.flags & UnitFlag::Linked) {
        if (++r > 6) r = 7;
    }
    return r;
}

// The weapon's flight time for the aim lead (weapon UTI slot 11).
std::int32_t flightTime(WeaponKind k, std::int32_t d) {
    switch (k) {
        case WeaponKind::Laser: case WeaponKind::Microwave: case WeaponKind::EnergyCannon: return 4;
        case WeaponKind::RailGun: case WeaponKind::Cannon: case WeaponKind::EMP: case WeaponKind::Stickyfoam:
            return std::max(3, (d >> 4) + 1);
        case WeaponKind::Acid: return std::max(6, (d >> 4) + 5);
        case WeaponKind::ESG: return std::max(8, (d >> 4) + 4);
        case WeaponKind::ThorsHammer: return 13;
        default: return 2;
    }
}

// How long a shot lingers after its damage (its hit animation; op2-rs measured
// laser 10, cannon 11, rail gun 41; the microwave's isn't measured: as the laser;
// a structure's explosion object isn't measured either: 1).
std::int32_t lingerTicks(WeaponKind k) {
    switch (k) {
        case WeaponKind::Cannon: return 11;
        case WeaponKind::RailGun: return 41;
        case WeaponKind::BuildingExplosionSmall:
        case WeaponKind::BuildingExplosionMedium:
        case WeaponKind::BuildingExplosionBig: return 1;
        default: return 10;
    }
}

// Where a unit target is for range checks (GetLocation): a vehicle's tile; a
// structure's footprint tile nearest the attacker (its rectangle: `size` round
// the pixel tile, w/2 left and h/2 up).
std::pair<int, int> locationFor(const World& w, const Unit& target, int fromX, int fromY) {
    if (target.kind != UnitKind::Building) return tileOf(target);
    const UnitType& t = w.type(target.typeId);
    const int cx = pixelToTile(target.px), cy = pixelToTile(target.py);
    const int x0 = cx - t.fpX / 2, y0 = cy - t.fpY / 2;
    return {std::clamp(fromX, x0, x0 + t.fpX - 1), std::clamp(fromY, y0, y0 + t.fpY - 1)};
}

bool inRangeOf(const CombatContext& c, const Unit& u, const Unit& target, int range) {
    const WeaponType* w = c.world.weaponOf(u);
    if (!w) return false;
    const int ux = pixelToTile(u.px), uy = pixelToTile(u.py);
    const auto [lx, ly] = locationFor(c.world, target, ux, uy);
    const int d = octagonal(tileCentrePx(lx) - u.px, tileCentrePx(ly) - u.py) >> 5;
    if (!lobbed(static_cast<WeaponKind>(w->kind)) && d != 0)
        return d <= range && hasLineOfSight(c.world, c.map, ux, uy, lx, ly);
    return d <= range;
}

// A structure holds the tile (its footprint less its dock: the tile map's unit
// index and bit 31 aren't set on the dock).
bool footprintHolds(const World& w, const Unit& b, int x, int y) { return w.holdsTile(b, x, y); }

// FindTargetInSightRange (0x004A6EA0): walk the y-sorted list up from the
// searcher, then down, within 32r px of its row; the first enemy that passes
// wins (x strictly inside the box, y inclusive).
UnitId findTarget(const CombatContext& c, UnitId self, const Unit& u, std::uint32_t mask) {
    const WeaponType* w = c.world.weaponOf(u);
    if (!w) return {};
    // Both terms of the exe's min() read the weapon's range: the chassis' sight
    // doesn't limit the search (op2-rs measured a linked range-4 post at 5).
    const int r = effectiveRange(u, *w);
    const std::vector<std::uint32_t>& order = c.world.order();
    std::size_t pos = 0;
    while (pos < order.size() && order[pos] != self.index) ++pos;
    const int ux = pixelToTile(u.px), uy = pixelToTile(u.py);
    const bool needSight = !lobbed(static_cast<WeaponKind>(w->kind));

    auto accept = [&](std::uint32_t index) -> bool {
        const Unit* t = c.world.at(index);
        if (!t || index == self.index || !targetable(*t)) return false;
        if (iabs(t->px - u.px) >= 32 * r) return false;
        if (t->kind == UnitKind::Building && !c.world.type(t->typeId).autoTarget) return false;
        if ((mask & 0x2) && t->kind != UnitKind::Vehicle) return false; // IsVehicle
        if ((mask & 0x10) && t->weapon == 0) return false;              // IsOffensive
        if (t->owner == u.owner) return false;                          // no alliances modelled
        if (needSight && !hasLineOfSight(c.world, c.map, pixelToTile(t->px), pixelToTile(t->py), ux, uy)) return false;
        return true;
    };
    for (std::size_t i = pos; i-- > 0;) {
        const Unit* t = c.world.at(order[i]);
        if (t && t->py < u.py - 32 * r) break;
        if (accept(order[i])) return c.world.idAt(order[i]);
    }
    for (std::size_t i = pos + 1; i < order.size(); ++i) {
        const Unit* t = c.world.at(order[i]);
        if (t && t->py > u.py + 32 * r) break;
        if (accept(order[i])) return c.world.idAt(order[i]);
    }
    return {};
}

// IdleTurretWander (0x004A7DD0): on a heartbeat with nothing to shoot.
void idleTurret(CombatContext& c, UnitId id, Unit& u) {
    static constexpr std::uint8_t kWhileMoving[16] = {0, 1, 0, 3, 4, 5, 6, 7, 8, 6, 10, 11, 12, 13, 14, 14};
    std::uint32_t mode = idleMood(id.index, c.tick);
    if (isMoving(u)) mode = kWhileMoving[mode];
    const WeaponType* w = c.world.weaponOf(u);
    const int spin = w ? w->turretRate : 1;
    // The chassis UTI's +0x2C: Move Points for a vehicle, Power required (10) for a Guard Post.
    const UnitType& chassis = c.world.type(u.typeId);
    const int settle = std::max(1, u.kind == UnitKind::Building ? chassis.powerRequired : chassis.moveSpeed);
    switch (mode) {
        case 1: u.turret = static_cast<std::uint8_t>(u.turret + spin); break;
        case 8: u.turret = static_cast<std::uint8_t>(u.turret - spin); break;
        case 0: case 3: case 5: case 7: case 10: case 12: case 14: turnStep(u.turret, u.rotation, settle); break;
        case 6: case 13: turnStep(u.turret, static_cast<std::uint8_t>(c.rng.getRand(256)), settle); break;
        default: break;
    }
}

// Unit.StartDeath (0x0043A990): dead at once; a vehicle is deleted 5 turns
// later, a structure 5 + its destroyed animation's 5 frames later (0x00408A20).
void startDeath(Unit& u) {
    u.flags |= UnitFlag::IsDead;
    u.command = kCmdDeath;
    u.action = Action::Done;
    u.actionTimer = u.kind == UnitKind::Building ? 10 : 5;
    u.planLen = u.planPos = 0;
    u.stepTicks = 0;
    u.vx = u.vy = 0;
}

// The standard damage pass (Unit:Weapon.DoDamage 0x004A3690) at the shot's
// impact, or the explosive pass (0x004A4160: no x1.5, structures within R).
void doDamage(CombatContext& c, const Unit& shot) {
    const WeaponType* w = c.world.weaponOf(shot);
    if (!w) return;
    const bool explosive = buildingExplosion(static_cast<WeaponKind>(w->kind));
    const bool linked = !explosive && (shot.flags & UnitFlag::ShotLinked);
    const int r = w->radius;
    const int structureReach = explosive ? r : std::max(r, 26);
    const int reach = (r + 31) / 32;
    const int ix = pixelToTile(shot.impactX), iy = pixelToTile(shot.impactY);
    const std::uint32_t source = shot.shotParent.index;
    auto hit = [&](UnitId tid, Unit& t, std::int32_t amount) {
        if (amount <= 0) return;
        t.damage += amount;
        if (t.lastAttacker != source) {
            if (t.lastAttacker) c.world.releaseRef(UnitId{t.lastAttacker, 1});
            c.world.addRef(UnitId{source, 1});
            t.lastAttacker = source;
        }
        t.lastHitTick = c.tick;
        const std::int32_t maxHp = c.world.type(t.typeId).maxHitPoints;
        if (maxHp > 0 && t.damage >= maxHp) {
            t.damage = maxHp;
            startDeath(t);
        }
        (void)tid;
    };
    for (int y = iy - reach; y <= iy + reach; ++y) {
        if (y < 0 || y >= c.map.height()) continue;
        for (int x = ix - reach; x <= ix + reach; ++x) {
            c.world.forEach([&](UnitId tid, Unit& t) {
                if (!targetable(t)) return;
                std::int32_t base = (armorValue(c.world.type(t.typeId).armor) * w->concussion >> 8) + w->penetration;
                if (linked) base += base >> 1; // a linked Guard Post's shot
                if (t.kind == UnitKind::Vehicle) {
                    if (tileOf(t) != std::pair<int, int>{x, y}) return;
                    const int d = octagonal(t.px - shot.impactX, t.py - shot.impactY);
                    if (d < r) hit(tid, t, (r - d) * base / r);
                } else if (footprintHolds(c.world, t, x, y)) {
                    const int d = octagonal(tileCentrePx(x) - shot.impactX, tileCentrePx(y) - shot.impactY);
                    if (d < structureReach) hit(tid, t, 10000 * base / 20000);
                }
            });
        }
    }
}

// Launch (0x004A3EE0 / LaunchProjectile 0x004A3B70): scatter round the aim
// point with four draws (x+, y+, y-, x-), clamp to the weapon's reach, face it.
void launch(CombatContext& c, Unit& shot, std::int32_t aimX, std::int32_t aimY, const WeaponType& w) {
    const std::int32_t n = octagonal(aimX - shot.px, aimY - shot.py) >> 4;
    std::int32_t dx = aimX + static_cast<std::int32_t>(c.rng.getRand(static_cast<std::uint32_t>(n)));
    std::int32_t dy = aimY + static_cast<std::int32_t>(c.rng.getRand(static_cast<std::uint32_t>(n)));
    dy -= static_cast<std::int32_t>(c.rng.getRand(static_cast<std::uint32_t>(n)));
    dx -= static_cast<std::int32_t>(c.rng.getRand(static_cast<std::uint32_t>(n)));
    std::int32_t minD = w.pixelsSkipped, maxD = w.range * 32 + ((shot.flags & UnitFlag::ShotLinked) ? 32 : 0);
    const std::int32_t yMax = c.map.height() * 32 - 16;
    std::uint8_t rot = 0;
    for (int guard = 0; guard < 8; ++guard) {
        rot = bearing(dx - shot.px, dy - shot.py);
        const std::int32_t dist = octagonal(dx - shot.px, dy - shot.py);
        std::int32_t to = -1;
        if (dist < minD) { to = minD; minD = 0; }
        else if (dist > maxD) { to = maxD; maxD *= 2; }
        if (to < 0) break;
        dx = shot.px + (cos256(rot) * to >> 6);
        dy = std::clamp(shot.py + (sin256(rot) * to >> 6), 16, yMax);
    }
    shot.rotation = rot;
    shot.impactX = dx;
    shot.impactY = dy;
}

// The aim point (GetHomingAimPoint 0x004A6A10): a moving target is led by its
// velocity times the weapon's flight time.
std::pair<std::int32_t, std::int32_t> aimPoint(const Unit& firer, const Unit& target, WeaponKind k) {
    if (target.kind != UnitKind::Vehicle || target.stepTicks == 0) return {target.px, target.py};
    const std::int32_t t = flightTime(k, octagonal(target.px - firer.px, target.py - firer.py));
    return {target.px + static_cast<std::int32_t>((static_cast<std::int64_t>(target.vx) * t + target.subX) >> 16),
            target.py + static_cast<std::int32_t>((static_cast<std::int64_t>(target.vy) * t + target.subY) >> 16)};
}

// Tank FireWeapon (0x00484F40): range check, then one shot at (x, y + 1), its
// centre put in its tile.
bool fireWeapon(CombatContext& c, UnitId id) {
    Unit* u = c.world.get(id);
    Unit* target = c.world.get(u->fireTarget);
    // GuardPost.FireWeapon (0x0042D120) first needs the post enabled.
    const bool post = u->kind == UnitKind::Building;
    if (!target || (post && !isEnabled(c.world, *u)) || !isTargetInRange(c, *u, *target)) {
        setRef(c.world, u->fireTarget, {});
        return false;
    }
    Unit shot;
    shot.kind = UnitKind::Weapon;
    shot.owner = u->owner;
    shot.weapon = u->weapon;
    if (post && (u->flags & UnitFlag::Linked)) shot.flags |= UnitFlag::ShotLinked; // the shot's 0x90
    shot.px = (u->px & ~31) + 16;
    shot.py = ((u->py + 1) & ~31) + 16;
    shot.shotAction = kAimCoarse;
    shot.impactX = target->px;
    shot.impactY = target->py;
    const UnitId parent = id, aimAt = u->fireTarget;
    const UnitId sid = c.world.spawn(shot); // may move the slot array: re-get below
    if (!sid.valid()) return false;
    Unit* s = c.world.get(sid);
    setRef(c.world, s->shotParent, parent); // weapon fire holds its firer's index
    s->shotTarget = aimAt;
    return true;
}

// UnitTypeInfo:Weapon.Process (0x004A7BC0).
void weaponProcess(CombatContext& c, UnitId id) {
    Unit* u = c.world.get(id);
    const WeaponType* w = c.world.weaponOf(*u);
    if (!w || !weaponSupported(static_cast<WeaponKind>(w->kind))) return;
    if (u->reloadTimer != 0) { --u->reloadTimer; return; }
    if (!u->fireTarget.valid()) {
        if (!isHeartbeatTick(id.index, c.tick)) return;
        std::uint32_t mask = 0x400;
        if (isMoving(*u)) mask |= 0x2;
        if (u->orderTarget.valid()) mask |= 0x10;
        const UnitId t = findTarget(c, id, *u, mask);
        if (!t.valid()) { idleTurret(c, id, *u); return; }
        setRef(c.world, u->fireTarget, t); // fired at on the next tick
        return;
    }
    const Unit* target = c.world.get(u->fireTarget);
    if (!alive(target) || (lobbed(static_cast<WeaponKind>(w->kind)) && !isTargetInRange(c, *u, *target))) {
        setRef(c.world, u->fireTarget, {});
        return;
    }
    if (fireWeapon(c, id)) {
        u = c.world.get(id);
        u->reloadTimer = w->reload >> ((u->flags & UnitFlag::DoubleFire) ? 1 : 0);
    }
}

// AlertNearbyAllies (0x0044DF20): a person's idle combat units within 96 px
// take on the victim's attacker, as if hit 25 ticks ago.
void alertAllies(CombatContext& c, const Unit& victim) {
    c.world.forEach([&](UnitId, Unit& o) {
        if (o.owner != victim.owner || o.weapon == 0 || o.command != 0 || o.lastAttacker != 0) return;
        if ((o.flags & (UnitFlag::IsDead | UnitFlag::IsEMPed)) || &o == &victim) return;
        if (iabs(o.px - victim.px) >= 96 || iabs(o.py - victim.py) > 96) return;
        o.lastAttacker = victim.lastAttacker;
        o.lastHitTick = c.tick - 25;
        c.world.addRef(UnitId{victim.lastAttacker, 1});
    });
}

} // namespace

WeaponKind weaponKindFromCode(const std::string& code) {
    if (code == "LASER") return WeaponKind::Laser;
    if (code == "MICROWAVE") return WeaponKind::Microwave;
    if (code == "RAIL_GUN") return WeaponKind::RailGun;
    if (code == "CANNON") return WeaponKind::Cannon;
    if (code == "EMP") return WeaponKind::EMP;
    if (code == "FOAM") return WeaponKind::Stickyfoam;
    if (code == "ACID") return WeaponKind::Acid;
    if (code == "SPAM") return WeaponKind::ESG;
    if (code == "THORS_HAMMER") return WeaponKind::ThorsHammer;
    if (code == "SCORPION_WEAPON") return WeaponKind::EnergyCannon;
    if (code == "SELF_DESTRUCT") return WeaponKind::Starflare;
    if (code == "SELF_DESTRUCT_ADV") return WeaponKind::Supernova;
    if (code == "BLD_EXPL_SML") return WeaponKind::BuildingExplosionSmall;
    if (code == "BLD_EXPL_MED") return WeaponKind::BuildingExplosionMedium;
    if (code == "BLD_EXPL_BIG") return WeaponKind::BuildingExplosionBig;
    return WeaponKind::Other;
}

bool weaponSupported(WeaponKind k) {
    return k == WeaponKind::Laser || k == WeaponKind::Microwave || k == WeaponKind::RailGun || k == WeaponKind::Cannon;
}

std::int32_t armorValue(std::int32_t armorClass) noexcept {
    static constexpr std::int32_t kArmor[6] = {256, 180, 130, 90, 60, 40};
    return kArmor[std::clamp(armorClass, 0, 5)];
}

std::int32_t octagonal(std::int32_t dx, std::int32_t dy) noexcept {
    const std::int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    return ax >= ay ? ax + ay / 2 : ay + ax / 2;
}

std::uint8_t bearing(std::int64_t dx, std::int64_t dy) noexcept {
    if (dx == 0 && dy == 0) return 0;
    const std::int64_t ay = dy < 0 ? -dy : dy;
    // The last k in 0..128 whose ray k*pi/128 the vector (dx, |dy|) is at or past.
    int lo = 0, hi = 128;
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;
        if (cosK(mid) * ay - sinK(mid) * dx >= 0) lo = mid; else hi = mid - 1;
    }
    // trunc toward zero: below the x axis the angle is -lo (and a half turn stays 128).
    return static_cast<std::uint8_t>(dy < 0 ? (256 - lo) & 255 : lo);
}

bool hasLineOfSight(const World& world, const Map& map, int fromX, int fromY, int toX, int toY) {
    const int dxT = toX - fromX, dyT = toY - fromY;
    if (dxT == 0 && dyT == 0) return false;
    // Doubled deltas, major-axis stepping; a minor step is taken on its own
    // tile first (4-connected), except on an exact tie (0x00436FA0).
    int twiceMinor = iabs(dxT) * 2, twiceMajor = iabs(dyT) * 2;
    const bool yMajor = twiceMinor < twiceMajor;
    int minorPos, majorPos, minorSign, majorSign;
    if (yMajor) {
        minorPos = 0; majorPos = fromY;
        minorSign = dxT >= 1 ? 1 : -1; majorSign = dyT >= 1 ? 1 : -1;
    } else {
        std::swap(twiceMinor, twiceMajor);
        minorPos = fromY; majorPos = 0;
        minorSign = dyT >= 1 ? 1 : -1; majorSign = dxT >= 1 ? 1 : -1;
    }
    int pending = 0;
    int err = twiceMinor - (twiceMinor / 2 + twiceMajor / 2);
    auto advance = [&](bool first) {
        if (!first && pending) { majorPos += majorSign; --pending; return; }
        if (!first) err += twiceMinor;
        if (err < 1) {
            if (err == 0) { err = -twiceMajor; minorPos += minorSign; }
            majorPos += majorSign;
        } else {
            err -= twiceMajor;
            minorPos += minorSign;
            pending = 1;
        }
    };
    advance(true);
    auto blocked = [&](int x, int y) {
        if (!map.inBounds(x, y)) return true;
        bool structure = false;
        world.forEach([&](UnitId, const Unit& b) {
            if (!structure && b.kind == UnitKind::Building) structure = footprintHolds(world, b, x, y);
        });
        return structure;
    };
    for (int guard = 0; guard < 4096; ++guard) {
        const int curDx = yMajor ? minorPos : majorPos;
        const int curY = yMajor ? majorPos : minorPos;
        if (curDx == dxT && curY == toY) return true; // the 2x2 result table 0x004D0488 is never 0
        const int x = fromX + curDx;
        if (blocked(x, curY)) return false;
        const int cell = static_cast<int>(map.at(x, curY).cellType);
        const bool losBlock = cell == 6 || cell == 8 || cell == 9 || cell == 11 || cell == 22 || cell == 23 ||
                              cell == 24 || cell == 25 || cell == 14 || cell == 15;
        if (losBlock) {
            if (cell != 8 && cell != 9) return false;
            const bool nearStart = iabs(curDx) <= 1 && iabs(curY - fromY) <= 1;
            const bool nearEnd = iabs(dxT - curDx) <= 1 && iabs(curY - toY) <= 1;
            if (!nearStart && !nearEnd) return false;
        }
        advance(false);
    }
    return false;
}

bool isTargetInRange(const CombatContext& c, const Unit& u, const Unit& target) {
    const WeaponType* w = c.world.weaponOf(u);
    return w && inRangeOf(c, u, target, effectiveRange(u, *w));
}

bool isEnabled(const World& world, const Unit& u) {
    if (u.kind != UnitKind::Building) return true;
    const std::int32_t hp = world.type(u.typeId).maxHitPoints;
    return (u.flags & UnitFlag::Powered) && !(u.flags & (UnitFlag::IsEMPed | UnitFlag::Idle | UnitFlag::IsDead)) &&
           u.damage < (hp * 3 >> 2);
}

void spontaneousExplode(CombatContext& c, UnitId id) {
    if (c.spontaneousDone || c.tick % 1179 != 0) return;
    Unit* u = c.world.get(id);
    if (u->kind != UnitKind::Building || (u->flags & (UnitFlag::IsDead | UnitFlag::Idle))) return;
    const UnitType& t = c.world.type(u->typeId);
    if (t.explosionSize == 0 || t.maxHitPoints <= 0 || u->damage * 10 < t.maxHitPoints) return;
    // One draw per eligible structure (the exe scales the odds for map_id 0x2A; not modelled).
    const std::uint32_t roll = c.rng.getRand(500);
    if (roll < static_cast<std::uint32_t>(u->damage * 10 / t.maxHitPoints + 3)) {
        u->damage = t.maxHitPoints;
        startDeath(*u);
        u->actionTimer = 1 + 5; // StartDeath(1), then the destroyed animation
        c.spontaneousDone = true; // ProcessUnits asks no further structure this tick
    }
}

void orderAttack(World& world, Unit& u, UnitId target) {
    setRef(world, u.orderTarget, target);
    u.command = kCmdAttack;
    u.planLen = u.planPos = 0;
    u.flags |= UnitFlag::OrderPending; // busy: acted on next turn
}

void combatCommandAndWeapon(CombatContext& c, UnitId id) {
    Unit* u = c.world.get(id);
    if (u->kind == UnitKind::Building) {
        // A Guard Post's Attack order (case 0x24 without IsOffensive): drop it
        // when the target is gone; take it as the gun's target once in range.
        if (u->command == kCmdAttack && !(u->flags & UnitFlag::OrderPending)) {
            const Unit* target = c.world.get(u->orderTarget);
            if (!alive(target)) {
                setRef(c.world, u->orderTarget, {});
                u->command = 0;
            } else if (!(u->fireTarget == u->orderTarget) && isTargetInRange(c, *u, *target)) {
                setRef(c.world, u->fireTarget, u->orderTarget);
            }
        }
        u->flags &= ~UnitFlag::OrderPending;
        if (c.world.weaponOf(*u)) weaponProcess(c, id);
        return;
    }
    // ctMoAttackObj (ProcessForGameCycle case 0x24, 0x0043D30F): only when not
    // busy and standing; busy toggles, so it is checked every other tick.
    if (u->command == kCmdAttack && !(u->flags & UnitFlag::OrderPending) && u->action == Action::Done &&
        u->stepTicks == 0) {
        const Unit* target = c.world.get(u->orderTarget);
        if (!alive(target)) {
            setRef(c.world, u->orderTarget, {});
            u->command = 0;
        } else if (isTargetInRange(c, *u, *target)) {
            if (!(u->fireTarget == u->orderTarget)) setRef(c.world, u->fireTarget, u->orderTarget);
            else u->flags |= UnitFlag::OrderPending; // re-issued: checked again in two ticks
        } else {
            // Drive towards it (SetAttackOrGuardCommand 0x00423970); chaseCheck stops it in range.
            const auto [tx, ty] = tileOf(*target);
            u->goalX = tx;
            u->goalY = ty;
            u->planLen = u->planPos = 0;
            u->action = Action::Move;
            u->flags |= UnitFlag::OrderPending;
        }
    }
    if (c.world.weaponOf(*c.world.get(id))) weaponProcess(c, id);
}

void chaseCheck(CombatContext& c, UnitId id) {
    Unit* u = c.world.get(id);
    if (u->command != kCmdAttack || u->action != Action::Move || u->stepTicks != 0 || (u->flags & UnitFlag::OrderPending))
        return;
    const Unit* target = c.world.get(u->orderTarget);
    const WeaponType* w = c.world.weaponOf(*u);
    if (!alive(target) || !w) return;
    // The chase's goal test (0x00423CB4): within range - 2 (moving target) - 1 tiles.
    const int range = std::max(1, w->range - (isMoving(*target) ? 2 : 0) - 1);
    if (inRangeOf(c, *u, *target, range)) {
        u->action = Action::Done;
        u->planLen = u->planPos = 0;
    }
}

void combatHeartbeat(CombatContext& c, UnitId id) {
    if (!isHeartbeatTick(id.index, c.tick)) return;
    Unit* u = c.world.get(id);
    if (u->flags & (UnitFlag::IsDead | UnitFlag::IsEMPed)) return;
    // ProcessTimers: a last attacker is forgotten after 700 ticks (0x0043E062).
    if (u->lastAttacker && c.tick - u->lastHitTick > 700) {
        c.world.releaseRef(UnitId{u->lastAttacker, 1});
        u->lastAttacker = 0;
    }
    const WeaponType* w = c.world.weaponOf(*u);
    if (!w || !weaponSupported(static_cast<WeaponKind>(w->kind))) return;
    if (u->kind == UnitKind::Building) {
        // GuardPost.ScanForTargets (0x0042CF20), after the weapon process.
        if (u->fireTarget.valid() || !isEnabled(c.world, *u)) return;
        if (u->command == 0 && u->action == Action::Done && u->lastAttacker &&
            c.tick - u->lastHitTick < 75) {
            const Unit* attacker = c.world.at(u->lastAttacker);
            if (!attacker || attacker->owner != u->owner) {
                // SetCommandTarget (0x00423F70): a dead attacker (deleted here, or
                // flagged) clears the order; it is asked again each beat for 75 ticks.
                if (alive(attacker)) {
                    setRef(c.world, u->orderTarget, c.world.idAt(u->lastAttacker));
                    u->command = kCmdAttack;
                } else {
                    setRef(c.world, u->orderTarget, {});
                    u->command = 0;
                }
                u->flags |= UnitFlag::OrderPending;
                return;
            }
        }
        const UnitId t = findTarget(c, id, *u, 0x400);
        if (t.valid()) {
            setRef(c.world, u->fireTarget, t);
        } else {
            // The powered sweep: (index ^ tick >> 9) & 3, 1 forward, 3 back.
            const std::uint32_t mode = (id.index ^ (c.tick >> 9)) & 3;
            if (mode == 1) u->turret = static_cast<std::uint8_t>(u->turret + w->turretRate);
            if (mode == 3) u->turret = static_cast<std::uint8_t>(u->turret - w->turretRate);
        }
        if (const Unit* held = c.world.get(u->fireTarget); held && !isTargetInRange(c, *u, *held))
            setRef(c.world, u->fireTarget, {});
        return;
    }
    if (u->kind != UnitKind::Vehicle) return;
    // Tank::DoEvent (0x00485300).
    if (u->lastAttacker && c.tick - u->lastHitTick < 12 && c.world.isHuman(u->owner)) alertAllies(c, *u);
    if (u->fireTarget.valid()) return;
    const bool idle = u->command == 0 && u->action == Action::Done && u->stepTicks == 0;
    if (idle && u->lastAttacker && c.tick - u->lastHitTick < 150) {
        const Unit* attacker = c.world.at(u->lastAttacker);
        if (attacker && attacker->owner != u->owner) {
            orderAttack(c.world, *u, c.world.idAt(u->lastAttacker));
            return;
        }
    }
    std::uint32_t mask = 0x400;
    if (c.world.isHuman(u->owner)) {
        if (isMoving(*u)) mask |= 0x2;
        if (u->orderTarget.valid()) mask |= 0x10;
    }
    const UnitId t = findTarget(c, id, *u, mask);
    if (t.valid()) setRef(c.world, u->fireTarget, t);
}

namespace {

std::uint8_t weaponIdOfKind(const World& w, WeaponKind k) {
    for (std::uint8_t i = 0; i < 250; ++i) {
        const WeaponType* t = w.weaponType(i);
        if (!t) break;
        if (static_cast<WeaponKind>(t->kind) == k) return static_cast<std::uint8_t>(i + 1);
    }
    return 0;
}

// Building.Explode (0x00483160) at a destroyed structure's deletion: raze the
// footprint (dock included) to bulldozed ground, leave the explosion object its
// Explosion Size calls for, then place the rubble with the gameplay RNG.
void explodeStructure(CombatContext& c, UnitId id, const Unit& b) {
    const UnitType& t = c.world.type(b.typeId);
    const int x0 = pixelToTile(b.px) - t.fpX / 2;
    const int y0 = std::max(0, pixelToTile(b.py) - t.fpY / 2);
    const int y1 = std::min(y0 + t.fpY - 1, c.map.height() - 1);
    for (int x = x0; x < x0 + t.fpX; ++x)
        for (int y = y0; y <= y1; ++y)
            if (c.map.inBounds(x, y)) c.map.at(x, y).cellType = CellType::DozedArea;

    // Size 1 also makes a Blast (an EMP ring, no hit-point damage; not modelled).
    const WeaponKind blast = t.explosionSize == 3 ? WeaponKind::BuildingExplosionBig
                           : t.explosionSize != 0 ? WeaponKind::BuildingExplosionMedium : WeaponKind::Other;
    if (blast != WeaponKind::Other) {
        if (const std::uint8_t wid = weaponIdOfKind(c.world, blast)) {
            Unit e;
            e.kind = UnitKind::Weapon;
            e.owner = b.owner;
            e.weapon = wid;
            e.px = b.px;
            e.py = b.py;
            e.impactX = b.px;
            e.impactY = b.py;
            e.shotAction = kWait; // detonates after a tick, centred on the structure
            e.actionTimer = 1;
            const UnitId eid = c.world.spawn(e);
            if (Unit* ep = c.world.get(eid)) setRef(c.world, ep->shotParent, id);
        }
    }

    // Rubble: each piece Rand(tiles left) for the n-th bulldozed tile (columns
    // left to right, rows top down), then Rand(4) for its graphic; commons first.
    int tilesLeft = t.fpX * (y1 - y0 + 1);
    auto place = [&](int pieces) {
        for (int p = 0; p < pieces && tilesLeft > 0; ++p) {
            for (int tries = 0; tries < 16; ++tries) {
                int n = static_cast<int>(c.rng.getRand(static_cast<std::uint32_t>(tilesLeft)));
                bool done = false;
                for (int x = x0; x < x0 + t.fpX && !done; ++x)
                    for (int y = y0; y <= y1 && !done; ++y)
                        if (c.map.inBounds(x, y) && c.map.at(x, y).cellType == CellType::DozedArea && n-- == 0) {
                            c.map.at(x, y).cellType = CellType::Rubble;
                            c.rng.getRand(4); // the rubble graphic (0x0048B870 / 0x0048B8D0)
                            done = true;
                        }
                if (done) break;
            }
            --tilesLeft;
        }
    };
    place(t.rubbleCommon);
    place(t.rubbleRare);
}

} // namespace

bool deathTurn(CombatContext& c, UnitId id) {
    Unit* u = c.world.get(id);
    if (!(u->flags & UnitFlag::IsDead)) return false;
    if (u->kind == UnitKind::Building && !(u->flags & UnitFlag::DeathStarted)) {
        // Its next turn starts the destroyed animation (UpdateAnimation 0x00408A20)
        // and a visual explosion sized to the footprint: 2 x 9 frames + 4 ticks
        // (op2-rs measured a 1x2 structure's; other sizes' frame counts are art data).
        u->flags |= UnitFlag::DeathStarted;
        Unit e;
        e.kind = UnitKind::Other;
        e.typeId = kExplosionType;
        e.px = u->px;
        e.py = u->py;
        e.actionTimer = 22;
        c.world.spawn(e);
        u = c.world.get(id);
    }
    if (u->actionTimer > 0) { --u->actionTimer; return true; }
    // ProcessForGameCycle case 0x2F: a vehicle whose hit points ran out explodes
    // (animation 0x87D, 26 ticks); a structure is razed (explodeStructure); then
    // it is deleted.
    const bool killed = u->damage >= c.world.type(u->typeId).maxHitPoints;
    const std::int32_t x = (u->px & ~31) + 16, y = (u->py & ~31) + 16;
    setRef(c.world, u->fireTarget, {});
    setRef(c.world, u->orderTarget, {});
    if (u->lastAttacker) c.world.releaseRef(UnitId{u->lastAttacker, 1});
    u->lastAttacker = 0;
    if (u->kind == UnitKind::Building) {
        const Unit b = *u;
        explodeStructure(c, id, b);
        c.world.destroy(id);
        return true;
    }
    c.world.destroy(id);
    if (killed) {
        Unit e;
        e.kind = UnitKind::Other;
        e.typeId = kExplosionType;
        e.px = x;
        e.py = y;
        e.actionTimer = 26;
        c.world.spawn(e);
    }
    return true;
}

void effectTurn(CombatContext& c, UnitId id) {
    Unit* e = c.world.get(id);
    if (--e->actionTimer <= 0) c.world.destroy(id);
}

void shotTurn(CombatContext& c, UnitId id) {
    Unit* s = c.world.get(id);
    const WeaponType* w = c.world.weaponOf(*s);
    if (!w) { c.world.destroy(id); return; }
    const WeaponKind kind = static_cast<WeaponKind>(w->kind);
    auto remove = [&] {
        Unit* sh = c.world.get(id);
        setRef(c.world, sh->shotParent, {});
        c.world.destroy(id);
    };

    // ProcessForGameCycle, command 0x36, when no action is running.
    if (s->shotAction == 0) {
        if (s->shotLastAction == kAimCoarse) {
            // DoFire: from the firer's pixel, at the (led) aim point.
            Unit* parent = c.world.get(s->shotParent);
            if (!alive(parent)) { remove(); return; }
            s->px = parent->px;
            s->py = parent->py;
            std::int32_t ax = s->impactX, ay = s->impactY;
            if (const Unit* t = c.world.get(s->shotTarget)) std::tie(ax, ay) = aimPoint(*parent, *t, kind);
            launch(c, *s, ax, ay, *w);
            if (beam(kind)) {
                s->actionTimer = 4;
            } else {
                s->startX = s->px;
                s->startY = s->py;
                s->flightSteps = std::max(3, octagonal(s->impactX - s->startX, s->impactY - s->startY) / 16 + 1);
                s->flightT = 0;
            }
            s->shotAction = kAimFine;
        } else if (s->shotLastAction == kAimFine) {
            s->shotAction = beam(kind) ? kWait : kFlight;
        } else if (!s->shotDamaged) {
            s->shotDamaged = 1;
            const Unit copy = *s;
            doDamage(c, copy);
            s = c.world.get(id);
            s->shotAction = kWait;
            s->actionTimer = lingerTicks(kind);
        } else {
            remove();
            return;
        }
    }

    // ProcessTimers: the action step.
    switch (s->shotAction) {
        case kAimCoarse:
        case kAimFine: {
            Unit* parent = c.world.get(s->shotParent);
            if (!parent) { remove(); return; }
            if (s->shotAction == kAimCoarse) {
                const Unit* t = c.world.get(s->shotTarget);
                if (t) { s->impactX = t->px; s->impactY = t->py; }
                s->rotation = bearing(s->impactX - parent->px, s->impactY - parent->py);
            }
            const int d = static_cast<std::int8_t>(static_cast<std::uint8_t>(s->rotation - parent->turret));
            if (iabs(d) < w->turretRate) {
                parent->turret = s->rotation;
                s->shotLastAction = s->shotAction;
                s->shotAction = 0;
            } else {
                parent->turret = static_cast<std::uint8_t>(d < 0 ? parent->turret - w->turretRate
                                                                 : parent->turret + w->turretRate);
            }
            break;
        }
        case kWait:
            if (--s->actionTimer <= 0) { s->shotLastAction = kWait; s->shotAction = 0; }
            break;
        case kFlight:
            if (++s->flightT <= s->flightSteps) {
                const std::int32_t left = s->flightT - s->flightSteps; // <= 0
                s->px = s->impactX + left * (s->impactX - s->startX) / s->flightSteps;
                s->py = s->impactY + left * (s->impactY - s->startY) / s->flightSteps;
            } else {
                s->shotLastAction = kFlight;
                s->shotAction = 0;
            }
            break;
        case kDelete:
            remove();
            break;
        default:
            break;
    }
}

} // namespace op2::sim
