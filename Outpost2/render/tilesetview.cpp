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
#include "op2/sim/command.h"

namespace {
constexpr int T = TilesetManager::kTileSize;

} // namespace

TilesetView::TilesetView(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAcceptHoverEvents(true);

    // Read settings from outpost2.ini (next to the exe).
    const QString iniPath = QCoreApplication::applicationDirPath() + "/outpost2.ini";
    QSettings settings(iniPath, QSettings::IniFormat);
    const QString base = settings.value(QStringLiteral("Game/Outpost2"),
                                        QStringLiteral("D:/opu/op2remake")).toString();
    animTicksPerGameTick_ =
        qMax(1, settings.value(QStringLiteral("Render/AnimationSpeed"), 7).toInt());
    animGapTicks_ =
        qMax(0, settings.value(QStringLiteral("Render/AnimationGap"), 60).toInt());
    ambientStructureEnabled_ =
        settings.value(QStringLiteral("Render/AnimationAmbientStructure"), true).toBool();
    ambientUnitEnabled_ =
        settings.value(QStringLiteral("Render/AnimationAmbientUnit"), true).toBool();
    ambientResourceEnabled_ =
        settings.value(QStringLiteral("Render/AnimationAmbientResource"), true).toBool();
    overlayEnabled_ =
        settings.value(QStringLiteral("Render/Overlay"), true).toBool();
    shadowsEnabled_ =
        settings.value(QStringLiteral("Game/StationaryShadows"), true).toBool();
    mobileShadowsEnabled_ =
        settings.value(QStringLiteral("Game/MobileShadows"), true).toBool();
    edgeScrollEnabled_ =
        settings.value(QStringLiteral("Game/ScrollByMouse"), true).toBool();

    base_ = base; // kept so the sidebar buttons can switch maps at runtime
    const QString tilesetDir = base + "/OPU/base/tilesets";
    const QString artPrt = base + "/OPU/base/sprites/op2_art.prt";
    const QString artBmp = base + "/OP2_ART.BMP";

    tilesets_.loadFromDirectory(tilesetDir);

    // Load the sprite art + building/unit catalogs ONCE — they're reused across every
    // map switch. (Frame advancement is driven by GameClock ticks; see advanceAnimation.)
    QString artErr;
    if (!art_.load(artPrt, artBmp, &artErr))
        qWarning("TilesetView: art load failed: %s", artErr.toUtf8().constData());
    // The "computer overlay" status light is a fixed 10×10 art sprite — group 47 (green),
    // drawn at each building's selection-box corner. Load it once. (op2art.dll hardcodes
    // this group index for its lights overlay.)
    constexpr int kOverlayLightGreen = 47; // Active
    constexpr int kOverlayLightGray  = 49; // Idle (the "off" dot — green went out)
    constexpr int kOverlayLightRed   = 48; // Disabled by damage (frame 0 solid; 8-frame flashing)
    if (art_.frameCount() > 0) {
        const int lg = art_.groupFrameStart(kOverlayLightGreen);
        if (lg >= 0) overlayLight_ = art_.frameToQImage(lg, &overlayLightAX_, &overlayLightAY_, false);
        const int la = art_.groupFrameStart(kOverlayLightGray);
        if (la >= 0) overlayLightGray_ = art_.frameToQImage(la, &overlayLightGrayAX_, &overlayLightGrayAY_, false);
        const int lr = art_.groupFrameStart(kOverlayLightRed);
        if (lr >= 0) overlayLightRed_ = art_.frameToQImage(lr, &overlayLightRedAX_, &overlayLightRedAY_, false);
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!catalog_.load(appDir + "/building_anim_catalog.tsv"))
        qWarning("TilesetView: building catalog not found");
    if (!unitCatalog_.load(appDir + "/unit_anim_catalog.tsv"))
        qWarning("TilesetView: unit catalog not found");
    // Stat sheets: per-type stats (HP, armor, …) from the original OP2 sheets (unpacked
    // from the game's .vol archive by the OPU 1.4.1 installer; sit under OPU/sheets/).
    if (!buildingSheet_.load((base + "/OPU/sheets/building.txt").toStdString()))
        qWarning("TilesetView: building.txt sheet not found");
    if (!vehicleSheet_.load((base + "/OPU/sheets/vehicles.txt").toStdString()))
        qWarning("TilesetView: vehicles.txt sheet not found");
    // space.txt — per-type data for space objects (satellites, starship components, launch
    // vehicles): Common/Rare ore cost, Build Time, Research Topic. Used by the Space Program panel.
    if (!spaceSheet_.load((base + "/OPU/sheets/space.txt").toStdString()))
        qWarning("TilesetView: space.txt sheet not found");
    populateSpaceObjects(); // parse space.txt costs/names into memory

    // [Debug] Map = which map to load at startup, relative to <Outpost2>/OPU/maps/
    // (accepts forward or back slashes, with/without a leading slash).
    const QString mapRel = settings.value(QStringLiteral("Debug/Map"),
                                          QStringLiteral("Nostalgia/opu_02.map")).toString();
    loadMap(mapRel);

    // Drive edge / arrow-key scrolling at ~60 Hz.
    connect(&scrollTimer_, &QTimer::timeout, this, &TilesetView::scrollTick);
    scrollTimer_.start(16);

    // OP2 hover dwell: after ~1s over an object, show its corner-box + HP bar + name tooltip.
    hoverDelayTimer_.setSingleShot(true);
    hoverDelayTimer_.setInterval(900);
    connect(&hoverDelayTimer_, &QTimer::timeout, this, [this] {
        if (hoverId_.valid()) { hoverActive_ = true; emit hoverChanged(); update(); }
    });
}

void TilesetView::loadMap(const QString& mapRelIn) {
    QString mapRel = mapRelIn;
    mapRel.replace('\\', '/');
    while (mapRel.startsWith('/')) mapRel.remove(0, 1);
    const QString mapPath = base_ + "/OPU/maps/" + mapRel;
    Logger::log(QStringLiteral("load map: ") + mapPath);

    // Reset all render + mission state so switching maps doesn't leave stale objects.
    clearSelection(); // drop any selection from the previous map
    missionMode_ = false;
    world_.clear();
    missionResources_.clear();
    friendlyPlayers_.clear(); // parade / fresh load → no ownership filter (all friendly)
    localPlayer_ = 0;
    placed_.clear();
    placedUnits_.clear();
    placedSprites_.clear();
    emit reportsChanged(); // command-report panels reset (parade has no factories)

    // Load terrain via the pluggable registry (OP2Utility backend; a .opm resolves its
    // companion .map one hop).
    op2::maps::MapLoaderRegistry registry = op2::maps::MapLoaderRegistry::withDefaults();
    op2::maps::LoadResult result = registry.load(mapPath.toStdString());
    if (result.ok) {
        map_ = std::move(result.map);
        mapLoaded_ = true;
        qInfo("TilesetView: loaded map %dx%d, %lld tileset sources",
              map_.width(), map_.height(),
              static_cast<long long>(map_.tilesetSources.size()));
        buildMinimap();
    } else {
        qWarning("TilesetView: map load failed: %s", result.error.c_str());
        update();
        return;
    }

    const bool artOk   = art_.frameCount() > 0;
    const bool haveBld  = !catalog_.buildings().empty();
    const bool haveUnit = !unitCatalog_.units().empty();
    // A .opm loads its mission objects into the World and renders THOSE; a plain .map
    // shows the static catalog parade. (The parade is kept as the .map path — and the
    // only thing that can draw impossible cross-faction combos.)
    const bool isOpm = mapRel.endsWith(QStringLiteral(".opm"), Qt::CaseInsensitive);
    if (artOk && isOpm && (haveBld || haveUnit) && loadMission(mapPath)) {
        missionMode_ = true;
        placeFromWorld();
        qInfo("TilesetView: mission mode — %d buildings, %d units from the World",
              static_cast<int>(placed_.size()), static_cast<int>(placedUnits_.size()));
    } else if (artOk) {
        if (haveBld)  placeAllBuildings();
        if (haveUnit) placeAllUnits();
        qInfo("TilesetView: parade — %d buildings, %d unit sprites",
              static_cast<int>(placed_.size()), static_cast<int>(placedUnits_.size()));
    }

    // Snap the camera home so the new map shows from its top-left.
    cameraX_ = cameraY_ = 0;
    clampCamera();
    update();
    emit cameraChanged();
}

void TilesetView::buildMinimap() {
    if (!mapLoaded_) return;
    minimapImage_ = QImage(map_.width(), map_.height(), QImage::Format_RGB32);
    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            const op2::sim::Tile& t = map_.at(x, y);
            QRgb c = qRgb(20, 20, 24);
            if (t.tilesetIndex < map_.tilesetSources.size()) {
                const int wi = tilesets_.wellIndexByName(
                    QString::fromStdString(map_.tilesetSources[t.tilesetIndex]));
                if (wi >= 0) c = tilesets_.tileAverageColor(wi, t.imageIndex);
            }
            minimapImage_.setPixel(x, y, c);
        }
    }
}




void TilesetView::paint(QPainter* painter) {
    if (mapLoaded_) {
        paintMap(painter);
    } else {
        paintTilesetGrid(painter);
    }
}

void TilesetView::paintMap(QPainter* painter) {
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false); // crisp tiles

    const qreal viewMapW = viewWidthMapPx();
    const qreal viewMapH = viewHeightMapPx();

    // Only draw the tiles visible under the current camera (culling).
    const int firstCol = qMax(0, static_cast<int>(std::floor(cameraX_ / T)));
    const int firstRow = qMax(0, static_cast<int>(std::floor(cameraY_ / T)));
    const int lastCol = qMin(map_.width(),
                             static_cast<int>(std::ceil((cameraX_ + viewMapW) / T)));
    const int lastRow = qMin(map_.height(),
                             static_cast<int>(std::ceil((cameraY_ + viewMapH) / T)));

    // Draw in map space; the painter applies zoom + camera (no seams).
    painter->save();
    painter->scale(zoom_, zoom_);
    painter->translate(-cameraX_, -cameraY_);
    paintMapContent(painter, firstCol, firstRow, lastCol, lastRow);
    painter->restore();
}

// Draws terrain tiles (over [firstCol,lastCol)×[firstRow,lastRow)), the grid,
// buildings and units — all in MAP-pixel space. The caller sets up the transform
// (viewport paint applies zoom+camera; full-map export uses identity). Shared so
// both paths render identically.
void TilesetView::paintMapContent(QPainter* painter, int firstCol, int firstRow,
                                  int lastCol, int lastRow) {
    for (int cy = firstRow; cy < lastRow; ++cy) {
        for (int cx = firstCol; cx < lastCol; ++cx) {
            const op2::sim::Tile& t = map_.at(cx, cy);
            if (t.tilesetIndex >= map_.tilesetSources.size()) continue;
            const int wi = tilesets_.wellIndexByName(
                QString::fromStdString(map_.tilesetSources[t.tilesetIndex]));
            if (wi < 0) continue;
            tilesets_.drawTile(*painter, QRect(cx * T, cy * T, T, T), wi, t.imageIndex);
        }
    }

    // 32px tile grid — over the terrain, under the buildings. Cosmetic pen so the
    // lines stay 1px on screen at any zoom.
    if (gridEnabled_) {
        QPen gridPen(QColor(0, 0, 0, 170)); // darker, near-black
        gridPen.setWidth(0);
        painter->setPen(gridPen);
        const int x0 = firstCol * T, x1 = lastCol * T;
        const int y0 = firstRow * T, y1 = lastRow * T;
        for (int cx = firstCol; cx <= lastCol; ++cx)
            painter->drawLine(cx * T, y0, cx * T, y1);
        for (int cy = firstRow; cy <= lastRow; ++cy)
            painter->drawLine(x0, cy * T, x1, cy * T);
    }

    // Map resources/markers/wreckage: ground features, so drawn over terrain but
    // UNDER buildings/units (a vent half-covered by a structure reads correctly).
    // Each frame draws so its logical origin lands at the sprite origin.
    for (const PlacedSprite& ps : placedSprites_) {
        if (ps.frames.empty()) continue;
        const int f = ps.frame % static_cast<int>(ps.frames.size());
        painter->drawImage(ps.originX - ps.anchorX[f], ps.originY - ps.anchorY[f], ps.frames[f]);
    }

    // Draw every placed building's current animation frame (map space), with a
    // small name label under its footprint so the whole catalog is verifiable.
    QFont labelFont = painter->font();
    labelFont.setPixelSize(7);
    painter->setFont(labelFont);
    for (const PlacedBuilding& pb : placed_) {
        if (pb.anims.empty()) continue;
        // Ground underlay (garage floor / dock pad) first, so the body sits on it.
        if (!pb.floorImg.isNull())
            painter->drawImage(pb.floorDrawX, pb.floorDrawY, pb.floorImg);
        // The single currently-playing animation (Normal, or one ambient). Draw so
        // its logical origin lands at the anim's origin — seats the body on its
        // footprint and keeps it fixed as the per-frame bounding box changes.
        const Anim& a = pb.anims[pb.curAnim];
        const int f = pb.frame % static_cast<int>(a.frames.size());
        painter->drawImage(a.originX - a.anchorX[f],
                           a.originY - a.anchorY[f], a.frames[f]);
        // Computer-overlay status light (own + allied only): player-Idle → gray (intent
        // wins), else condition-Disabled → red, else Active → green.
        if (overlayEnabled_ && pb.hasLight && pb.friendly) {
            if (pb.idle && !overlayLightGray_.isNull())
                painter->drawImage(pb.lightDrawX - overlayLightGrayAX_,
                                   pb.lightDrawY - overlayLightGrayAY_, overlayLightGray_);
            else if (pb.disabled && !overlayLightRed_.isNull())
                painter->drawImage(pb.lightDrawX - overlayLightRedAX_,
                                   pb.lightDrawY - overlayLightRedAY_, overlayLightRed_);
            else if (pb.active && !overlayLight_.isNull())
                painter->drawImage(pb.lightDrawX - overlayLightAX_,
                                   pb.lightDrawY - overlayLightAY_, overlayLight_);
        }
        if (labelsEnabled_) {
            painter->setPen(QColor(235, 230, 140));
            painter->drawText(pb.tileX * T, (pb.tileY + pb.fpY) * T + 8, pb.name);
        }
    }

    // Units: every placed direction sprite (chassis, then turret on top for combat
    // units) drawn at its shared origin. Static here — they'd rotate when moving.
    for (const PlacedUnit& pu : placedUnits_) {
        if (!pu.chassis.isNull())
            painter->drawImage(pu.originX - pu.cAnchorX, pu.originY - pu.cAnchorY, pu.chassis);
        if (!pu.turret.isNull()) {
            painter->drawImage(pu.tOriginX - pu.tAnchorX, pu.tOriginY - pu.tAnchorY, pu.turret);
            // Stacked second bomb (Supernova/Starflare on Tiger): same sprite, raised.
            if (pu.turretStackDy > 0)
                painter->drawImage(pu.tOriginX - pu.tAnchorX,
                                   pu.tOriginY - pu.tAnchorY - pu.turretStackDy, pu.turret);
        }
        // GuardPost status overlay: unlike buildings, NO light when Active. When off show
        // the gray "off" dot if player-Idled, else red if condition-Disabled — CENTRED.
        if (pu.isGuardPost && overlayEnabled_ && pu.friendly && !pu.active) {
            const QImage& dot = pu.idle ? overlayLightGray_ : overlayLightRed_;
            if (!dot.isNull())
                painter->drawImage(pu.centerX - dot.width() / 2,
                                   pu.centerY - dot.height() / 2, dot);
        }
        if (labelsEnabled_ && !pu.label.isEmpty()) {
            painter->setPen(QColor(150, 220, 235));
            painter->drawText(pu.labelX, pu.labelY, pu.label);
        }
    }

    // Selection box around the selected object — over its footprint (buildings) or 1×1
    // cell (units / GuardPosts). Cosmetic 1px green rectangle (OP2's selection outline).
    if (selectedId_.valid()) {
        QRect box;
        bool found = false;
        for (const PlacedBuilding& pb : placed_)
            if (pb.sourceId == selectedId_) {
                box = QRect(pb.tileX * T, pb.tileY * T, pb.fpX * T, pb.fpY * T);
                found = true; break;
            }
        if (!found)
            for (const PlacedUnit& pu : placedUnits_)
                if (pu.sourceId == selectedId_) {
                    const op2::sim::Unit* su = world_.get(pu.sourceId);
                    const int cx = su ? su->tileX : pu.centerX / T;
                    const int cy = su ? su->tileY : pu.centerY / T;
                    box = QRect(cx * T, cy * T, T, T);
                    found = true; break;
                }
        if (found) {
            QPen pen(QColor(120, 230, 120));
            pen.setWidth(0); // cosmetic: stays 1px at any zoom
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(box.adjusted(0, 0, -1, -1));
        }
    }

    paintHover(painter); // OP2 delayed-hover corner box + HP bar (on top)
}

// OP2 hover decorations: a light corner-bracket box around the dwelled-on object + a green HP bar
// above it. Drawn in map space (follows the camera). The name tooltip is a QML overlay.
void TilesetView::paintHover(QPainter* painter) {
    if (!hoverActive_ || !hoverId_.valid() || hoverBox_.isNull()) return;
    // The reticle sits a few px OUTSIDE the footprint (margin, as in OP2).
    const QRect b = hoverBox_.adjusted(-4, -4, 4, 4);
    const int x0 = b.left(), y0 = b.top(), x1 = b.left() + b.width(), y1 = b.top() + b.height();
    const int L = qBound(8, qMin(b.width(), b.height()) / 4, 16); // bracket arm length

    QPen pen(QColor(210, 210, 210));     // light grey reticle
    pen.setWidth(0);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(x0, y0, x0 + L, y0); painter->drawLine(x0, y0, x0, y0 + L); // top-left
    painter->drawLine(x1, y0, x1 - L, y0); painter->drawLine(x1, y0, x1, y0 + L); // top-right
    painter->drawLine(x0, y1, x0 + L, y1); painter->drawLine(x0, y1, x0, y1 - L); // bottom-left
    painter->drawLine(x1, y1, x1 - L, y1); painter->drawLine(x1, y1, x1, y1 - L); // bottom-right

    // HP bar above the box — green fill over a 1px black border, ~55% of the box width, centred
    // (measured from the OP2 reference: ~7px tall, pure #00ff00, black outline).
    if (hoverMaxHp_ > 0) {
        const int barH = 6, gap = 4;
        const int barW = qMax(16, b.width() * 11 / 20);
        const int barX = b.left() + (b.width() - barW) / 2;
        const int barY = y0 - barH - gap;
        painter->fillRect(barX - 1, barY - 1, barW + 2, barH + 2, QColor(0, 0, 0));
        const double ratio = qBound(0.0, static_cast<double>(hoverHp_) / hoverMaxHp_, 1.0);
        painter->fillRect(barX, barY, static_cast<int>(barW * ratio), barH, QColor(0, 255, 0));
    }
}

void TilesetView::clampCamera() {
    if (!mapLoaded_) {
        cameraX_ = cameraY_ = 0;
        return;
    }
    const qreal maxX = qMax(0.0, map_.width() * static_cast<qreal>(T) - viewWidthMapPx());
    const qreal maxY = qMax(0.0, map_.height() * static_cast<qreal>(T) - viewHeightMapPx());
    cameraX_ = qBound(0.0, cameraX_, maxX);
    cameraY_ = qBound(0.0, cameraY_, maxY);
}

void TilesetView::centerOnMap(qreal mapX, qreal mapY) {
    cameraX_ = mapX - viewWidthMapPx() / 2.0;
    cameraY_ = mapY - viewHeightMapPx() / 2.0;
    clampCamera();
    update();
    emit cameraChanged();
}

void TilesetView::setZoomEnabled(bool e) {
    if (zoomEnabled_ == e) return;
    zoomEnabled_ = e;
    if (!e && zoom_ != 1.0) {
        zoom_ = 1.0;
        clampCamera();
        update();
        emit cameraChanged();
    }
    emit zoomEnabledChanged();
}

void TilesetView::setShadowsEnabled(bool e) {
    if (shadowsEnabled_ == e) return;
    shadowsEnabled_ = e;
    // Re-composite with/without the shadow layer — from the World in mission mode,
    // else the static parade. (Don't clobber the mission with the parade.)
    if (missionMode_) {
        placeFromWorld();
    } else {
        placed_.clear();
        placeAllBuildings();
    }
    update();
    emit shadowsEnabledChanged();
}

void TilesetView::setMobileShadowsEnabled(bool e) {
    if (mobileShadowsEnabled_ == e) return;
    mobileShadowsEnabled_ = e;
    if (missionMode_) placeFromWorld(); else placeAllUnits();
    update();
    emit mobileShadowsEnabledChanged();
}

QString TilesetView::exportViewport() {
    const int w = qMax(1, static_cast<int>(width()));
    const int h = qMax(1, static_cast<int>(height()));
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    paint(&p); // render the current game view (terrain + grid + buildings)
    p.end();

    const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/exports");
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/viewport-") +
                         QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") +
                         QStringLiteral(".png");
    if (!img.save(path)) {
        Logger::log(QStringLiteral("export FAILED: ") + path);
        return QString();
    }
    Logger::log(QStringLiteral("exported viewport: ") + path);
    return path;
}

QString TilesetView::exportFullMap() {
    if (!mapLoaded_) {
        Logger::log(QStringLiteral("export full map: no map loaded"));
        return QString();
    }
    // One image at the full map's pixel size (1:1, no zoom/camera). Guard against
    // an absurd allocation on a huge map.
    const qint64 fullW = static_cast<qint64>(map_.width()) * T;
    const qint64 fullH = static_cast<qint64>(map_.height()) * T;
    if (fullW <= 0 || fullH <= 0 || fullW * fullH > 268435456LL) { // ~256 MPx cap
        Logger::log(QStringLiteral("export full map: size out of range %1x%2")
                        .arg(fullW).arg(fullH));
        return QString();
    }
    QImage img(static_cast<int>(fullW), static_cast<int>(fullH), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    // Identity transform = map-pixel space; render every tile, building and unit.
    paintMapContent(&p, 0, 0, map_.width(), map_.height());
    p.end();

    const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/exports");
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/map-") +
                         QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") +
                         QStringLiteral(".png");
    if (!img.save(path)) {
        Logger::log(QStringLiteral("export full map FAILED: ") + path);
        return QString();
    }
    Logger::log(QStringLiteral("exported full map (%1x%2): %3")
                    .arg(fullW).arg(fullH).arg(path));
    return path;
}

void TilesetView::zoomAt(qreal deltaY, qreal x, qreal y) {
    if (!zoomEnabled_) return;

    // Discrete zoom levels: out (0.5x), normal (1x), in (2x). Wheel up zooms in.
    static const qreal levels[] = {0.5, 1.0, 2.0};
    constexpr int n = 3;
    int idx = 1;
    for (int i = 0; i < n; ++i) {
        if (qFuzzyCompare(levels[i], zoom_)) { idx = i; break; }
    }
    idx += (deltaY > 0) ? 1 : -1;
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    const qreal target = levels[idx];
    if (qFuzzyCompare(target, zoom_)) return;

    // Keep the map point under the cursor fixed across the zoom change.
    const qreal mapX = cameraX_ + x / zoom_;
    const qreal mapY = cameraY_ + y / zoom_;
    zoom_ = target;
    cameraX_ = mapX - x / zoom_;
    cameraY_ = mapY - y / zoom_;
    clampCamera();
    update();
    emit cameraChanged();
}

// Change one placed object's status. The render layer never writes World flags
// itself (see the header's World contract): a mission object's change is issued
// as a sim command — the same packet a network peer or a replay would apply —
// and the cached render flags are then read back FROM the World, so they can
// never drift from the source of truth.
void TilesetView::setObjectStatus(PlacedBuilding* pb, PlacedUnit* pu,
                                  bool disabledFlag, bool on) {
    const op2::sim::UnitId id = pb ? pb->sourceId : pu->sourceId;
    if (id.valid()) {
        if (disabledFlag) {
            // Disabled is a sim-derived condition (no power/workers/damage/EMP),
            // not a player command — debug-poked through the command layer's
            // explicit backdoor until the economy sim computes it.
            if (!op2::sim::debugSetDisabled(world_, id, on)) return; // stale handle
        } else {
            const op2::sim::CommandPacket pkt = op2::sim::makeIdleCommand(
                static_cast<std::uint32_t>(animTick_), localPlayer_, id, on);
            if (!op2::sim::applyCommand(world_, pkt)) return; // rejected: stale handle
        }
        // Refresh the cached render flags from the World result.
        const op2::sim::Unit* su = world_.get(id);
        const bool idle = su->flags & op2::sim::UnitFlag::Idle;
        const bool disabled = su->flags & op2::sim::UnitFlag::Disabled;
        if (pb) { pb->idle = idle; pb->disabled = disabled; }
        else    { pu->idle = idle; pu->disabled = disabled; }
    } else {
        // Static parade: no World object exists, so the cached flags ARE the state.
        if (pb) { (disabledFlag ? pb->disabled : pb->idle) = on; }
        else    { (disabledFlag ? pu->disabled : pu->idle) = on; }
    }
    // Re-derive "operating"; a stopped structure snaps to its base frame.
    if (pb) {
        pb->active = !pb->idle && !pb->disabled;
        if (!pb->active) {
            pb->curAnim = 0; pb->frame = 0; pb->frameAccum = 0.0;
            pb->playIdx = 0; pb->gapRemaining = 0;
        }
    } else {
        pu->active = !pu->idle && !pu->disabled;
    }
    if (id.valid() && id == selectedId_) { rebuildSelectedInfo(); emit selectionChanged(); }
    update();
}

void TilesetView::toggleBuildingAt(qreal mouseX, qreal mouseY, bool toggleDisabled) {
    if (!mapLoaded_) return;
    // Screen px → map tile (inverse of the paint transform).
    const int tx = static_cast<int>(std::floor((cameraX_ + mouseX / zoom_) / T));
    const int ty = static_cast<int>(std::floor((cameraY_ + mouseY / zoom_) / T));
    const char* what = toggleDisabled ? "Disabled" : "Idle";

    // Topmost building whose footprint covers the tile (search back-to-front = draw order).
    for (auto it = placed_.rbegin(); it != placed_.rend(); ++it) {
        PlacedBuilding& pb = *it;
        if (tx < pb.tileX || tx >= pb.tileX + pb.fpX ||
            ty < pb.tileY || ty >= pb.tileY + pb.fpY) continue;
        setObjectStatus(&pb, nullptr, toggleDisabled, toggleDisabled ? !pb.disabled : !pb.idle);
        Logger::log(QStringLiteral("building '%1' at %2,%3: %4=%5 (idle=%6 disabled=%7)")
                        .arg(pb.name).arg(tx).arg(ty).arg(QString::fromLatin1(what))
                        .arg(toggleDisabled ? pb.disabled : pb.idle).arg(pb.idle).arg(pb.disabled));
        return;
    }
    // GuardPosts are 1×1 buildings rendered as units — toggle their flags too.
    for (auto it = placedUnits_.rbegin(); it != placedUnits_.rend(); ++it) {
        PlacedUnit& pu = *it;
        if (!pu.isGuardPost) continue;
        if (pu.centerX / T != tx || pu.centerY / T != ty) continue; // 1×1 cell hit-test
        setObjectStatus(nullptr, &pu, toggleDisabled, toggleDisabled ? !pu.disabled : !pu.idle);
        Logger::log(QStringLiteral("GuardPost at %1,%2: %3 (idle=%4 disabled=%5)")
                        .arg(tx).arg(ty).arg(QString::fromLatin1(what)).arg(pu.idle).arg(pu.disabled));
        return;
    }
}

bool TilesetView::surveyMineAt(int tileX, int tileY) {
    // Reveal the unsurveyed mining beacon on this cell (a Robo-Surveyor reached it).
    for (const PlacedSprite& ps : placedSprites_) {
        if (!ps.isMine || ps.cellX != tileX || ps.cellY != tileY || ps.resourceIndex < 0) continue;
        MissionResource& mr = missionResources_[ps.resourceIndex];
        if (mr.surveyed) return false;
        mr.surveyed = true;
        placeResources(); // rebuild → the beacon now shows its real type/yield
        Logger::log(QStringLiteral("surveyed mine at %1,%2 → %3 %4")
                        .arg(tileX).arg(tileY).arg(mr.oreType, mr.barYield));
        update();
        return true;
    }
    return false;
}

int TilesetView::surveyAllMines(int player) {
    // EDWARD Satellite: reveal every mining beacon at once. (Per-player visibility isn't
    // modelled yet — we render one player's view — so this surveys them all; `player` is
    // carried for when ownership/visibility exists.)
    int revealed = 0;
    for (MissionResource& mr : missionResources_) {
        if (mr.kind != QStringLiteral("Beacon") || mr.subType != QStringLiteral("MiningBeacon"))
            continue;
        if (mr.surveyed) continue;
        mr.surveyed = true;
        ++revealed;
    }
    if (revealed > 0) {
        placeResources(); // rebuild all beacons in one pass
        update();
    }
    Logger::log(QStringLiteral("survey all mines (player %1): %2 revealed")
                    .arg(player).arg(revealed));
    return revealed;
}

// --- selection (the detail panel / "7th view") -------------------------------

QString TilesetView::armorClassName(int /*cls*/) {
    // TODO: map the sheet's "Armor Class" number to OP2's name (e.g. "Heavy Armor").
    // Until confirmed, the panel shows the raw class number, so return "" here.
    return QString();
}

namespace {
// Each object type has a CATEGORY (OP2 hardcodes it — it's not in building.txt) that picks the
// portrait icon (files in ui/tech/). GuardPosts → Weapon; the Command Center → the player's
// faction logo; otherwise a per-building category. Returns the icon file STEM (note the original
// "Tech_Strcture" misspelling is the real filename). Unknown buildings fall back to Structure,
// vehicles to Vehicle. (Some categories are best-guess — easy to retune.)
QString techIconStem(const QString& name, bool eden, bool isBuilding, bool isGuardPost) {
    if (isGuardPost) return QStringLiteral("Tech_Weapon");
    // TODO(vehicles): vehicles should have proper per-type categories. For now they all use the
    // weapon icon as a stand-in (per the user) until the per-vehicle mapping is decided.
    if (!isBuilding) return QStringLiteral("Tech_Weapon");
    // Faction-logo buildings → the player's faction emblem (Eden vs Plymouth).
    if (name == QStringLiteral("CommandCenter") || name == QStringLiteral("TradeCenter") ||
        name == QStringLiteral("LightTower"))
        return eden ? QStringLiteral("Tech_Eden") : QStringLiteral("Tech_Plymouth");
    static const QHash<QString, QString> kCategory = {
        // Power
        {"Tokamak","Tech_Power"}, {"MHDGenerator","Tech_Power"},
        {"SolarPowerArray","Tech_Power"}, {"GeothermalPlant","Tech_Power"},
        // Metals / ore (mines, smelters, storage, magma)
        {"CommonOreMine","Tech_Metal"}, {"RareOreMine","Tech_Metal"},
        {"CommonOreSmelter","Tech_Metal"}, {"RareOreSmelter","Tech_Metal"},
        {"CommonStorage","Tech_Metal"}, {"RareStorage","Tech_Metal"}, {"MagmaWell","Tech_Metal"},
        // Food
        {"Agridome","Tech_Food"},
        // Science (labs)
        {"BasicLab","Tech_Science"}, {"StandardLab","Tech_Science"}, {"AdvancedLab","Tech_Science"},
        // Colonists (habitation + morale + university + consumer goods)
        {"Residence","Tech_Colonists"}, {"ReinforcedResidence","Tech_Colonists"},
        {"AdvancedResidence","Tech_Colonists"}, {"Nursery","Tech_Colonists"},
        {"Forum","Tech_Colonists"}, {"RecFacility","Tech_Colonists"},
        {"University","Tech_Colonists"}, {"ConsumerFactory","Tech_Colonists"},
        // Vehicle-related buildings
        {"VehicleFactory","Tech_Vehicle"}, {"Garage","Tech_Vehicle"}, {"GORF","Tech_Vehicle"},
        {"RobotCommand","Tech_Vehicle"},
        // Arachnid factory (its own icon)
        {"ArachnidFactory","Tech_Arachnid"},
        // Structures / misc
        {"StructureFactory","Tech_Strcture"},
        // Space program
        {"Spaceport","Tech_Starship"}, {"Observatory","Tech_Space"},
        // Emergency / disaster response / medical
        {"DIRT","Tech_Emergency"}, {"MeteorDefense","Tech_Emergency"},
        {"MedicalCenter","Tech_Emergency"},
    };
    const auto it = kCategory.constFind(name);
    return it != kCategory.constEnd() ? it.value() : QStringLiteral("Tech_Strcture");
}
} // namespace

// Refresh selectedInfo_ from the currently-selected handle: look up the World unit + its placed
// render object and pack every panel field into the map. Marks the selection invalid (and clears
// the handle) if the object no longer exists. Callers emit selectionChanged() afterwards.
void TilesetView::rebuildSelectedInfo() {
    QVariantMap m;
    m[QStringLiteral("valid")] = false;
    const op2::sim::Unit* su = world_.get(selectedId_);
    if (selectedId_.valid() && su) {
        // Re-find the placed render object (its name/status/weapon) by the stable handle.
        const PlacedBuilding* pb = nullptr;
        const PlacedUnit* pu = nullptr;
        for (const PlacedBuilding& b : placed_)
            if (b.sourceId == selectedId_) { pb = &b; break; }
        if (!pb)
            for (const PlacedUnit& u : placedUnits_)
                if (u.sourceId == selectedId_) { pu = &u; break; }
        if (pb || pu) {
            const bool isGP = pu && pu->isGuardPost;
            const bool isBuilding = pb || isGP;
            const bool idle = pb ? pb->idle : pu->idle;
            const bool disabled = pb ? pb->disabled : pu->disabled;
            const QString name = pb ? pb->name
                : (su->typeId < nameByTypeId_.size() ? nameByTypeId_[su->typeId]
                                                     : QStringLiteral("Unit"));
            const op2::sim::UnitType& t = world_.type(su->typeId);
            const char* kindStr = "Object";
            switch (isGP ? op2::sim::UnitKind::Building : t.kind) {
                case op2::sim::UnitKind::Building: kindStr = "Structure"; break;
                case op2::sim::UnitKind::Vehicle:  kindStr = "Vehicle";   break;
                case op2::sim::UnitKind::Weapon:   kindStr = "Weapon";    break;
                default: break;
            }
            m[QStringLiteral("valid")]      = true;
            m[QStringLiteral("name")]       = name;
            m[QStringLiteral("owner")]      = su->owner + 1;        // 1-based player for the title
            m[QStringLiteral("idx")]        = int(selectedId_.index); // stable per-object number
            m[QStringLiteral("gen")]        = int(selectedId_.generation); // handle generation
            m[QStringLiteral("cellX")]      = su->tileX;            // 0-based map cell
            m[QStringLiteral("cellY")]      = su->tileY;
            m[QStringLiteral("hp")]         = su->hitPoints;
            m[QStringLiteral("maxHp")]      = t.maxHitPoints;
            m[QStringLiteral("armor")]      = t.armor;
            m[QStringLiteral("armorName")]  = armorClassName(t.armor);
            m[QStringLiteral("power")]      = t.powerRequired;       // building.txt requirements
            m[QStringLiteral("workers")]    = t.workersRequired;
            m[QStringLiteral("scientists")] = t.scientistsRequired;
            m[QStringLiteral("weapon")]     = pb ? pb->weapon : pu->weapon;
            m[QStringLiteral("idle")]       = idle;
            m[QStringLiteral("disabled")]   = disabled;
            m[QStringLiteral("status")]     = idle ? QStringLiteral("Idle")
                                    : (disabled ? QStringLiteral("Disabled")
                                                : QStringLiteral("Active"));
            m[QStringLiteral("kind")]       = QString::fromLatin1(kindStr);
            m[QStringLiteral("isBuilding")] = isBuilding;
            m[QStringLiteral("isGuardPost")]= isGP;
            const bool eden = (su->flags & op2::sim::UnitFlag::IsEden);
            m[QStringLiteral("iconName")]   = techIconStem(name, eden, isBuilding, isGP);
            m[QStringLiteral("friendly")]   = pb ? pb->friendly : pu->friendly;
        }
    }
    if (!m.value(QStringLiteral("valid")).toBool()) selectedId_ = {}; // object gone
    selectedInfo_ = m;
}

void TilesetView::selectAt(qreal mouseX, qreal mouseY) {
    if (!mapLoaded_) { clearSelection(); return; }
    const int tx = static_cast<int>(std::floor((cameraX_ + mouseX / zoom_) / T));
    const int ty = static_cast<int>(std::floor((cameraY_ + mouseY / zoom_) / T));
    // Topmost building whose footprint covers the tile (only mission objects carry a World
    // handle; the static parade has none and isn't selectable).
    for (auto it = placed_.rbegin(); it != placed_.rend(); ++it) {
        const PlacedBuilding& pb = *it;
        if (tx < pb.tileX || tx >= pb.tileX + pb.fpX ||
            ty < pb.tileY || ty >= pb.tileY + pb.fpY) continue;
        if (!pb.sourceId.valid()) continue;
        selectedId_ = pb.sourceId;
        rebuildSelectedInfo();
        emit selectionChanged();
        update();
        return;
    }
    // Units (GuardPosts + vehicles), hit-tested via their World cell.
    for (auto it = placedUnits_.rbegin(); it != placedUnits_.rend(); ++it) {
        const op2::sim::Unit* su = world_.get(it->sourceId);
        if (!su || su->tileX != tx || su->tileY != ty) continue;
        selectedId_ = it->sourceId;
        rebuildSelectedInfo();
        emit selectionChanged();
        update();
        return;
    }
    clearSelection(); // empty ground → deselect
}

// Drop the current selection (empty-ground click, right-click, or a map switch).
void TilesetView::clearSelection() {
    if (!selectedId_.valid() && selectedInfo_.isEmpty()) return;
    selectedId_ = {};
    selectedInfo_.clear();
    emit selectionChanged();
    update();
}

void TilesetView::focusUnit(quint32 index, quint32 generation) {
    const op2::sim::UnitId id{index, generation};
    const op2::sim::Unit* su = world_.get(id);
    if (!su) return; // stale/dead handle
    selectedId_ = id;
    rebuildSelectedInfo();
    emit selectionChanged();
    // Centre the camera on the object's tile (centerOnMap clamps + repaints).
    centerOnMap(su->tileX * T + T / 2.0, su->tileY * T + T / 2.0);
}

void TilesetView::setSelectedIdle(bool on) {
    if (!selectedId_.valid()) return;
    for (PlacedBuilding& pb : placed_)
        if (pb.sourceId == selectedId_) {
            setObjectStatus(&pb, nullptr, /*disabledFlag=*/false, on);
            return;
        }
    for (PlacedUnit& pu : placedUnits_)
        if (pu.sourceId == selectedId_) {
            if (!pu.isGuardPost) return;  // only structures/GuardPosts can be idled
            setObjectStatus(nullptr, &pu, /*disabledFlag=*/false, on);
            return;
        }
}

void TilesetView::scrollTick() {
    const qreal speed = static_cast<qreal>(scrollSpeed_);
    qreal vx = 0;
    qreal vy = 0;

    if (keyLeft_)  vx -= speed;
    if (keyRight_) vx += speed;
    if (keyUp_)    vy -= speed;
    if (keyDown_)  vy += speed;

    if (edgeScrollEnabled_) {
      if (QQuickWindow* w = window()) {
        if (w->isActive()) {
            const QPoint p = w->mapFromGlobal(QCursor::pos());
            const int winW = w->width();
            const int winH = w->height();
            const bool inside = (p.x() >= 0 && p.x() < winW &&
                                 p.y() >= 0 && p.y() < winH);
            if (inside || edgeScrollOutside_) {
                if (p.x() < kEdgeMargin)               vx -= speed;
                else if (p.x() >= winW - kEdgeMargin)  vx += speed;
                if (p.y() < kEdgeMargin)               vy -= speed;
                else if (p.y() >= winH - kEdgeMargin)  vy += speed;
            }
        }
      }
    }

    if (vx != 0 || vy != 0) {
        cameraX_ += vx;
        cameraY_ += vy;
        clampCamera();
        update();
        emit cameraChanged();
    }
}

bool TilesetView::blitUnitSprite(int group, QImage* img, int* ax, int* ay) const {
    if (group < 0 || group >= art_.groupCount()) return false;
    const int fs = art_.groupFrameStart(group);
    if (fs < 0) return false;
    // Units use the MobileShadows toggle (separate from building shadows).
    *img = art_.frameToQImage(fs, ax, ay, mobileShadowsEnabled_);
    return !img->isNull();
}


void TilesetView::setAmbientStructureEnabled(bool e) {
    if (ambientStructureEnabled_ == e) return;
    ambientStructureEnabled_ = e;
    if (!e) { // hold every building on its base (Normal) frame
        for (PlacedBuilding& pb : placed_) {
            pb.curAnim = 0; pb.frame = 0; pb.frameAccum = 0.0;
            pb.gapRemaining = 0; pb.playIdx = 0;
        }
        update();
    }
    emit ambientStructureEnabledChanged();
}

void TilesetView::setAmbientUnitEnabled(bool e) {
    if (ambientUnitEnabled_ == e) return;
    ambientUnitEnabled_ = e;
    if (!e) { // rebuild static: turrets back to hull facing (mission units are already static)
        if (missionMode_) placeFromWorld(); else placeAllUnits();
        update();
    }
    emit ambientUnitEnabledChanged();
}

void TilesetView::setAmbientResourceEnabled(bool e) {
    if (ambientResourceEnabled_ == e) return;
    ambientResourceEnabled_ = e;
    if (!e) { // hold every Gaia sprite on its base frame
        for (PlacedSprite& ps : placedSprites_) { ps.frame = 0; ps.frameAccum = 0.0; }
        update();
    }
    emit ambientResourceEnabledChanged();
}

void TilesetView::setOverlayEnabled(bool e) {
    if (overlayEnabled_ == e) return;
    overlayEnabled_ = e;
    update();
    emit overlayEnabledChanged();
}


void TilesetView::updateHover(qreal mouseX, qreal mouseY) {
    hoverCursorX_ = mouseX;
    hoverCursorY_ = mouseY;
    int tx = 0;
    int ty = 0;
    QString info;
    // The object under the cursor (for the delayed corner-box + HP bar + tooltip).
    op2::sim::UnitId objId{};
    QString objName;
    QRect objBox;
    int objHp = 0, objMaxHp = 0;
    bool objFriendly = true;
    if (mapLoaded_) {
        const int cx = static_cast<int>(std::floor((cameraX_ + mouseX / zoom_) / T));
        const int cy = static_cast<int>(std::floor((cameraY_ + mouseY / zoom_) / T));
        if (cx >= 0 && cy >= 0 && cx < map_.width() && cy < map_.height()) {
            tx = cx + 1; // OP2 tiles are 1-based; top-left is 1,1
            ty = cy + 1;
            // Object under the cursor → "Name  HP cur/max", read from the World object + its
            // UnitType (the parade has no World object, so it shows just the name).
            auto hpStr = [&](const QString& name, const op2::sim::Unit* su) {
                if (!su) return name;
                const int mx = world_.type(su->typeId).maxHitPoints;
                return mx > 0 ? name + QStringLiteral("  HP %1/%2").arg(su->hitPoints).arg(mx) : name;
            };
            for (const PlacedBuilding& pb : placed_) {
                if (cx >= pb.tileX && cx < pb.tileX + pb.fpX &&
                    cy >= pb.tileY && cy < pb.tileY + pb.fpY) {
                    const op2::sim::Unit* su = world_.get(pb.sourceId);
                    info = hpStr(pb.name, su);
                    objId = pb.sourceId; objName = pb.name; objFriendly = pb.friendly;
                    objBox = QRect(pb.tileX * T, pb.tileY * T, pb.fpX * T, pb.fpY * T);
                    if (su) { objHp = su->hitPoints; objMaxHp = world_.type(su->typeId).maxHitPoints; }
                    break;
                }
            }
            if (info.isEmpty())  // units: GuardPosts + vehicles, hit-tested via their World cell
                for (const PlacedUnit& pu : placedUnits_) {
                    const op2::sim::Unit* su = world_.get(pu.sourceId);
                    if (su && su->tileX == cx && su->tileY == cy) {
                        const QString nm = (su->typeId < nameByTypeId_.size())
                                               ? nameByTypeId_[su->typeId] : QStringLiteral("Unit");
                        info = hpStr(nm, su);
                        objId = pu.sourceId; objName = nm; objFriendly = pu.friendly;
                        objBox = QRect(cx * T, cy * T, T, T);
                        objHp = su->hitPoints; objMaxHp = world_.type(su->typeId).maxHitPoints;
                        break;
                    }
                }
        }
    }

    const bool infoChanged = (tx != hoverTileX_ || ty != hoverTileY_ || info != hoverInfo_);
    hoverTileX_ = tx;
    hoverTileY_ = ty;
    hoverInfo_ = info;

    // Delayed-hover tracking: when the object under the cursor changes, hide the decorations and
    // restart the dwell timer; when it fires, hoverActive_ turns on (corner box + HP bar + tooltip).
    if (objId != hoverId_) {
        hoverId_ = objId;
        hoverName_ = objName;
        hoverBox_ = objBox;
        hoverHp_ = objHp; hoverMaxHp_ = objMaxHp; hoverFriendly_ = objFriendly;
        hoverActive_ = false;
        if (objId.valid()) hoverDelayTimer_.start(); else hoverDelayTimer_.stop();
        update();          // hide the previous box immediately
        emit hoverChanged();
    } else {
        if (objId.valid()) { hoverBox_ = objBox; hoverHp_ = objHp; hoverMaxHp_ = objMaxHp; }
        // Same object: keep the tooltip following the cursor while active, and refresh the status bar.
        if (infoChanged || hoverActive_) emit hoverChanged();
    }
}

void TilesetView::hoverMoveEvent(QHoverEvent* event) {
    updateHover(event->position().x(), event->position().y());
}

void TilesetView::hoverLeaveEvent(QHoverEvent* /*event*/) {
    hoverDelayTimer_.stop();
    const bool had = hoverActive_ || hoverId_.valid() || hoverTileX_ != 0 || hoverTileY_ != 0;
    hoverTileX_ = 0;
    hoverTileY_ = 0;
    hoverInfo_.clear();
    hoverId_ = {};
    hoverActive_ = false;
    if (had) { update(); emit hoverChanged(); }
}

void TilesetView::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Left:  keyLeft_ = true; break;
        case Qt::Key_Right: keyRight_ = true; break;
        case Qt::Key_Up:    keyUp_ = true; break;
        case Qt::Key_Down:  keyDown_ = true; break;
        // +/- zoom about the viewport centre (matches OP2). '+' is usually Shift+'='.
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomAt(1.0, width() / 2.0, height() / 2.0);
            break;
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            zoomAt(-1.0, width() / 2.0, height() / 2.0);
            break;
        default: QQuickPaintedItem::keyPressEvent(event); return;
    }
    event->accept();
}

void TilesetView::keyReleaseEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        event->ignore();
        return;
    }
    switch (event->key()) {
        case Qt::Key_Left:  keyLeft_ = false; break;
        case Qt::Key_Right: keyRight_ = false; break;
        case Qt::Key_Up:    keyUp_ = false; break;
        case Qt::Key_Down:  keyDown_ = false; break;
        default: QQuickPaintedItem::keyReleaseEvent(event); return;
    }
    event->accept();
}

void TilesetView::paintTilesetGrid(QPainter* painter) {
    if (tilesets_.wellCount() == 0) {
        painter->setPen(Qt::gray);
        painter->drawText(boundingRect(), Qt::AlignCenter,
                          QStringLiteral("No tilesets loaded.\nCheck the Outpost2 path in outpost2.ini."));
        return;
    }
    const int cols = qMax(1, static_cast<int>(width()) / T);
    int col = 0;
    int row = 0;
    for (int w = 0; w < tilesets_.wellCount(); ++w) {
        const int n = tilesets_.tileCount(w);
        for (int t = 0; t < n; ++t) {
            const int y = row * T;
            if (y >= static_cast<int>(height())) return;
            tilesets_.drawTile(*painter, QRect(col * T, y, T, T), w, t);
            if (++col >= cols) {
                col = 0;
                ++row;
            }
        }
    }
}
