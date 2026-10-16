#pragma once
#include <QString>

#include <vector>

// =============================================================================
// MissionLoader  —  parse an OP2MissionEditor .opm into placeable objects
// =============================================================================
//
// The .opm is JSON (see D:\opu\op2opmtools\OPM_FORMAT.md). This loader uses Qt's
// QJsonDocument (the app layer has Qt; the op2maps/op2sim libs stay Qt-free), and
// flattens every player's pre-placed Units into a faction-tagged list the renderer
// turns into World units. Coordinates are the raw editor tile coords (same grid the
// editor shows on the .map; the +31/-1 LOCATION offset is an engine-export detail,
// not used for rendering the map as the editor displays it).

struct MissionObject {
    QString typeId;       // map_id name, e.g. "Lynx", "CommandCenter", "RoboDozer"
    int owner = 0;        // player index
    bool isEden = true;   // player faction
    int tileX = 0;
    int tileY = 0;
    int direction = 0;    // 0..7 (East, SouthEast, … NorthEast)
    QString cargoType;    // weapon (combat) / cargo (truck) / structure (ConVec) / "None"
    int cargoAmount = 0;
};

// A map resource/marker/wreckage object (under MasterVariant.TethysGame). These
// are 1×1 map features, not player-owned units: ore beacons, magma vents, fumaroles,
// objective markers, and salvageable wreckage. (See OP2's BeaconData/MarkerData/
// WreckageData; map_id MiningBeacon=0x51 / MagmaVent=0x52 / Fumarole=0x53.)
struct MissionResource {
    QString kind;       // "Beacon" | "Marker" | "Wreckage"
    QString subType;    // Beacon MapID (MiningBeacon/MagmaVent/Fumarole); Marker type
                        // (Circle/DNA/Beaker); empty for Wreckage (single sprite).
    QString oreType;    // beacons only: "Common" | "Rare" | "Random"
    QString barYield;   // beacons only: "Bar3" | "Bar2" | "Bar1" | "Random"
    int tileX = 0;      // 1-based game coords (caller subtracts 1, like units)
    int tileY = 0;
    // Mining beacons start UNSURVEYED — shown as a generic "1 bar" marker — until a
    // Robo-Surveyor reveals the ore type + yield. (Default: unsurveyed.)
    bool surveyed = false;
};

struct MissionData {
    QString mapName;       // LevelDetails.MapName (companion .map)
    QString description;
    std::vector<MissionObject> objects;
    std::vector<MissionResource> resources;
    // The viewing ("local") player + the players friendly to it (itself + allies). OP2
    // only shows the computer-overlay status light on your own and allied objects.
    int localPlayer = 0;
    std::vector<int> friendlyPlayers; // includes localPlayer
};

// Parse a .opm. Returns false (with *error) on open/parse failure.
bool loadOpmMission(const QString& path, MissionData* out, QString* error = nullptr);

// .opm Direction name -> 0..7 facing index (East=0 … NorthEast=7). Unknown -> 0.
int opmDirectionIndex(const QString& name);
