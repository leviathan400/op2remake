#pragma once
#include <cstdint>

#include "op2/sim/map.h"
#include "op2/sim/world.h"

// =============================================================================
// Vehicle movement — driving tile centre to tile centre
// =============================================================================
//
// Measured against the original by op2-rs's harness oracle
// (harness/oracle/FINDINGS.md "Speed", "Turning", "Orders", "Two units",
// "Paths"); reimplemented here:
//
//   * A step from tile A to B takes T ticks, ts = kCellSpeed[cell][track], m =
//     the type's Move Points (vehicles.txt); StartNextTileStep 0x0044B760:
//       straight  T = m * (ts(A) + ts(B)) >> 10
//       diagonal  T = (m * 0x2D4 >> 9) * (ts(A) + ts(B)) >> 10     (x724/512 ~ sqrt 2)
//     plus damage * T / maxHP, at least 2. (Lynx, m 3, wheeled: fast 24,
//     medium1 30, slow1 39, dozed 18; diagonal fast 32.) Each moving axis goes
//     floor(2^21 / T) + 1 in 16.16 pixels per tick and the unit lands exactly
//     on B's centre after T ticks. No acceleration; steps run back to back.
//   * Turning: a turn of more than 16 is made in place on a tile centre, at the
//     type's Turn Rate per tick, the shorter way (coords.h turnTowards), and the
//     step starts the tick after (a 45-degree corner costs a Lynx 2 ticks);
//     a smaller one finishes while the unit moves.
//   * An order is taken in the cycle it arrives and acted on in the next; a new
//     order or a Stop doesn't interrupt the step in progress.
//   * A unit claims the tile it steps into when it starts the step; another
//     unit that wants it waits on its own centre, without turning.
//   * The route is OP2's coarse plan (pathfind.h): a greedy octant walk that
//     traces round obstacles. It is planned on the unit's turn and replanned
//     when it runs out short of the goal or meets a blocked tile; an empty plan
//     (nowhere nearer) ends the order.
//
// Not modelled: night slowdown, the RCC (AI) players' corridor search, bumping
// (Unit.CanBeBumpedBy: other vehicles never block a plan, a held tile makes
// the mover wait), x-wrap on 512-wide maps.

namespace op2::sim {

// Track types, in the game's order (vehicles.txt "Track Type": L, W, M, T).
namespace Track {
    inline constexpr std::uint8_t Legged = 0, Wheeled = 1, Miner = 2, Tracked = 3, Hover = 4;
}
// The track type for a vehicles.txt letter (L/W/M/T/H); Wheeled for anything else.
std::uint8_t trackFromLetter(char c) noexcept;

// Cell speeds, [cell type][track]: the cost of crossing a tile (lower is
// faster), 0 = can't enter. The game fills this table at start-up (CellTypeInfo
// 0x4DEBC0, 36-byte entries, speeds at +0x10..+0x20, by the routine at
// 0x40B5F0); values as op2-rs's harness reads them from the running game
// (gamedata.json `trackSpeed`), rows 2 and 3 checked against the routine's
// immediates. Columns: Legged, Wheeled, Miner, Tracked, Hover.
extern const std::int32_t kCellSpeed[32][5];

inline std::int32_t cellSpeed(CellType c, std::uint8_t track) noexcept {
    return kCellSpeed[static_cast<int>(c) & 31][track < 5 ? track : Track::Wheeled];
}

// Ticks for one step (the formula above); at least 1.
std::int32_t stepTicks(std::int32_t moveSpeed, std::int32_t speedFrom, std::int32_t speedTo, bool diagonal) noexcept;

// Per-axis velocity of a T-tick step, 16.16 pixels per tick.
constexpr std::int32_t stepVelocity(std::int32_t ticks) noexcept { return (1 << 21) / ticks + 1; }

// Unit.action while driving (0 = done/standing).
namespace Action {
    inline constexpr std::uint8_t Done = 0, Move = 1;
}

// Start a Move order to `goal`; the route is planned on the unit's next turn on
// a tile centre (mid-step, once the step ends).
void orderMove(Unit& u, int goalX, int goalY) noexcept;
// Stop: drop the route; a step in progress finishes.
void orderStop(Unit& u) noexcept;

// One unit's movement on its turn (Game::step calls it in processing order).
void moveUnit(World& world, const Map& map, UnitId id, Unit& u);

} // namespace op2::sim
