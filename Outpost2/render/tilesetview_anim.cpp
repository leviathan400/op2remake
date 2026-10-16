#include "tilesetview.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPoint>
#include <QQuickWindow>
#include <QSettings>
#include <QString>
#include <QtGlobal>

#include <cmath>

#include "logger.h"
#include "missionloader.h"
#include "op2/maps/maploader.h"

namespace {
constexpr int T = TilesetManager::kTileSize;

// OP2 plays building animations at a *uniform* rate: SetAnimation() holds every
// frame for a constant `animationDelay` (total = numFrames * animationDelay) — it
// does NOT use the per-frame art-delay bytes for operational loops. We advance at
// most 1 frame per game tick, but also cap the *loop* length: a short group
// (e.g. 10 frames) would otherwise cycle 4x/sec and look frantic, while long ones
// (SF 50f, Command Center 87f) look right at 1 frame/tick. So no loop plays faster
// than kMinLoopTicks game ticks (~1.25s at speed 10); longer groups keep 1/tick.
constexpr double kMinLoopTicks = 50.0;
} // namespace

// Advance the looping map sprites one game tick (vents plume, wreckage flickers,
// markers pulse). Static sprites (delay 0) never advance. Independent of the
// building rest-gap cycle — these animate continuously.
void TilesetView::advanceSprites(int gameTicks) {
    if (placedSprites_.empty() || gameTicks <= 0) return;
    // Gaia / non-player objects (vents, ore beacons, wreckage, markers) animate at a
    // calmer pace than buildings — divide the advance rate so a fumarole plumes gently
    // and an ore deposit sparkles slowly, rather than strobing. Scaled by ticks-this-
    // frame so the pace still tracks game speed.
    constexpr double kGaiaSlowdown = 6.0;
    bool changed = false;
    for (PlacedSprite& ps : placedSprites_) {
        const int n = static_cast<int>(ps.frames.size());
        if (n < 2) continue;
        const int delay = ps.delays[ps.frame % n];
        if (delay <= 0) continue;                 // static frame
        ps.frameAccum += static_cast<double>(animTicksPerGameTick_) * gameTicks / (delay * kGaiaSlowdown);
        while (ps.frameAccum >= 1.0) {
            ps.frameAccum -= 1.0;
            ps.frame = (ps.frame + 1) % n;
            changed = true;
        }
    }
    if (changed) update();
}

// Idle military units slowly rotate their TURRET to look "alive" — only the turret
// moves; the hull stays put. This mirrors OP2's idle routine (Weapon::AnimateIdleTurret
// @0x4A7DD0, gated by no-target in @0x4A7650): it's a mode-driven WANDER, not a clean
// spin. Each "beat" a slowly-changing pseudo-random mode picks step-CW / step-CCW /
// drift-toward-hull / drift-toward-random / hold. We work in 16-direction space (OP2
// tracks 256 but only renders 16). Reproduced deterministically (no per-unit sim state):
//   beat   : ((tick ^ idx) & 7) == 0           — ~every 8th tick, phased per unit
//   mode   : (idx ^ (tick >> 8)) & 0xF         — changes slowly
//   step is ±1 of 16; random uses a cheap per-unit hash (showcase, not the sim LFSR).
void TilesetView::advanceUnitTurrets(int gameTicks) {
    // The idle-turret beat is quantised on animTick_ (already advanced by gameTicks in
    // advanceAnimation), so the wander cadence tracks game speed without extra scaling.
    Q_UNUSED(gameTicks);
    if (placedUnits_.empty()) return;
    auto stepToward = [](int cur, int target) {            // ±1 toward target (mod 16)
        const int d = ((target - cur + 8) & 0xF) - 8;      // signed shortest delta
        if (d == 0) return cur;
        return (cur + (d > 0 ? 1 : 15)) & 0xF;
    };
    bool changed = false;
    int i = 0;
    for (PlacedUnit& pu : placedUnits_) {
        const int idx = i++;
        if (!pu.turretDef) continue;
        if (pu.isGuardPost && !pu.active) continue; // an off GuardPost doesn't wander
        // OP2 keys the beat/mode on the unit's POOL index, which is scattered (units
        // spawn at different times). Our parade indices are sequential, so neighbours
        // (a showcase column shares one hull facing) would drift in unison. Scatter the
        // index with an integer hash so each unit gets an independent phase + mode.
        unsigned h = static_cast<unsigned>(idx);
        h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
        if (((static_cast<unsigned>(animTick_) + (h & 7)) & 7) != 0) continue; // this unit's beat
        const int mode = static_cast<int>((h ^ static_cast<unsigned>(animTick_ >> 8)) & 0xF);
        // A vehicle re-centers its turret on its HULL (the OP2 look — turret returns to
        // forward). A GuardPost has no hull, and a whole map of them usually shares one
        // facing, so re-centering them all on that facing makes a row look like it's
        // rotating in lockstep. Give each GuardPost a STABLE per-unit idle "home"
        // (different hash bits than the beat/mode) so a row settles at varied angles.
        const int home = pu.isGuardPost ? static_cast<int>((h >> 11) & 0xF) : pu.bodyRot;
        const int before = pu.turretRot;
        switch (mode) {
            case 1:  pu.turretRot = (pu.turretRot + 1) & 0xF; break;   // CW
            case 8:  pu.turretRot = (pu.turretRot + 15) & 0xF; break;  // CCW
            case 6: case 13: {                                         // toward random dir
                const int r = static_cast<int>((h + static_cast<unsigned>(animTick_ >> 8) * 2654435761u) & 0xF);
                pu.turretRot = stepToward(pu.turretRot, r);
                break;
            }
            case 0: case 3: case 5: case 7: case 10: case 12: case 14: // drift toward idle home
                pu.turretRot = stepToward(pu.turretRot, home);
                break;
            default: break;                                            // hold (2,4,9,11,15)
        }
        if (pu.turretRot == before) continue;
        const int g = pu.turretDef->groups[pu.turretRot];
        blitUnitSprite(g, &pu.turret, &pu.tAnchorX, &pu.tAnchorY);
        int ox = 0, oy = 0, tdx = 0, tdy = 0;
        // Vehicles mount the turret on the chassis frame's optional offset. GuardPosts have
        // NO chassis (the combined sprite is self-contained) — adding an offset there would
        // jitter the sprite as it rotates, so skip it when there's no chassis image. This
        // MUST match placeGuardPost's placement seating (upX − groupDisplacement) exactly.
        if (!pu.chassis.isNull()) {
            const int mountFrame = pu.chassisFrame >= 0 ? pu.chassisFrame
                                                        : art_.groupFrameStart(pu.chassisGroup);
            art_.frameTurretOffset(mountFrame, &ox, &oy);
        }
        art_.groupDisplacement(g, &tdx, &tdy);
        pu.tOriginX = pu.seatUpX + ox - tdx;
        pu.tOriginY = pu.seatUpY + oy - tdy;
        changed = true;
    }
    if (changed) update();
}

void TilesetView::advanceAnimation(int gameTicks) {
    if (gameTicks <= 0) return; // a frame with no game tick elapsed (low speed) — hold
    animTick_ += gameTicks;
    if (ambientUnitEnabled_) advanceUnitTurrets(gameTicks);
    if (ambientResourceEnabled_) advanceSprites(gameTicks); // vents/beacons/markers loop
    if (!ambientStructureEnabled_ || placed_.empty()) return;
    // AnimationSpeed=7 (the calibrated default) → base scale 1.0. Raise/lower to
    // speed up or slow down every building together. Multiply by the ticks elapsed
    // this frame so building animation tracks game speed.
    const double speedScale = (animTicksPerGameTick_ / 7.0) * gameTicks;
    bool advanced = false;
    for (PlacedBuilding& pb : placed_) {
        if (pb.anims.empty() || pb.playOrder.empty()) continue;
        if (!pb.active) continue; // Idle status: hold the base frame, no animation

        // Resting between plays: hold the rest frame for the gap, then start the
        // next animation in the rotation. This is the pause before repeating.
        if (pb.gapRemaining > 0) {
            pb.gapRemaining = qMax(0, pb.gapRemaining - gameTicks);
            continue;
        }

        const int n = static_cast<int>(pb.anims[pb.curAnim].frames.size());
        if (n == 0) continue;

        // Advance the current animation (no loop faster than kMinLoopTicks, scaled by
        // ticks-this-frame). When it completes one pass, rest for animGapTicks_ and
        // advance to the next animation in playOrder (Normal ⇄ ambients).
        const double framesPerTick = qMin(1.0, n / kMinLoopTicks) * speedScale;
        pb.frameAccum += framesPerTick;
        while (pb.frameAccum >= 1.0) {
            pb.frameAccum -= 1.0;
            pb.frame = (pb.frame + 1) % n;
            advanced = true;
            if (pb.frame == 0) { // finished this animation -> rest, then next
                pb.frameAccum = 0.0;
                pb.gapRemaining = animGapTicks_;
                pb.playIdx = (pb.playIdx + 1) % static_cast<int>(pb.playOrder.size());
                pb.curAnim = pb.playOrder[pb.playIdx];
                break;
            }
        }
    }
    if (advanced) update();
}
