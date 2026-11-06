#pragma once
#include <QHash>
#include <QImage>
#include <QQuickPaintedItem>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <map>
#include <vector>

#include "artfile.h"
#include "buildingcatalog.h"
#include "missionloader.h"
#include "unitcatalog.h"
#include "op2/sim/game.h"
#include "op2/sim/map.h"
#include "op2/sim/sheet.h"
#include "op2/sim/world.h"
#include "tileset.h"

// =============================================================================
// TilesetView  —  QML item that renders the map into the game view
// =============================================================================
//
// World contract: the op2::sim::World is the single source of truth for game
// state, and this view is a READ-ONLY observer of it. The view reads units +
// types freely (paint, hover, selection, reports), but never writes RUNNING
// World state itself — every gameplay mutation goes through the sim command
// layer (op2/sim/command.h): the view builds a CommandPacket and
// applyCommand() performs the write, exactly as a network peer's packet
// would. The one exception is debugSetDisabled(), the command layer's own
// explicit backdoor for the not-yet-simulated Disabled condition. Mission
// LOADING is setup, not gameplay: loadMission()/populateType() construct the
// World (spawn/setType), like OP2's InitProc. The cached status flags on
// PlacedBuilding/PlacedUnit are render-side copies refreshed FROM the World
// after a command lands, never authored here.
//
// Loads the map on startup and draws it tile-by-tile (culled to the visible
// range). RTS-style camera:
//   * Edge scroll (window-relative; right edge is past the sidebar).
//   * edgeScrollOutside: keep scrolling past an edge even off-window (OP2).
//   * Arrow keys scroll too.
//   * scrollSpeed: pixels per tick.
//   * Zoom: one zoom-out level (mouse wheel), gated by zoomEnabled.
//   * No click-drag.
// Reports the hovered tile (1-based) for the status bar, builds a downscaled
// minimap image, and exposes the camera so the minimap can draw its view box.

class TilesetView : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int hoverTileX READ hoverTileX NOTIFY hoverChanged)
    Q_PROPERTY(int hoverTileY READ hoverTileY NOTIFY hoverChanged)
    // Name + HP of the building/GuardPost under the cursor ("" if none) — a lightweight
    // peek at object stats until there's a proper selection/info panel.
    Q_PROPERTY(QString hoverInfo READ hoverInfo NOTIFY hoverChanged)
    // OP2 delayed hover: after ~1s over an object a light corner-bracket box + HP bar are drawn
    // (in paint) and a name tooltip shows at the cursor (QML). hoverObjActive gates the tooltip;
    // hoverObjName is the type name; hoverCursorX/Y is the cursor position for the tooltip.
    Q_PROPERTY(bool hoverObjActive READ hoverObjActive NOTIFY hoverChanged)
    Q_PROPERTY(QString hoverObjName READ hoverObjName NOTIFY hoverChanged)
    Q_PROPERTY(int hoverCursorX READ hoverCursorX NOTIFY hoverChanged)
    Q_PROPERTY(int hoverCursorY READ hoverCursorY NOTIFY hoverChanged)
    // The currently selected object — its stats for the selection detail panel (the "7th
    // view"). selectedInfo is a map of fields (name, kind, hp, maxHp, armor, status, weapon,
    // owner, idx, idle, disabled, isBuilding, isGuardPost); selectedValid is false when
    // nothing is selected. Populated by selectAt(); the panel binds to these.
    Q_PROPERTY(QVariantMap selectedInfo READ selectedInfo NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedValid READ selectedValid NOTIFY selectionChanged)
    // The local player's factories / labs, for the Factories and Labs command-report panels.
    // Each is a list of { title, status, idx, gen } maps; rebuilt with the mission (reportsChanged).
    Q_PROPERTY(QVariantList factoriesReport READ factoriesReport NOTIFY reportsChanged)
    Q_PROPERTY(QVariantList labsReport READ labsReport NOTIFY reportsChanged)
    // Colony economy summary for the Resources Report (the "Summary" sub-report). A map of fields;
    // only the mine/smelter counts are live now — the rest are 0/placeholder until the economy sim.
    Q_PROPERTY(QVariantMap resourcesReport READ resourcesReport NOTIFY reportsChanged)
    // "Colonists Status" sub-report (Alt+C) — population / workers / scientists breakdown. All
    // sim-blocked (population sim) → 0 for now.
    Q_PROPERTY(QVariantMap colonistsReport READ colonistsReport NOTIFY reportsChanged)
    // "Morale Status" sub-report (Alt+M) — morale level + the factors that drive it. Mostly
    // sim-blocked; the Nursery/University/GORF "Operational" flags are derived (player owns an
    // active one). moraleStructures = the STRUCTURES drill-down: morale buildings + live status.
    Q_PROPERTY(QVariantMap moraleReport READ moraleReport NOTIFY reportsChanged)
    Q_PROPERTY(QVariantList moraleStructures READ moraleStructures NOTIFY reportsChanged)
    // The STRUCTURES drill-down of Colonists Status — colonist buildings (University/Nursery/Medical
    // Center) with live Active/Idle status.
    Q_PROPERTY(QVariantList colonistStructures READ colonistStructures NOTIFY reportsChanged)
    // Event log for the Communications panel — newest-first { mark, text, warn } messages. Empty
    // until the event/message system exists (births, deaths, training, structure-disabled, …).
    Q_PROPERTY(QVariantList commsMessages READ commsMessages NOTIFY reportsChanged)
    // Satellite status for the Space Program panel — { name, operational } per satellite. Default
    // "Not Operational" until the space-program sim tracks launches.
    Q_PROPERTY(QVariantList spaceReport READ spaceReport NOTIFY reportsChanged)
    // Space Program → Starship page — the starship-operation components ({ name, status, done }).
    // Research/build state is sim-blocked → "Not Researched" for now.
    Q_PROPERTY(QVariantList starshipReport READ starshipReport NOTIFY reportsChanged)
    Q_PROPERTY(bool edgeScrollOutside READ edgeScrollOutside
                   WRITE setEdgeScrollOutside NOTIFY edgeScrollOutsideChanged)
    Q_PROPERTY(int scrollSpeed READ scrollSpeed
                   WRITE setScrollSpeed NOTIFY scrollSpeedChanged)
    Q_PROPERTY(bool zoomEnabled READ zoomEnabled
                   WRITE setZoomEnabled NOTIFY zoomEnabledChanged)
    Q_PROPERTY(bool shadowsEnabled READ shadowsEnabled
                   WRITE setShadowsEnabled NOTIFY shadowsEnabledChanged)
    Q_PROPERTY(bool mobileShadowsEnabled READ mobileShadowsEnabled
                   WRITE setMobileShadowsEnabled NOTIFY mobileShadowsEnabledChanged)
    Q_PROPERTY(bool gridEnabled READ gridEnabled
                   WRITE setGridEnabled NOTIFY gridEnabledChanged)
    Q_PROPERTY(bool labelsEnabled READ labelsEnabled
                   WRITE setLabelsEnabled NOTIFY labelsEnabledChanged)
    Q_PROPERTY(bool ambientStructureEnabled READ ambientStructureEnabled
                   WRITE setAmbientStructureEnabled NOTIFY ambientStructureEnabledChanged)
    Q_PROPERTY(bool ambientUnitEnabled READ ambientUnitEnabled
                   WRITE setAmbientUnitEnabled NOTIFY ambientUnitEnabledChanged)
    Q_PROPERTY(bool ambientResourceEnabled READ ambientResourceEnabled
                   WRITE setAmbientResourceEnabled NOTIFY ambientResourceEnabledChanged)
    Q_PROPERTY(bool overlayEnabled READ overlayEnabled
                   WRITE setOverlayEnabled NOTIFY overlayEnabledChanged)
    Q_PROPERTY(bool edgeScrollEnabled READ edgeScrollEnabled
                   WRITE setEdgeScrollEnabled NOTIFY edgeScrollEnabledChanged)
public:
    explicit TilesetView(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    // Advance all animation by gameTicks game ticks (0..N). Called from QML once per
    // fixed-cadence frame with the GameClock's ticksThisFrame, so animation scales
    // with game speed independent of render load. gameTicks==0 means no advance.
    Q_INVOKABLE void advanceAnimation(int gameTicks = 1);

    int hoverTileX() const { return hoverTileX_; }
    int hoverTileY() const { return hoverTileY_; }
    QString hoverInfo() const { return hoverInfo_; }
    bool hoverObjActive() const { return hoverActive_; }
    QString hoverObjName() const { return hoverName_; }
    int hoverCursorX() const { return static_cast<int>(hoverCursorX_); }
    int hoverCursorY() const { return static_cast<int>(hoverCursorY_); }

    // Selection (the detail panel). selectAt picks the topmost object under the cursor
    // (building → GuardPost → vehicle); clicking empty ground clears it, or, with one of the
    // local player's vehicles selected, orders it there (a ctMoMove packet). The selection box
    // is drawn around it; setSelectedIdle toggles the selected structure's player-Idle flag
    // (the panel's IDLE action). Vehicles can't be idled (no-op).
    QVariantMap selectedInfo() const { return selectedInfo_; }
    bool selectedValid() const { return selectedInfo_.value(QStringLiteral("valid")).toBool(); }
    Q_INVOKABLE void selectAt(qreal mouseX, qreal mouseY);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setSelectedIdle(bool on);
    // Select the unit with this World handle and centre the camera on it. Used by the report
    // panels (e.g. double-clicking a factory) to jump to + select an object. No-op if the handle
    // is stale/dead.
    Q_INVOKABLE void focusUnit(quint32 index, quint32 generation);

    // Command-report data: the LOCAL player's factories / labs as { title: "Name # p-NN", status,
    // idx, gen } maps. Production/research is sim-blocked → status is "Producing/Researching Nothing"
    // for now; idx/gen are the World handle (double-click an entry → focusUnit).
    QVariantList factoriesReport() const;
    QVariantList labsReport() const;
    QVariantMap resourcesReport() const;
    QVariantMap colonistsReport() const;
    QVariantMap moraleReport() const;
    QVariantList moraleStructures() const;
    QVariantList colonistStructures() const;
    QVariantList commsMessages() const;
    QVariantList spaceReport() const;
    QVariantList starshipReport() const;

    bool edgeScrollOutside() const { return edgeScrollOutside_; }
    void setEdgeScrollOutside(bool on) {
        if (edgeScrollOutside_ != on) {
            edgeScrollOutside_ = on;
            emit edgeScrollOutsideChanged();
        }
    }

    int scrollSpeed() const { return scrollSpeed_; }
    void setScrollSpeed(int s) {
        if (s < 1) s = 1;
        if (s > 100) s = 100;
        if (scrollSpeed_ != s) {
            scrollSpeed_ = s;
            emit scrollSpeedChanged();
        }
    }

    bool zoomEnabled() const { return zoomEnabled_; }
    void setZoomEnabled(bool e);

    bool shadowsEnabled() const { return shadowsEnabled_; }
    void setShadowsEnabled(bool e); // re-composites all buildings

    bool mobileShadowsEnabled() const { return mobileShadowsEnabled_; }
    void setMobileShadowsEnabled(bool e); // re-composites all units

    bool gridEnabled() const { return gridEnabled_; }
    void setGridEnabled(bool e) {
        if (gridEnabled_ != e) { gridEnabled_ = e; update(); emit gridEnabledChanged(); }
    }

    bool labelsEnabled() const { return labelsEnabled_; }
    void setLabelsEnabled(bool e) {
        if (labelsEnabled_ != e) { labelsEnabled_ = e; update(); emit labelsEnabledChanged(); }
    }

    bool ambientStructureEnabled() const { return ambientStructureEnabled_; }
    void setAmbientStructureEnabled(bool e); // off => hold buildings on base frame
    bool ambientUnitEnabled() const { return ambientUnitEnabled_; }
    void setAmbientUnitEnabled(bool e); // off => reset turrets to their hull facing
    bool ambientResourceEnabled() const { return ambientResourceEnabled_; }
    void setAmbientResourceEnabled(bool e); // off => hold beacons/vents on base frame
    bool overlayEnabled() const { return overlayEnabled_; }
    void setOverlayEnabled(bool e);         // building status light (green = active)

    // Edge/mouse scroll: when off, only the arrow keys pan (no window-edge scroll).
    bool edgeScrollEnabled() const { return edgeScrollEnabled_; }
    void setEdgeScrollEnabled(bool e) {
        if (edgeScrollEnabled_ != e) { edgeScrollEnabled_ = e; emit edgeScrollEnabledChanged(); }
    }

    // Save the current game-view (terrain + grid + buildings) to a PNG under
    // <appdir>/exports/. Returns the path written (empty on failure).
    Q_INVOKABLE QString exportViewport();

    // Save the ENTIRE map (every tile, building and unit, 1:1, ignoring camera/zoom)
    // to a PNG under <appdir>/exports/. Returns the path written (empty on failure).
    Q_INVOKABLE QString exportFullMap();

    // Zoom toggle at a point (called from the QML WheelHandler). deltaY < 0
    // (wheel down) zooms out; otherwise zooms back in.
    Q_INVOKABLE void zoomAt(qreal deltaY, qreal x, qreal y);

    // Click a building/GuardPost to toggle a status flag (mouse coords in view px).
    // toggleDisabled=false flips player-Idle (gray light); true flips condition-Disabled
    // (red light). Either stops animation; the change is written back to the World object
    // (UnitFlag::Idle / ::Disabled). Left-click → Idle, right-click → Disabled in the UI.
    Q_INVOKABLE void toggleBuildingAt(qreal mouseX, qreal mouseY, bool toggleDisabled);

    // Survey a SINGLE mining beacon at a map cell — reveals its ore type + yield. This is
    // what a Robo-Surveyor does when it reaches the beacon's tile. No-op if there's no
    // unsurveyed mine there. Returns true if a mine was newly surveyed.
    Q_INVOKABLE bool surveyMineAt(int tileX, int tileY);
    // Survey ALL mining beacons on the map — what deploying an EDWARD Satellite does for a
    // player. (Beacons are Gaia/unowned and we render one player's view, so this reveals them
    // globally; the player arg is for future per-player visibility.) Returns the count revealed.
    Q_INVOKABLE int surveyAllMines(int player = -1);

    // Switch the displayed map at runtime (sidebar buttons). mapRel is relative to
    // <Outpost2>/OPU/maps/ (slashes either way). A .opm loads its mission into the
    // World and renders that; a plain .map shows the static catalog parade. Art and
    // the catalogs are loaded once at startup and reused.
    Q_INVOKABLE void loadMap(const QString& mapRel);

    // --- accessors for the minimap ---------------------------------------
    const QImage& minimapImage() const { return minimapImage_; }
    qreal cameraX() const { return cameraX_; }
    qreal cameraY() const { return cameraY_; }
    qreal viewWidthMapPx() const { return zoom_ > 0 ? width() / zoom_ : width(); }
    qreal viewHeightMapPx() const { return zoom_ > 0 ? height() / zoom_ : height(); }
    void centerOnMap(qreal mapX, qreal mapY);

signals:
    void hoverChanged();
    void edgeScrollOutsideChanged();
    void scrollSpeedChanged();
    void zoomEnabledChanged();
    void shadowsEnabledChanged();
    void mobileShadowsEnabledChanged();
    void gridEnabledChanged();
    void labelsEnabledChanged();
    void ambientStructureEnabledChanged();
    void ambientUnitEnabledChanged();
    void ambientResourceEnabledChanged();
    void overlayEnabledChanged();
    void edgeScrollEnabledChanged();
    void selectionChanged(); // selected object changed (detail panel + selection box)
    void reportsChanged();   // command-report data changed (mission (re)loaded)
    void cameraChanged(); // camera moved or zoom changed (minimap repaints)

protected:
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void paintMap(QPainter* painter);
    // Draws tiles/grid/buildings/units in map-pixel space (caller sets the transform).
    void paintMapContent(QPainter* painter, int firstCol, int firstRow,
                         int lastCol, int lastRow);
    void paintTilesetGrid(QPainter* painter);
    void clampCamera();
    void updateHover(qreal mouseX, qreal mouseY);
    void scrollTick();
    void buildMinimap();
    void placeAllBuildings();
    // Build one animated building (Normal + up to 2 ambients + optional floor pad) seated
    // on its footprint at (tileX,tileY); pushes a PlacedBuilding to placed_. Shared by the
    // parade and the .opm mission render.
    void placeBuildingInstance(const BuildingDef& def, int normalGroup, const QString& label,
                               int tileX, int tileY, int floorGroup, bool lastFrameOnly,
                               int amb1, int amb2);
    // Which .prt groups draw a building for one faction (input to placeBuildingInstance).
    struct BuildingArt {
        int normal = -1;            // Normal (operating) group; -1 = the faction has none
        int floor = -1;             // ground underlay group (the Garage pad); -1 = none
        bool lastFrameOnly = false; // static look: only the group's final frame
        int amb1 = -1, amb2 = -1;   // Ambient1/Ambient2 animation groups
    };
    // Resolve a building's per-faction art, INCLUDING the per-building special cases
    // (Garage, LightTower). This is the single authority shared by the catalog showcase
    // (placeAllBuildings) and the .opm mission render (placeFromWorld) — add new special
    // cases here and both paths pick them up; they used to be duplicated and could drift.
    static BuildingArt resolveBuildingArt(const BuildingDef& def, bool eden);
    void placeAllUnits();
    // Re-blit a unit sprite group into img with its anchor (honours MobileShadows).
    bool blitUnitSprite(int group, QImage* img, int* ax, int* ay) const;
    // Step every combat unit's idle turret rotation one notch and re-seat it.
    void advanceUnitTurrets(int gameTicks);
    // Re-seat each placed vehicle on its World unit's pixel and hull facing after sim
    // ticks (re-blits the chassis when the facing changes). Returns false if a placed
    // unit's World object is gone, so the caller rebuilds the placed lists.
    bool syncVehicles();

    // Build a command-report list (factories/labs/…): the LOCAL player's buildings whose name is in
    // displayMap, as { title:"<Display> # p-NN", status:"<verb> Nothing", idx, gen }. Shared by
    // factoriesReport()/labsReport(). NN is a per-type instance counter.
    QVariantList buildingReport(const QHash<QString, QString>& displayMap,
                                const QString& verb) const;
    // Like buildingReport but the status line is the building's live OPERATING state
    // (Active/Idle/Disabled from its flags) — for the "Structures" drill-down lists.
    QVariantList structureList(const QHash<QString, QString>& displayMap) const;
    // True if the local player owns at least one ACTIVE building of this name (for the morale
    // "Nursery/University/GORF Operational" flags).
    bool hasActive(const QString& name) const;

    // --- .opm mission → World → render -------------------------------------
    // Parse a .opm and spawn its objects into world_ (returns false on failure).
    bool loadMission(const QString& opmPath);
    // Build placed_/placedUnits_ from the live World units (mission render path).
    void placeFromWorld();
    // Build placedSprites_ from missionResources_ (ore beacons, vents, fumaroles,
    // markers, wreckage). Called by placeFromWorld so a toggle rebuilds them too.
    void placeResources();
    // Advance the continuously-animating map sprites (vents/wreckage/markers).
    void advanceSprites(int gameTicks);

    // --- GuardPost: a static base platform + a rotating weapon turret (its OWN art,
    // distinct from vehicle turrets) — drawn like a fixed Lynx. The turret idle-wanders
    // (except the Starflare/Supernova bombs). Groups extracted from Outpost2.exe.
    // resolveGuardPostTurret returns a synthesised 16-direction UnitDef (cached for a
    // stable pointer) for weapon+faction; *rotates=false for the static bomb turrets.
    const UnitDef* resolveGuardPostTurret(const QString& weapon, const QString& faction,
                                          bool* rotates);
    // Build+append one GuardPost (base + turret) at a 0-based cell, facing 0..15.
    void placeGuardPost(const QString& faction, const QString& weapon, int cellX, int cellY,
                        int facing, const QString& label, bool idleRotate);
    // Catalog lookups (shared by the mission render path).
    const UnitDef* findUnitDef(const QString& name, const QString& faction) const;
    const UnitDef* findTurretDef(const QString& name, const QString& faction) const;
    // Resolve a combat unit's turret from chassis+weapon+faction (single/dual/bomb,
    // faction-split where applicable). Null for a weaponless/unknown combo.
    const UnitDef* resolveTurretDef(const QString& chassis, const QString& weapon,
                                    const QString& faction) const;
    // map_id name <-> World typeId registry (assigns on first use).
    quint16 typeIdFor(const QString& name);

    static constexpr int kEdgeMargin = 16;

    TilesetManager tilesets_;
    // The whole sim (clock, RNG, World, map). advanceAnimation steps it on a mission; the
    // view reads it and only changes it through queued command packets (and mission setup).
    // map_ and world_ name its parts.
    op2::sim::Game game_;
    op2::sim::Map& map_ = game_.map();
    bool mapLoaded_ = false;
    QString base_;        // [Game] Outpost2 base dir, kept for runtime map switching
    QImage minimapImage_; // mapW x mapH, one averaged pixel per tile

    // .opm mission: objects spawned into the World, rendered by placeFromWorld().
    // When missionMode_ is set, the map view shows the mission (not the static parade).
    op2::sim::World& world_ = game_.world();
    bool missionMode_ = false;
    QHash<QString, quint16> typeIdByName_; // map_id name -> World typeId
    QStringList nameByTypeId_;             // typeId -> map_id name
    // Map resources/markers/wreckage parsed from the .opm (kept so placeFromWorld can
    // rebuild them on a shadow/anim toggle, like the World units/buildings).
    std::vector<MissionResource> missionResources_;
    // Synthesised GuardPost turret defs (weapon|faction → 16-dir groups), node-based so
    // PlacedUnit.turretDef pointers stay valid as new ones are cached.
    std::map<QString, UnitDef> gpTurretCache_;
    // Ownership: the local (viewing) player + the set friendly to it (itself + allies),
    // from the .opm. The computer overlay lights only friendly objects.
    int localPlayer_ = 0;
    std::vector<int> friendlyPlayers_;
    bool isFriendlyOwner(int owner) const {
        if (friendlyPlayers_.empty()) return true; // no mission loaded (parade) → all friendly
        for (int p : friendlyPlayers_) if (p == owner) return true;
        return false;
    }

    // A simple positioned animation (ore beacon, magma vent, fumarole, marker,
    // wreckage). Unlike buildings these animate CONTINUOUSLY (no rest gap) — a vent
    // always plumes — or are a single static frame (ore beacons). Drawn at the tile
    // centre via the owning group's pixel displacement, like every other sprite.
    struct PlacedSprite {
        int originX = 0, originY = 0;     // tile-centre map px minus group displacement
        std::vector<QImage> frames;
        std::vector<int> delays;          // art per-frame delay (ticks); 0 ⇒ static
        std::vector<int> anchorX, anchorY;
        int frame = 0;
        double frameAccum = 0.0;
        QString name;                     // label (debug / parity with buildings)
        int cellX = 0, cellY = 0;         // 0-based map cell (for click hit-testing)
        int resourceIndex = -1;           // index into missionResources_ (-1 = none)
        bool isMine = false;              // a mining beacon (surveyable)
    };
    std::vector<PlacedSprite> placedSprites_;

    // Sprite animation ticks per game tick (the art's per-frame delay is in
    // finer-than-tick units). Read from [Render] AnimationSpeed in outpost2.ini.
    int animTicksPerGameTick_ = 7;
    // Ticks a building rests (holds its rest frame) between animation plays — the
    // pause before it animates again. Read from [Render] AnimationGap. Staggered
    // per building so they don't all animate in unison.
    int animGapTicks_ = 60;
    // [Render] AnimationAmbientStructure / AnimationAmbientUnit / AnimationAmbientResource —
    // master toggles for building idle/ambient animations, idle turret rotation on military
    // units, and the looping Gaia map objects (ore beacons, vents, fumaroles, markers,
    // wreckage). Read at startup; default on.
    bool ambientStructureEnabled_ = true;
    bool ambientUnitEnabled_ = true;
    bool ambientResourceEnabled_ = true;
    // [Render] Overlay — the building "computer overlay" status light (OP2's green dot,
    // a fixed art sprite, group 47). Drawn at each building's selection-box top-left+2
    // while the building is Active. The light sprite + its anchor are loaded once.
    bool overlayEnabled_ = true;
    QImage overlayLight_;            // group 47: green (Active)
    int overlayLightAX_ = 0;
    int overlayLightAY_ = 0;
    QImage overlayLightGray_;        // group 49: solid gray "off" dot (Idle)
    int overlayLightGrayAX_ = 0;
    int overlayLightGrayAY_ = 0;
    QImage overlayLightRed_;         // group 48 frame 0: red (Disabled by damage). Group 48 is
    int overlayLightRedAX_ = 0;      // the 8-frame FLASHING red — animate it later (HP-driven).
    int overlayLightRedAY_ = 0;
    // Composite the building's drop-shadow (art shadow pictures, type 4/5) under
    // each frame. Initialised from [Game] StationaryShadows; toggled at runtime
    // from the sidebar (re-composites every building's frames).
    bool shadowsEnabled_ = true;
    // Unit (mobile) drop-shadows — same 1bpp art shadows, separate toggle.
    // Initialised from [Game] MobileShadows; re-composites every unit sprite.
    bool mobileShadowsEnabled_ = true;
    bool gridEnabled_ = false; // draw the 32px tile grid over terrain, under buildings
    bool labelsEnabled_ = true; // draw building name labels under each building

    ArtFile art_;
    BuildingCatalog catalog_;
    // Stat sheets (base/sheets/*.txt) — per-type stats (HP, armor, …). Loaded once.
    op2::sim::Sheet buildingSheet_;
    op2::sim::Sheet vehicleSheet_;
    op2::sim::Sheet spaceSheet_; // space.txt — satellites / starship components (cost, build, research)

    // Space object (satellite / starship component / launch vehicle) — per-type data parsed from
    // space.txt into memory at load. `name` is the EXE display name (space.txt only has codes).
    struct SpaceObject {
        QString code;       // space.txt Code_Name (EDWARD, SOLAR_SAT, SPACE_1..15, SULV, RLV, …)
        QString name;       // display name (from the EXE / tech tree)
        int commonCost = 0; // Common ore cost
        int rareCost = 0;   // Rare ore cost
        int buildTime = 0;  // build time
        QString research;   // research topic code that unlocks it ("00000" = none)
    };
    std::vector<SpaceObject> spaceObjects_;            // all rows, in space.txt order
    QHash<QString, int> spaceObjectByName_;            // display name → index into spaceObjects_
    // Parse space.txt (spaceSheet_) into spaceObjects_ once at load. Maps the generic codes to
    // their display names (the names live in the EXE, not the sheet).
    void populateSpaceObjects();
    // Look up a space object's costs by display name (nullptr if unknown).
    const SpaceObject* spaceObject(const QString& name) const;
    // Build the UnitType (kind, footprint, HP, armor) for a typeId from the catalog +
    // sheets and store it in the World (world_.setType). Idempotent; called at load.
    void populateType(quint16 typeId, const QString& name);

    // One complete animation a building can play (a .prt group: Normal, or an
    // Ambient). Each is a self-contained sequence of fully-composited frames
    // (base + that group's moving part). Frames are pre-composited so paint()
    // just blits. anchorX/Y keep the body fixed as the per-frame bbox changes.
    struct Anim {
        std::vector<QImage> frames;
        std::vector<int> anchorX, anchorY;
        std::vector<int> delays;
        int originX = 0;   // map-pixel where the frame's logical (0,0) lands
        int originY = 0;   // (from this group's pixel displacement)
    };

    // Every building from the catalog, placed on the map. OP2 plays exactly ONE
    // animation at a time (confirmed from Unit.DoAmbientAnimation @0x483580) — never
    // layered. The building plays one animation through, holds its rest frame for a
    // gap, then plays the next. `playOrder` interleaves Normal between the ambients
    // (e.g. [Normal, Amb1, Normal, Amb2]) so Normal stays dominant yet every ambient
    // is shown; OP2 itself picks via a per-position table, but for the catalog
    // showcase we cycle so nothing is hidden.
    struct PlacedBuilding {
        int tileX = 0;   // top-left footprint tile
        int tileY = 0;
        int fpX = 1;
        int fpY = 1;
        QString name;
        std::vector<Anim> anims;       // [0] = Normal; [1..] = Ambient1/2
        std::vector<int> playOrder;    // sequence of anim indices to cycle through
        int playIdx = 0;               // position in playOrder
        int curAnim = 0;               // anim currently playing/resting
        int frame = 0;
        double frameAccum = 0.0;       // fractional frames advanced so far
        int gapRemaining = 0;          // ticks left resting (paused) between plays
        // Optional ground underlay (e.g. the Garage floor / dock pad vehicles
        // drive onto) — a separate sprite drawn under the body, already seated at
        // its own map-pixel position. Null when the building has no floor.
        QImage floorImg;
        int floorDrawX = 0;
        int floorDrawY = 0;
        bool debug = false;        // trace this one's switches to the log
        // Object STATUS: Active (animates) vs Idle (holds its base frame, no ambient/
        // operating animation) — mirrors OP2's IsEnabled gate. The World unit is the
        // source of truth (UnitFlag::Idle); sourceId links back so a toggle persists.
        // Two INDEPENDENT status flags (as in OP2): idle = the player switched it off;
        // disabled = a condition (no power/workers/scientists, damage, EMP). A structure
        // operates (animates + green) only when NEITHER is set. Light: idle→gray (player
        // intent wins), else disabled→red, else green. active is the cached "operating".
        bool active = true;        // == !idle && !disabled
        bool idle = false;         // player-idled → gray light
        bool disabled = false;     // condition-disabled → red light
        bool friendly = true;      // owned by the local player or an ally — the computer
                                   // overlay only lights friendly buildings (true in the parade)
        QString weapon;            // weapon name for the detail panel (empty = unarmed structure)
        op2::sim::UnitId sourceId{}; // invalid for the static parade (no World object)
        // "Computer overlay" status light (OP2's dot): map px where the light sprite's
        // LOGICAL origin lands (selection-box corner + 2). Each light variant subtracts
        // its own anchor at paint time.
        bool hasLight = false;
        int lightDrawX = 0;
        int lightDrawY = 0;
    };
    std::vector<PlacedBuilding> placed_;
    long long animTick_ = 0;       // counts advanceAnimation() calls (game ticks)

    // Units rotate rather than animate-in-place: each placed instance is one
    // direction of one unit, pre-composited (chassis, plus a turret on top for
    // combat units — both at the same origin, per the OP2 draw order). We lay the
    // whole catalog out showing all 16 directions of every unit.
    UnitCatalog unitCatalog_;
    struct PlacedUnit {
        int originX = 0;     // map px where the rotation sprite's (0,0) lands
        int originY = 0;
        QImage chassis;
        int cAnchorX = 0, cAnchorY = 0;
        QImage turret;       // null if not a combat unit
        int tAnchorX = 0, tAnchorY = 0;
        // Turret's own (0,0) screen position. OP2 seats the turret on the hull via
        // the difference between the chassis and turret group pixel-displacements
        // (the mount offset), so this is originX/Y + (chassisDisp - turretDisp).
        int tOriginX = 0, tOriginY = 0;
        // Bomb weapons (Supernova/Starflare) have no dedicated Tiger dual sprite — OP2
        // draws the SAME single turret twice, stacked. When >0, paint a second turret
        // copy this many px above the first.
        int turretStackDy = 0;
        // Idle turret rotation (AnimationAmbientUnit): re-seat inputs. turretDef is
        // null for non-combat units (no idle rotation). Points into unitCatalog_
        // (stable for the app lifetime).
        const UnitDef* turretDef = nullptr;
        int turretRot = 0;          // current turret rotation index (0..15)
        int bodyRot = 0;            // the hull facing (idle wander can drift back toward it)
        int seatUpX = 0, seatUpY = 0; // the unit-pixel centre, for re-seating the turret
        int chassisGroup = 0;       // chassis frame supplies the (fixed) turret-mount offset
        int chassisFrame = -1;      // exact frame for the turret-mount offset; -1 ⇒ group's
                                    // first frame (vehicles). GuardPost uses its base's LAST
                                    // (construction-complete) frame, not the first.
        QString label;       // non-empty only on the row's first direction
        int labelX = 0, labelY = 0; // where to draw the row label (under the row)
        // GuardPost status (GuardPosts are weapon-bearing buildings rendered as units).
        // Unlike buildings, a GuardPost shows NO light when Active; when off it shows the
        // gray "off" dot (g49), or red (g48) if disabled by damage — centred on the post.
        bool isGuardPost = false;
        bool active = true;       // == !idle && !disabled
        bool idle = false;        // player-idled → gray dot
        bool disabled = false;    // condition-disabled → red dot
        bool friendly = true;     // own/allied → status light shown
        QString weapon;           // weapon name for the detail panel (GuardPosts/combat units)
        op2::sim::UnitId sourceId{};
        int centerX = 0, centerY = 0; // unit pixel (footprint centre) for the centred light
        // A mission vehicle's body and turret art (16 facings each), so syncVehicles can
        // re-blit them as the sim turns it. bodyDef is null for GuardPosts and the parade;
        // turretArt is set for bombs too (turretDef, which drives the idle wander, isn't).
        const UnitDef* bodyDef = nullptr;
        const UnitDef* turretArt = nullptr;
    };
    std::vector<PlacedUnit> placedUnits_;
    // Seat a vehicle's chassis at unit pixel upX/upY facing 0..15 (re-blitting it when the
    // facing changes), and its turret, if any, at turretRot.
    void seatVehicle(PlacedUnit& pu, int upX, int upY, int facing);

    // Change a structure's player-Idle / condition-Disabled status. Exactly one of
    // pb/pu is non-null. Mission objects (valid sourceId) route through the sim
    // command layer — Idle as a real ctMoIdle/ctMoUnidle CommandPacket, Disabled via
    // the layer's debug backdoor — then the cached render flags are refreshed FROM
    // the World result. Parade objects have no World unit, so their cached flags
    // toggle directly. Shared by toggleBuildingAt + setSelectedIdle.
    void setObjectStatus(PlacedBuilding* pb, PlacedUnit* pu, bool disabledFlag, bool on);

    // Camera = top-left of the viewport in map pixels; zoom = screen px per map px.
    qreal cameraX_ = 0;
    qreal cameraY_ = 0;
    qreal zoom_ = 1.0;

    QTimer scrollTimer_;
    bool keyLeft_ = false;
    bool keyRight_ = false;
    bool keyUp_ = false;
    bool keyDown_ = false;

    bool edgeScrollOutside_ = true;
    bool edgeScrollEnabled_ = true; // window/mouse edge-scroll on; off = arrow keys only
    int scrollSpeed_ = 12;
    bool zoomEnabled_ = true;

    int hoverTileX_ = 0;
    int hoverTileY_ = 0;
    QString hoverInfo_;

    // --- OP2 delayed hover (corner-bracket box + HP bar + name tooltip) ------
    op2::sim::UnitId hoverId_{};   // object under the cursor (invalid = none)
    QString hoverName_;            // its type name (tooltip text)
    QRect hoverBox_;               // its footprint box in MAP px (camera-transformed at paint)
    int hoverHp_ = 0, hoverMaxHp_ = 0; // for the HP bar
    bool hoverFriendly_ = true;
    bool hoverActive_ = false;     // the ~1s delay elapsed → draw the decorations + tooltip
    qreal hoverCursorX_ = 0, hoverCursorY_ = 0; // cursor (view px) for the tooltip
    QTimer hoverDelayTimer_;       // single-shot; fires hoverActive_ after the dwell delay
    void paintHover(QPainter* painter); // draws the corner box + HP bar (map space)

    // --- selection (detail panel + selection box) ---------------------------
    // The selected object is tracked by its stable World handle (survives a placed_/
    // placedUnits_ rebuild). rebuildSelectedInfo() rescans the placed lists for it and
    // refreshes selectedInfo_ (or marks it invalid if the object is gone). The selection
    // box in paint() is drawn around whichever placed object carries selectedId_.
    op2::sim::UnitId selectedId_{};
    QVariantMap selectedInfo_;
    void rebuildSelectedInfo();
    // Friendly name for an armor-class number (sheet "Armor Class"); "" if unmapped.
    static QString armorClassName(int cls);
};
