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

namespace { constexpr int T = TilesetManager::kTileSize; } // namespace

// Build one fully-animated building at a given top-left tile and append it to placed_.
// Shared by BOTH the catalog showcase (placeAllBuildings, via placeVariant) and the
// mission loader (placeFromWorld), so a building loaded from a .opm animates exactly
// like one in the parade. Resolves the Normal anim, up to two ambient anims, an optional
// floor underlay, the interleaved play order, and a staggered first rest. The caller
// supplies the resolved .prt groups (per faction / per special-case) and the tile.
void TilesetView::placeBuildingInstance(const BuildingDef& def, int normalGroup,
                                        const QString& label, int tileX, int tileY,
                                        int floorGroup, bool lastFrameOnly, int amb1, int amb2) {
    if (normalGroup < 0 || normalGroup >= art_.groupCount()) return;
    PlacedBuilding pb;
    pb.fpX = def.fpX; pb.fpY = def.fpY; pb.name = label;
    pb.tileX = tileX; pb.tileY = tileY;

    // One Anim from a .prt group, seated via the group's pixel displacement: the frame's
    // logical origin (0,0) maps to (footprint-centre-pixel − displacement). lastFrameOnly
    // keeps just the final frame (static normal, e.g. the garage's built look).
    auto buildAnim = [&](int group, bool onlyLast) -> Anim {
        Anim a;
        const int gfs = art_.groupFrameStart(group);
        const int gfc = art_.groupFrameCount(group);
        if (gfs < 0 || gfc <= 0) return a;
        const int first = onlyLast ? gfc - 1 : 0;
        for (int f = first; f < gfc; ++f) {
            int ax = 0, ay = 0;
            QImage img = art_.frameToQImage(gfs + f, &ax, &ay, shadowsEnabled_);
            if (!img.isNull()) {
                a.frames.push_back(std::move(img));
                a.delays.push_back(art_.frameDelay(gfs + f));
                a.anchorX.push_back(ax);
                a.anchorY.push_back(ay);
            }
        }
        int dx = 0, dy = 0;
        art_.groupDisplacement(group, &dx, &dy);
        a.originX = qRound((pb.tileX + def.fpX / 2.0) * T) - dx;
        a.originY = qRound((pb.tileY + def.fpY / 2.0) * T) - dy;
        return a;
    };

    Anim normal = buildAnim(normalGroup, lastFrameOnly);
    if (normal.frames.empty()) return;
    pb.anims.push_back(std::move(normal));
    // Ambients: extra COMPLETE animations the building switches to one at a time (not
    // overlays); each shares the base, so switching doesn't move the body.
    for (int ag : {amb1, amb2}) {
        if (ag < 0 || ag >= art_.groupCount()) continue;
        Anim a = buildAnim(ag, false);
        if (a.frames.size() > 1) pb.anims.push_back(std::move(a));
    }
    // Floor underlay (own group displacement) — lands on the footprint's dock tiles.
    if (floorGroup >= 0 && floorGroup < art_.groupCount()) {
        int fax = 0, fay = 0;
        QImage fimg = art_.frameToQImage(art_.groupFrameStart(floorGroup), &fax, &fay, shadowsEnabled_);
        if (!fimg.isNull()) {
            int fdx = 0, fdy = 0;
            art_.groupDisplacement(floorGroup, &fdx, &fdy);
            pb.floorDrawX = qRound((pb.tileX + def.fpX / 2.0) * T) - fdx - fax;
            pb.floorDrawY = qRound((pb.tileY + def.fpY / 2.0) * T) - fdy - fay;
            pb.floorImg = std::move(fimg);
        }
    }
    // Play order: Normal interleaved with each ambient ([0,1,0,2,…]); staggered rest gap.
    if (pb.anims.size() == 1) {
        pb.playOrder = {0};
    } else {
        for (int i = 1; i < static_cast<int>(pb.anims.size()); ++i) {
            pb.playOrder.push_back(0);
            pb.playOrder.push_back(i);
        }
    }
    pb.gapRemaining = animGapTicks_ > 0
        ? (static_cast<int>(placed_.size()) * 17) % (animGapTicks_ + 1) : 0;
    // Status-light position: the building's selection-box top-left + (2,2), in the same
    // map space as the body (logical origin = anims[0].originX/Y). op2art.dll draws the
    // light sprite there; we offset by the light sprite's own anchor at paint time.
    if (!overlayLight_.isNull()) {
        int sl = 0, st = 0, sr = 0, sb = 0;
        art_.groupSelectionBox(normalGroup, &sl, &st, &sr, &sb);
        // Store where the light's LOGICAL origin lands; paint subtracts each variant's anchor.
        pb.lightDrawX = pb.anims[0].originX + sl + 2;
        pb.lightDrawY = pb.anims[0].originY + st + 2;
        pb.hasLight = true;
    }
    placed_.push_back(std::move(pb));
}

// The per-faction art table, including every per-building special case. Shared by the
// showcase and the mission render (see the header note) so the two paths cannot drift.
TilesetView::BuildingArt TilesetView::resolveBuildingArt(const BuildingDef& def, bool eden) {
    BuildingArt a;
    // Garage: the catalog's Normal slot points at the *damaged* sprite (the engine
    // derives the clean look from construction). Use the construction group's last
    // frame as the static clean normal, plus the separate floor pad (g2039 Eden /
    // g431 Plymouth) vehicles drive onto, like a dock.
    if (def.name == QStringLiteral("Garage")) {
        a.normal = eden ? def.eden[0] : def.plym[0];
        a.floor = eden ? 2039 : 431;
        a.lastFrameOnly = true;
        return a;
    }
    // LightTower: Eden has a real animated-lamp Normal (g648); Plymouth writes no
    // Normal and its clean tower is the construction group's last frame (the
    // catalog's old pNorm=187 was the *damaged* sprite).
    if (def.name == QStringLiteral("LightTower")) {
        if (eden) a.normal = def.eden[5];
        else { a.normal = def.plym[0]; a.lastFrameOnly = true; }
        return a;
    }
    // Generic: the faction's Normal group ([5]) + its Ambient1/Ambient2 ([6]/[7]).
    a.normal = eden ? def.eden[5] : def.plym[5];
    a.amb1   = eden ? def.eden[6] : def.plym[6];
    a.amb2   = eden ? def.eden[7] : def.plym[7];
    return a;
}

void TilesetView::placeAllBuildings() {
    // Uniform grid showcase. Each building is shown PER FACTION where the Eden and
    // Plymouth sprites differ (Eden_X / Plymouth_X), matching the reference set —
    // e.g. the Eden and Plymouth Spaceports are different buildings. Each variant
    // gets a cell snapped to the tile grid; the body is seated on its footprint.
    const int startX = 2;
    const int startY = 2;
    const int cellW = 7;
    const int cellH = 7;
    const int cols = mapLoaded_ ? qMax(1, (map_.width() - 2 * startX) / cellW) : 8;
    int idx = 0;

    // Place one variant (a sprite group at the next grid cell, with a label).
    // floorGroup >= 0 draws a separate ground sprite (dock/garage pad) under the
    // body; lastFrameOnly shows just the group's final frame as a static normal
    // (used for buildings whose "operating" look is the construction end state).
    auto placeVariant = [&](const BuildingDef& def, int g, const QString& label,
                            int floorGroup = -1, bool lastFrameOnly = false,
                            int amb1 = -1, int amb2 = -1) {
        if (g < 0 || g >= art_.groupCount()) return;
        const int col = idx % cols;
        const int row = idx / cols;
        const int tileX = startX + col * cellW + (cellW - def.fpX) / 2;
        const int tileY = startY + row * cellH + (cellH - def.fpY) / 2;
        placeBuildingInstance(def, g, label, tileX, tileY, floorGroup, lastFrameOnly, amb1, amb2);
        ++idx;
    };

    for (const BuildingDef& def : catalog_.buildings()) {
        // GuardPost is a weapon-bearing turret, not a normal building — it's shown in
        // the combat showcase (base+turret per weapon, below the unit parade), so skip
        // it here instead of drawing its bare construction sprite.
        if (def.name == QStringLiteral("GuardPost")) continue;
        // Per-faction art, special cases included (resolveBuildingArt — shared with the
        // mission render). A Plymouth variant is shown only when its sprite differs.
        const BuildingArt ea = resolveBuildingArt(def, /*eden=*/true);
        const BuildingArt pa = resolveBuildingArt(def, /*eden=*/false);
        if (ea.normal >= 0)
            placeVariant(def, ea.normal, QStringLiteral("Eden ") + def.name,
                         ea.floor, ea.lastFrameOnly, ea.amb1, ea.amb2);
        if (pa.normal >= 0 && pa.normal != ea.normal)
            placeVariant(def, pa.normal, QStringLiteral("Plymouth ") + def.name,
                         pa.floor, pa.lastFrameOnly, pa.amb1, pa.amb2);
        // No Normal at all (GuardPost = turret): show the construction sprite so
        // the building still appears.
        if (ea.normal < 0 && pa.normal < 0) placeVariant(def, def.displayGroup(), def.name);
    }
}

// Lay out the unit catalog as a "parade": each chassis/flying unit gets a row
// showing all 16 rotations; combat chassis get a weapon turret composited on top
// (same origin). Placed in a band below the building showcase.
void TilesetView::placeAllUnits() {
    placedUnits_.clear();
    if (!mapLoaded_ || unitCatalog_.units().empty()) return;

    const int startX = 2;
    const int dirStepTiles = 2;   // one tile per unit + one tile gap; unit centred in its tile
    const int rowStepTiles = 2;   // tiles between unit rows (room for the label under)
    // Start below the building showcase (which flows in a 7-wide/7-tall cell grid).
    const int cellW = 7;
    const int cols = qMax(1, (map_.width() - 2 * startX) / cellW);
    const int buildingRows = (static_cast<int>(placed_.size()) + cols - 1) / cols;
    int rowY = 2 + buildingRows * 7 + 3;
    int uRow = 0;

    auto blit = [&](int group, QImage* img, int* ax, int* ay) -> bool {
        return blitUnitSprite(group, img, ax, ay);
    };

    // Place one unit (16 rotations) as a row starting at (rowStartX, tileY), with
    // a fully-formed label drawn under it. Used by both the main parade and the
    // combat-turret showcase column.
    auto placeRow = [&](int rowStartX, int tileY, const UnitDef& body,
                        const UnitDef* turret, const QString& rowLabel, int stackDy = 0,
                        bool idleRotate = true) {
        for (int r = 0; r < 16; ++r) {
            PlacedUnit pu;
            const int chassisGroup = body.groups[r];
            if (!blit(chassisGroup, &pu.chassis, &pu.cAnchorX, &pu.cAnchorY)) continue;
            // OP2-exact seating (Unit:Vehicle.DrawUnit / Unit:Tank.DrawUnit): the
            // unit pixel is the tile centre; the chassis is drawn at
            // (unitPixel - chassisGroupDisplacement), and the turret at
            // (unitPixel + chassisFrame.turretOffset - turretGroupDisplacement),
            // where turretOffset is the frame's optional block (the per-rotation
            // mount point on the hull). Displacement is shadow-independent, so the
            // unit doesn't jump when shadows toggle (only the shadow extends).
            const int tileX = rowStartX + r * dirStepTiles;
            const int upX = tileX * T + T / 2;
            const int upY = tileY * T + T / 2;
            int cdx = 0, cdy = 0;
            art_.groupDisplacement(chassisGroup, &cdx, &cdy);
            pu.originX = upX - cdx;
            pu.originY = upY - cdy;
            pu.tOriginX = pu.originX;
            pu.tOriginY = pu.originY;
            if (turret) {
                blit(turret->groups[r], &pu.turret, &pu.tAnchorX, &pu.tAnchorY);
                int ox = 0, oy = 0, tdx = 0, tdy = 0;
                art_.frameTurretOffset(art_.groupFrameStart(chassisGroup), &ox, &oy);
                art_.groupDisplacement(turret->groups[r], &tdx, &tdy);
                pu.tOriginX = upX + ox - tdx;
                pu.tOriginY = upY + oy - tdy;
                pu.turretStackDy = stackDy; // bomb-on-Tiger: draw the turret twice
                // Idle turret rotation (AnimationAmbientUnit): keep the inputs needed
                // to re-seat the turret at a new orientation each tick. Body stays put;
                // only the turret cycles. Starts matching the chassis facing (r).
                // Bomb weapons (Supernova/Starflare) don't idle-rotate in OP2 — it's a
                // bomb on the hull, no turret — so leave turretDef null (static).
                if (idleRotate) {
                    pu.turretDef = turret;
                    pu.turretRot = r;
                    pu.bodyRot = r;
                    pu.seatUpX = upX;
                    pu.seatUpY = upY;
                    pu.chassisGroup = chassisGroup;
                }
            }
            if (r == 0) {                        // label the row once, UNDER it (like buildings)
                pu.label = rowLabel;
                pu.labelX = rowStartX * T;
                pu.labelY = (tileY + 1) * T + 10;
            }
            placedUnits_.push_back(std::move(pu));
        }
    };

    auto findUnit = [&](const QString& name, const QString& faction) -> const UnitDef* {
        for (const UnitDef& u : unitCatalog_.units())
            if (u.name == name && (faction.isEmpty() || u.faction == faction)) return &u;
        return nullptr;
    };

    // Turret lookup: most weapon turrets are shared (faction "both", player-palette only),
    // but a few have distinct Eden vs Plymouth ART (Starflare: Eden g1623 / Plymouth g1341;
    // EMP GuardPost). Prefer the chassis faction's variant, else fall back to the shared one.
    auto findTurret = [&](const QString& name, const QString& faction) -> const UnitDef* {
        const UnitDef* shared = nullptr;
        for (const UnitDef& u : unitCatalog_.units()) {
            if (u.kind != QStringLiteral("turret") || u.name != name) continue;
            if (u.faction == faction) return &u;                       // exact faction art
            if (u.faction == QStringLiteral("both")) shared = &u;
        }
        return shared;
    };

    // --- Main parade: every chassis/flying unit, all 16 rotations -------------
    for (const UnitDef& u : unitCatalog_.units()) {
        if (u.kind == QStringLiteral("turret")) continue;  // composited via its chassis
        const UnitDef* turret = (u.kind == QStringLiteral("chassis"))
                                    ? unitCatalog_.turretForChassis(u.name)
                                    : nullptr;
        QString label = u.name;
        if (u.faction == QStringLiteral("eden")) label += QStringLiteral(" [E]");
        else if (u.faction == QStringLiteral("plymouth")) label += QStringLiteral(" [P]");
        placeRow(startX, rowY + uRow * rowStepTiles, u, turret, label);
        ++uRow;
    }

    // --- Combat showcase: EVERY chassis mounts EVERY weapon (OP2-faithful).
    // Turret model (from OP2 art + EXE selectors): Lynx & Panther SHARE one turret
    // graphic per weapon; Tiger has its OWN graphic (usually a dual turret, sometimes
    // just a larger single e.g. ESG). Tiger tables exist for 9 weapons. Supernova (and
    // Starflare) are BOMB weapons with NO dedicated dual sprite — Tiger reuses the
    // single graphic (the in-game "dual" look is just the single stacked), so the
    // nullptr→single path below is correct, not a gap. The mount point comes from each
    // chassis's own frame optional-block, so the shared graphic seats correctly on a
    // different hull. One row = one chassis+weapon, 16 dirs.
    // dual = the Tiger-specific sprite (nullptr ⇒ none). bomb = no dedicated dual; on
    // Tiger the single is drawn twice (stacked) instead.
    struct Weapon { const char* name; const char* single; const char* dual; bool bomb; };
    const std::vector<Weapon> weapons = {
        {"Laser",       "Laser_Lynx",            "Laser_Tiger",       false},
        {"RailGun",     "RailGun_LynxTiger",     "RailGun_Tiger",     false},
        {"ESG",         "ESG_LynxTiger",         "ESG_Tiger",         false},
        {"ThorsHammer", "ThorsHammer_LynxTiger", "ThorsHammer_Tiger", false},
        {"Stickyfoam",  "Stickyfoam_LynxTiger",  "Stickyfoam_Tiger",  false},
        {"AcidCloud",   "AcidCloud_LynxTiger",   "AcidCloud_Tiger",   false},
        {"EMP",         "EMP_Panther",           "EMP_Tiger",         false},
        {"RPG",         "RPG_Panther",           "RPG_Tiger",         false},
        {"Microwave",   "Microwave_Panther",     "Microwave_Tiger",   false},
        {"Supernova",   "Supernova_Panther",     nullptr,             true },  // bomb: Tiger stacks the single
        {"Starflare",   "Starflare",             nullptr,             true },  // bomb; faction-split turret (Eden g1623 / Plymouth g1341), constant non-rotating
    };
    const char* chassisNames[3] = {"Lynx", "Panther", "Tiger"};

    // Lay one faction's combat column at columnStartX; returns rows placed.
    auto placeCombatColumn = [&](const QString& faction, const QString& factTag,
                                 int columnStartX) -> int {
        int cRow = 0;
        for (const char* chName : chassisNames) {
            const UnitDef* chassis = findUnit(QString::fromLatin1(chName), faction);
            if (!chassis) continue;
            const bool isTiger = (QString::fromLatin1(chName) == QStringLiteral("Tiger"));
            for (const Weapon& w : weapons) {
                // Tiger prefers its dual turret; Lynx/Panther (and Tiger fallback)
                // use the single. Same turret art, seated via this chassis's offset.
                const char* turretName = (isTiger && w.dual) ? w.dual : w.single;
                // Faction-aware: Starflare (and other split turrets) pick the chassis's
                // faction art (Eden silver vs Plymouth yellow); shared turrets fall back.
                const UnitDef* turret = findTurret(QString::fromLatin1(turretName), faction);
                if (!turret) continue;
                // Bomb weapon on Tiger: no dual sprite, so stack the single turret
                // (a second copy raised by ~⅓ of its art height, like OP2's twin bomb).
                int stackDy = 0;
                if (isTiger && w.bomb) {
                    int fs = art_.groupFrameStart(turret->groups[0]), ax = 0, ay = 0;
                    QImage tmp = art_.frameToQImage(fs, &ax, &ay, false);
                    stackDy = tmp.isNull() ? 8 : qMax(4, tmp.height() / 3);
                }
                placeRow(columnStartX, rowY + cRow * rowStepTiles, *chassis, turret,
                         QStringLiteral("%1 + %2 %3").arg(QString::fromLatin1(chName),
                                                          QString::fromLatin1(w.name), factTag),
                         stackDy, /*idleRotate=*/!w.bomb); // bombs are static
                ++cRow;
            }
        }
        // GuardPosts: one static base + each faction's weapon turret (its OWN art), shown
        // across 16 directions like the chassis rows. Eden-only weapons on Eden, Plymouth-
        // only on Plymouth, plus the both-faction EMP/Starflare/Supernova on each.
        const bool eden = (faction == QStringLiteral("eden"));
        const char* edenGp[] = {"Laser", "RailGun", "AcidCloud", "ThorsHammer",
                                "EMP", "Starflare", "Supernova"};
        const char* plymGp[] = {"ESG", "RPG", "Microwave", "Stickyfoam",
                                "EMP", "Starflare", "Supernova"};
        for (const char* gw : eden ? edenGp : plymGp) {
            const QString weapon = QString::fromLatin1(gw);
            const int tileY = rowY + cRow * rowStepTiles;
            for (int r = 0; r < 16; ++r)
                placeGuardPost(faction, weapon, columnStartX + r * dirStepTiles, tileY, r,
                               r == 0 ? QStringLiteral("GuardPost + %1 %2").arg(weapon, factTag)
                                      : QString(),
                               /*idleRotate=*/true);
            ++cRow;
        }
        return cRow;
    };

    const int columnWidthTiles = 16 * dirStepTiles + 4;  // 16 dirs + gap between columns
    const int edenCombatStartX = startX + columnWidthTiles;          // right of the parade
    const int plymCombatStartX = edenCombatStartX + columnWidthTiles; // right of Eden column
    const int cRowE = placeCombatColumn(QStringLiteral("eden"),     QStringLiteral("[E]"), edenCombatStartX);
    const int cRowP = placeCombatColumn(QStringLiteral("plymouth"), QStringLiteral("[P]"), plymCombatStartX);
    const int cRow = cRowE + cRowP;

    Logger::log(QStringLiteral("placed %1 unit sprites (%2 parade rows, %3 combat rows)")
                    .arg(placedUnits_.size()).arg(uRow).arg(cRow));
}
