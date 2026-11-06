#include "op2/sim/movement.h"

#include "op2/sim/command.h"
#include "op2/sim/coords.h"
#include "op2/sim/pathfind.h"

namespace op2::sim {

const std::int32_t kCellSpeed[32][5] = {
    {3584, 4096, 4096, 4096, 0},       // 0 Fast Passible 1
    {0, 0, 0, 0, 0},                   // 1 Impassible 2
    {5632, 6656, 6656, 6656, 0},       // 2 Slow Passible 1
    {6656, 6144, 6144, 5632, 0},       // 3 Slow Passible 2
    {5120, 5120, 5120, 4608, 0},       // 4 Medium Passible 1
    {4608, 5632, 5632, 5120, 0},       // 5 Medium Passible 2
    {0, 0, 0, 0, 0},                   // 6 Impassible 1
    {4096, 4096, 4096, 4096, 0},       // 7 Fast Passible 2
    {0, 0, 0, 0, 0},                   // 8 North Cliffs
    {0, 0, 0, 0, 0},                   // 9 Cliffs, high side
    {0, 0, 0, 0, 0},                   // 10 Cliffs, low side
    {0, 0, 6656, 0, 0},                // 11 Vents and Fumaroles (miners only)
    {512, 512, 512, 512, 512},         // 12-20 padding types, never on a map
    {341, 512, 512, 512, 512},
    {1024, 0, 0, 1024, 0},
    {0, 0, 0, 0, 0},
    {512, 512, 512, 512, 512},
    {512, 512, 512, 512, 512},
    {512, 512, 512, 512, 512},
    {512, 512, 512, 512, 512},
    {512, 512, 512, 512, 512},
    {3072, 3072, 3072, 3072, 3072},    // 21 Dozed Area
    {4096, 4096, 4096, 4096, 4096},    // 22 Rubble
    {0, 0, 0, 0, 0},                   // 23 Normal Wall
    {0, 0, 0, 0, 0},                   // 24 Microbe Wall
    {0, 0, 0, 0, 0},                   // 25 Lava Wall
    {4096, 4096, 4096, 4096, 4096},    // 26-31 Tube 0-5
    {4096, 4096, 4096, 4096, 4096},
    {4096, 4096, 4096, 4096, 4096},
    {4096, 4096, 4096, 4096, 4096},
    {4096, 4096, 4096, 4096, 4096},
    {4096, 4096, 4096, 4096, 4096},
};

std::uint8_t trackFromLetter(char c) noexcept {
    switch (c) {
        case 'L': case 'l': return Track::Legged;
        case 'M': case 'm': return Track::Miner;
        case 'T': case 't': return Track::Tracked;
        case 'H': case 'h': return Track::Hover;
        default: return Track::Wheeled;
    }
}

namespace {

constexpr std::uint8_t kCmdAttackObj = 0x24;

int dirOf(int dx, int dy) noexcept {
    for (int d = 0; d < 8; ++d)
        if (kDirSteps[d].dx == dx && kDirSteps[d].dy == dy) return d;
    return 0;
}

int sign(int v) noexcept { return (v > 0) - (v < 0); }

// The structure holds the tile (its dock tile is free for vehicles).
bool inFootprint(const World& world, const Unit& o, int x, int y) { return world.holdsTile(o, x, y); }

// The planner's tile test (IsTilePassable 0x004383C0, as far as it's
// modelled): on the map, ground this track can cross, and no structure.
// Vehicles don't block a plan here (OP2 asks Unit.CanBeBumpedBy, not modelled);
// a held tile makes the mover wait instead (moveUnit).
bool plannable(const World& world, const Map& map, std::uint8_t track, int x, int y) {
    if (!map.inBounds(x, y) || cellSpeed(map.at(x, y).cellType, track) == 0) return false;
    bool structure = false;
    world.forEach([&](UnitId, const Unit& o) {
        if (!structure && o.kind == UnitKind::Building) structure = inFootprint(world, o, x, y);
    });
    return !structure;
}

// Whether another vehicle holds the tile: a standing one its tile, a stepping
// one the tile it is stepping into.
bool heldByVehicle(const World& world, int x, int y, UnitId self) {
    bool held = false;
    world.forEach([&](UnitId id, const Unit& o) {
        if (held || id == self || o.kind != UnitKind::Vehicle) return;
        held = o.stepTicks > 0 ? (o.destX == x && o.destY == y) : (pixelToTile(o.px) == x && pixelToTile(o.py) == y);
    });
    return held;
}

void endOrder(Unit& u) noexcept {
    u.command = 0; // ctNop
    u.action = Action::Done;
    u.planLen = u.planPos = 0;
}

int planStep(const Unit& u, int i) noexcept {
    const std::uint8_t b = u.plan[static_cast<std::size_t>(i / 2)];
    return (i & 1) ? (b >> 4) : (b & 0xF);
}

void storePlan(Unit& u, const std::vector<std::uint8_t>& steps) noexcept {
    u.plan.fill(0);
    const std::size_t n = steps.size() < static_cast<std::size_t>(kMaxPlanSteps) ? steps.size() : kMaxPlanSteps;
    for (std::size_t i = 0; i < n; ++i)
        u.plan[i / 2] = static_cast<std::uint8_t>(u.plan[i / 2] | ((steps[i] & 7) << ((i & 1) ? 4 : 0)));
    u.planLen = static_cast<std::uint16_t>(n);
    u.planPos = 0;
}

// One tick of the step in progress (a small turn finishes while moving); lands
// on the destination centre on its last tick, sub-pixels reset (0x8000).
void advance(Unit& u, int turnRate) noexcept {
    const std::uint8_t heading = headingOfDir(dirOf(sign(u.vx), sign(u.vy)));
    if (u.rotation != heading) u.rotation = turnTowards(u.rotation, heading, turnRate);
    const std::int32_t ax = static_cast<std::int32_t>(u.subX) + u.vx;
    const std::int32_t ay = static_cast<std::int32_t>(u.subY) + u.vy;
    u.px += ax >> 16;
    u.py += ay >> 16;
    u.subX = static_cast<std::uint16_t>(ax & 0xFFFF);
    u.subY = static_cast<std::uint16_t>(ay & 0xFFFF);
    if (--u.stepTicks == 0) {
        u.px = tileCentrePx(u.destX);
        u.py = tileCentrePx(u.destY);
        u.subX = u.subY = kSubPixelRest;
        u.vx = u.vy = 0;
    }
}

} // namespace

std::int32_t stepTicks(std::int32_t moveSpeed, std::int32_t speedFrom, std::int32_t speedTo, bool diagonal) noexcept {
    // StartNextTileStep 0x0044B760: diagonal base x 0x2D4 >> 9 (about sqrt 2); at least 2 ticks.
    const std::int64_t base = diagonal ? (static_cast<std::int64_t>(moveSpeed) * 0x2D4) >> 9 : moveSpeed;
    const std::int64_t t = (base * (static_cast<std::int64_t>(speedFrom) + speedTo)) >> 10;
    return t < 2 ? 2 : static_cast<std::int32_t>(t);
}

void orderMove(Unit& u, int goalX, int goalY) noexcept {
    // The route is planned on the unit's next turn on a tile centre (mid-step,
    // from the tile it is stepping into: the step isn't interrupted).
    u.goalX = goalX;
    u.goalY = goalY;
    u.planLen = u.planPos = 0;
    u.command = static_cast<std::uint8_t>(Opcode::ctMoMove);
    u.action = Action::Move;
    u.flags |= UnitFlag::OrderPending;
}

void orderStop(Unit& u) noexcept {
    endOrder(u);
    u.flags |= UnitFlag::OrderPending;
}

void moveUnit(World& world, const Map& map, UnitId id, Unit& u) {
    if (u.kind != UnitKind::Vehicle || (u.flags & UnitFlag::IsDead)) return;
    const UnitType& t = world.type(u.typeId);
    const int turnRate = t.turnRate > 0 ? t.turnRate : 1;
    if (u.stepTicks > 0) { advance(u, turnRate); return; } // a step always runs to its end
    if (u.flags & UnitFlag::OrderPending) {                 // taken this cycle, acted on next
        u.flags &= ~UnitFlag::OrderPending;
        return;
    }
    // A Move order, or an Attack order's chase (combat.h), drives to the goal.
    const bool chase = u.command == kCmdAttackObj;
    if (u.command != Opcode::ctMoMove && !(chase && u.action == Action::Move)) return;
    auto stop = [&] {
        if (chase) { u.action = Action::Done; u.planLen = u.planPos = 0; } // the Attack order goes on
        else endOrder(u);
    };

    const int fromX = pixelToTile(u.px), fromY = pixelToTile(u.py);
    if (u.planPos >= u.planLen) {
        if (fromX == u.goalX && fromY == u.goalY) { stop(); return; } // arrived
        // Plan (or replan when a route ran out short of the goal).
        const ClipRect clip{0, 0, map.width() - 1, map.height() - 1};
        const CoarsePlan plan = planCoarsePath(fromX, fromY, u.goalX, u.goalY, clip,
                                               [&](int x, int y) { return plannable(world, map, t.track, x, y); });
        if (plan.steps.empty()) { stop(); return; } // nowhere nearer to go
        storePlan(u, plan.steps);
    }

    const int dir = planStep(u, u.planPos);
    const int toX = fromX + kDirSteps[dir].dx, toY = fromY + kDirSteps[dir].dy;
    if (!plannable(world, map, t.track, toX, toY)) {
        u.planLen = u.planPos = 0; // the route is blocked: replan on the next turn
        return;
    }
    if (heldByVehicle(world, toX, toY, id)) return; // wait on this centre, without turning

    // A turn of more than 16 is made wholly in place; the step starts the tick
    // after it ends. A smaller one finishes while moving.
    const std::uint8_t heading = headingOfDir(dir);
    const int delta = headingDelta(u.rotation, heading);
    if (delta != 0 && (delta > 16 || delta < -16 || (u.flags & UnitFlag::TurnInPlace))) {
        u.flags |= UnitFlag::TurnInPlace;
        u.rotation = turnTowards(u.rotation, heading, turnRate);
        return;
    }
    u.flags &= ~UnitFlag::TurnInPlace;

    const bool diagonal = (dir & 1) != 0;
    std::int32_t ticks = stepTicks(t.moveSpeed, cellSpeed(map.at(fromX, fromY).cellType, t.track),
                                   cellSpeed(map.at(toX, toY).cellType, t.track), diagonal);
    if (t.maxHitPoints > 0 && u.damage > 0) ticks += u.damage * ticks / t.maxHitPoints; // damaged units are slower
    const std::int32_t v = stepVelocity(ticks);
    ++u.planPos;
    u.destX = toX;
    u.destY = toY;
    u.vx = kDirSteps[dir].dx * v;
    u.vy = kDirSteps[dir].dy * v;
    u.stepTicks = ticks;
    advance(u, turnRate); // the step's first tick
}

} // namespace op2::sim
