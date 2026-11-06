#include "op2/sim/pathfind.h"

#include <climits>

#include "op2/sim/coords.h"

namespace op2::sim {

namespace {

struct Pt {
    int x, y;
    bool operator==(const Pt&) const = default;
};

Pt step(Pt p, int dir) { return {p.x + kDirSteps[dir & 7].dx, p.y + kDirSteps[dir & 7].dy}; }

// 16.16 distance to the goal, max(|dx|,|dy|) + min/2 (0x0043FDE0).
std::int32_t distance16(Pt p, Pt goal) {
    const std::int32_t ax = (goal.x > p.x ? goal.x - p.x : p.x - goal.x) << 16;
    const std::int32_t ay = (goal.y > p.y ? goal.y - p.y : p.y - goal.y) << 16;
    return ax >= ay ? ax + ay / 2 : ay + ax / 2;
}

// One trace step (0x0043FCC0): rotate the heading by (8 - rotation) until the
// next tile is passable, step there, then turn back by `rotation` (twice after
// a diagonal). Returns the heading moved in, or -1 if every neighbour is blocked
// (the exe would spin forever there).
int traceStep(int& heading, Pt& p, int rotation, const Passable& passable) {
    for (int tries = 0; tries < 8; ++tries) {
        const Pt next = step(p, heading);
        if (passable(next.x, next.y)) {
            const int moved = heading;
            p = next;
            heading = (heading & 1) ? (heading + 2 * rotation) & 7 : (heading + rotation) & 7;
            return moved;
        }
        heading = (heading + 8 - rotation) & 7;
    }
    return -1;
}

// Loop closure (0x0043FE90): the trace is back on the blocked tile `cur` (after
// a diagonal step, or on the tile diagonally next to it in direction
// lastStep + 3, which then moves the trace onto `cur` and turns the step), and
// every neighbour of `cur` from the blocked heading round (counter-clockwise for
// rotation 7) to the tile the trace came from is impassable.
bool closedOnStart(Pt cur, int blockedHeading, Pt& tracePos, std::uint8_t& lastStep, Pt prevPos, int rotation,
                   int headingOffset, const Passable& passable) {
    bool adjust = false;
    if ((lastStep & 1) == 0) {
        if (!(tracePos == cur)) return false;
    } else {
        const int dy = cur.y - tracePos.y, dx = cur.x - tracePos.x;
        if (dy < -1 || dy > 1 || dx < -1 || dx > 1) return false;
        if (!(dx == 0 && dy == 0)) {
            const int u = (headingOffset + lastStep) & 7;
            if (kDirSteps[u].dx != dx || kDirSteps[u].dy != dy) return false;
            adjust = true;
        }
    }
    int h = blockedHeading;
    for (int k = 0;; ++k) {
        if (k == 8) return false;
        h = (h + rotation) & 7;
        const Pt n = step(cur, h);
        if (n == prevPos) break;
        if (passable(n.x, n.y)) return false;
    }
    if (adjust) {
        tracePos = cur;
        lastStep = static_cast<std::uint8_t>((lastStep - rotation) & 7);
    }
    return true;
}

struct Measure { std::int32_t cost = INT32_MAX; int index = 0; };

// The detour measure (0x00440030): for each traced tile, its distance to the
// goal plus the collapsed length of the trace up to it (32 cardinal, 45
// diagonal); the lowest wins, the first on a tie; stops at the goal.
Measure measure(const std::vector<std::uint8_t>& steps, const std::vector<Pt>& pos, Pt goal) {
    static constexpr std::int32_t kLen[2] = {32, 45};
    Measure best;
    std::vector<std::uint8_t> c;
    std::vector<std::int32_t> len; // len[j]: length of the first j+1 collapsed steps
    for (std::size_t i = 0; i < steps.size(); ++i) {
        c.push_back(steps[i]);
        const int m = collapseTurns(c.data(), static_cast<int>(c.size()));
        c.resize(static_cast<std::size_t>(m));
        len.resize(static_cast<std::size_t>(m));
        const std::int32_t l = m == 0 ? 0 : (m >= 2 ? len[m - 2] : 0) + kLen[c[m - 1] & 1];
        if (m > 0) len[m - 1] = l;
        const std::int32_t d = distance16(pos[i], goal);
        if (l + d < best.cost) best = {l + d, static_cast<int>(i)};
        if (d == 0) break;
    }
    return best;
}

// The same, walking a closed trace the other way round (0x00440190): from the
// last step back to the second, each step reversed, positions one behind.
Measure measureReversed(const std::vector<std::uint8_t>& steps, const std::vector<Pt>& pos, Pt goal) {
    static constexpr std::int32_t kLen[2] = {32, 45};
    Measure best;
    std::vector<std::uint8_t> c;
    std::vector<std::int32_t> len;
    for (int i = static_cast<int>(steps.size()) - 1; i >= 1; --i) {
        c.push_back(static_cast<std::uint8_t>((steps[i] + 4) & 7));
        const int m = collapseTurns(c.data(), static_cast<int>(c.size()));
        c.resize(static_cast<std::size_t>(m));
        len.resize(static_cast<std::size_t>(m));
        const std::int32_t l = m == 0 ? 0 : (m >= 2 ? len[m - 2] : 0) + kLen[c[m - 1] & 1];
        if (m > 0) len[m - 1] = l;
        const std::int32_t d = distance16(pos[i - 1], goal);
        if (l + d < best.cost) best = {l + d, i};
        if (d == 0) break;
    }
    return best;
}

void append(std::vector<std::uint8_t>& out, int dir) {
    out.push_back(static_cast<std::uint8_t>(dir & 7));
    out.resize(static_cast<std::size_t>(collapseTurns(out.data(), static_cast<int>(out.size()))));
}

} // namespace

int direction8(int dx, int dy) noexcept {
    if (dx == 0) return dy >= 0 ? 2 : 6;
    const std::int64_t s = (static_cast<std::int64_t>(dy) * 256) / dx; // truncates, as idiv
    if (s >= 0x26A) return dx >= 0 ? 2 : 6;
    if (s <= -0x26A) return dx < 0 ? 2 : 6;
    if (s > 0x69) return dx >= 0 ? 1 : 5;
    if (s > -0x6A) return dx >= 0 ? 0 : 4;
    return dx >= 0 ? 7 : 3;
}

int collapseTurns(std::uint8_t* steps, int n) noexcept {
    int i = n - 1;
    while (i >= 1) {
        const int cur = steps[i], prev = steps[i - 1];
        const int rel = (cur - prev) & 7;
        int merged;
        if ((prev & 1) == 0) { // after a cardinal step
            if (rel == 2 || rel == 3) merged = cur - 1;
            else if (rel == 5 || rel == 6) merged = cur + 1;
            else if (rel == 4) { i -= 2; continue; }
            else break;
        } else {               // after a diagonal step
            if (rel == 3) merged = cur - 2;
            else if (rel == 5) merged = cur + 2;
            else if (rel == 4) { i -= 2; continue; }
            else break;
        }
        --i;
        steps[i] = static_cast<std::uint8_t>(merged & 7);
    }
    return i + 1;
}

CoarsePlan planCoarsePath(int startX, int startY, int goalX, int goalY, const ClipRect& clip,
                          const Passable& passable, int maxSteps) {
    CoarsePlan plan;
    std::vector<std::uint8_t>& out = plan.steps;
    const Pt goal{goalX, goalY};
    Pt cur{startX, startY};
    bool traced = false; // the one open detour has been used

    for (int guard = 0; guard < 4 * kMaxPlanSteps; ++guard) {
        // Greedy walk towards the goal.
        int d = 0;
        while (!(cur == goal) && static_cast<int>(out.size()) < maxSteps) {
            d = direction8(goal.x - cur.x, goal.y - cur.y);
            const Pt next = step(cur, d);
            if (!passable(next.x, next.y)) break;
            append(out, d);
            cur = next;
        }
        if (cur == goal || static_cast<int>(out.size()) >= maxSteps || traced) break;
        const int budget = maxSteps - static_cast<int>(out.size());

        // Trace A: obstacle on the left.
        std::vector<std::uint8_t> a;
        std::vector<Pt> pa;
        int h = (d + 1) & 7;
        Pt p = cur;
        bool closed = false;
        while (static_cast<int>(a.size()) <= budget) {
            const int moved = traceStep(h, p, 7, passable);
            if (moved < 0) break;
            a.push_back(static_cast<std::uint8_t>(moved));
            pa.push_back(p);
            if ((p.y == clip.y1 && h == 7) || (p.y == clip.y2 && h == 3) || (p.x == clip.x1 && h == 5) ||
                (p.x == clip.x2 && h == 1)) break;
            if (a.size() >= 3 && closedOnStart(cur, d, p, a.back(), pa[a.size() - 2], 7, 3, passable)) {
                closed = true;
                break;
            }
        }

        Pt target = cur;
        bool keepTracing = false;
        const std::int32_t d0 = distance16(cur, goal);
        if (closed) {
            const Measure f = measure(a, pa, goal);
            const Measure r = measureReversed(a, pa, goal);
            if (f.cost < d0 || r.cost < d0) {
                if (r.cost < f.cost) {
                    for (int i = static_cast<int>(a.size()) - 1; i >= r.index; --i) append(out, a[i] + 4);
                    target = pa[r.index - 1];
                } else {
                    for (int i = 0; i <= f.index; ++i) append(out, a[i]);
                    target = pa[f.index];
                }
                keepTracing = true; // a closed loop doesn't use up the open detour
            }
        } else {
            if (static_cast<int>(a.size()) > budget) { a.resize(budget); pa.resize(budget); }
            // Trace B: the other way round, obstacle on the right.
            std::vector<std::uint8_t> b;
            std::vector<Pt> pb;
            h = (d - 1) & 7;
            p = cur;
            while (static_cast<int>(b.size()) < budget) {
                const int moved = traceStep(h, p, 1, passable);
                if (moved < 0) break;
                if ((p.y == clip.y1 && h == 5) || (p.y == clip.y2 && h == 1) || (p.x == clip.x1 && h == 3) ||
                    (p.x == clip.x2 && h == 7)) break; // this step isn't kept
                b.push_back(static_cast<std::uint8_t>(moved));
                pb.push_back(p);
            }
            const Measure ma = measure(a, pa, goal);
            const Measure mb = measure(b, pb, goal);
            if (ma.cost < mb.cost || (ma.cost == mb.cost && ma.index < mb.index)) {
                if (ma.cost < d0) {
                    for (int i = 0; i <= ma.index; ++i) append(out, a[i]);
                    target = pa[ma.index];
                }
            } else if (mb.cost < d0) {
                for (int i = 0; i <= mb.index; ++i) append(out, b[i]);
                target = pb[mb.index];
            }
        }
        if (target == cur) break;
        if (!keepTracing) traced = true;
        cur = target;
    }
    plan.reachesGoal = cur == goal;
    return plan;
}

} // namespace op2::sim
