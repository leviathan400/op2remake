#pragma once
#include <cstdint>
#include <functional>
#include <vector>

// =============================================================================
// Coarse path planner — OP2's greedy walk with obstacle tracing
// =============================================================================
//
// What a non-RCC player's Move order plans (PathFinder.PlanCoarsePath
// 0x0043F6C0, called from PlanPath 0x00440300). Learned from the op2kb
// (docs/engine/PATHFINDER*.md and the decompile); reimplemented here:
//
//   * Greedy walk: from the current tile, step in the octant towards the goal
//     (`direction8`, GetDirection8FromDelta 0x0045C680) while that tile is
//     passable; every step goes through `collapseTurns` (0x0043FD60).
//   * At a block, trace the obstacle with it on the left (TraceObstacleStep
//     0x0043FCC0, start heading d+1, rotation 7) until a clip edge, the step
//     budget, or (from the 3rd step) the trace closes on the blocked tile
//     (HasTraceClosedOnStart 0x0043FE90).
//       - Closed: measure the loop both ways round (MeasureDetour 0x00440030,
//         MeasureDetourReversed 0x00440190), splice the better in, walk on.
//       - Open: trace the other way (heading d-1, rotation 1), measure both,
//         splice the better in; after this one open detour the next block ends
//         the plan.
//     The measure is distance to the goal (16.16 max + min/2, 0x0043FDE0) plus
//     the collapsed length (32 per cardinal, 45 per diagonal step), so it picks
//     the traced tile nearest the goal; a detour is taken only if that tile is
//     nearer than the blocked one.
//   * Passability is boolean (terrain costs don't shape the route), up to 600
//     steps.
//
// Not here: the short straight walk for distance^2 < 5 (it gives the same
// steps as the greedy walk when it succeeds) and the RCC players' corridor
// search (0x00440770/0x00441360), which AI players use instead.

namespace op2::sim {

inline constexpr int kMaxPlanSteps = 600;

// The 8-way direction (0 = E, clockwise) of a delta by integer slope, as
// GetDirection8FromDelta: slope = (dy << 8) / dx against tan 22.5 / 67.5 x 256.
int direction8(int dx, int dy) noexcept;

// Merge the last two steps of `steps[0..n)` while they turn 90-135 degrees
// (into one rotated step) or reverse (both dropped); returns the new count.
int collapseTurns(std::uint8_t* steps, int n) noexcept;

// The planner's tile test: whether the unit can be planned onto (x, y).
using Passable = std::function<bool(int x, int y)>;

// Clip rectangle, inclusive tiles; the traces stop on its edges.
struct ClipRect { int x1, y1, x2, y2; };

struct CoarsePlan {
    std::vector<std::uint8_t> steps; // direction codes 0..7
    bool reachesGoal = false;        // the walk ends on the goal tile
};

CoarsePlan planCoarsePath(int startX, int startY, int goalX, int goalY, const ClipRect& clip,
                          const Passable& passable, int maxSteps = kMaxPlanSteps);

} // namespace op2::sim
