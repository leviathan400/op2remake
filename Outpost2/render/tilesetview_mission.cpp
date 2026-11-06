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
#include "op2/sim/movement.h"

namespace { constexpr int T = TilesetManager::kTileSize; } // namespace

// =============================================================================
// .opm mission → World → render
// =============================================================================

namespace {
// .opm map_id names → our unit-catalog names (the hyphenated Robo-* differ).
QString normalizeUnitName(const QString& opmName) {
    if (opmName == QStringLiteral("RoboDozer"))    return QStringLiteral("Robo-Dozer");
    if (opmName == QStringLiteral("RoboMiner"))    return QStringLiteral("Robo-Miner");
    if (opmName == QStringLiteral("RoboSurveyor")) return QStringLiteral("Robo-Surveyor");
    return opmName;
}

// .opm CargoTruck CargoType → our loaded-truck catalog body name ("Cargo <X>"), or "" for an
// empty bed. Mirrors OP2MissionEditor's UnitRenderer cargo→sprite mapping exactly (TruckCargo
// 1–7 named; 8 Spaceport→Starship; 9 Garbage→Wreckage; anything else, incl. a numeric module
// id like "90", → GeneBank). The catalog has all 10 loaded sets.
QString cargoTruckBodyName(const QString& cargo) {
    if (cargo.isEmpty() || cargo == QStringLiteral("None") || cargo == QStringLiteral("Empty"))
        return {};                                                    // empty bed
    if (cargo == QStringLiteral("Food"))         return QStringLiteral("Cargo Food");
    if (cargo == QStringLiteral("CommonOre"))    return QStringLiteral("Cargo Common Ore");
    if (cargo == QStringLiteral("RareOre"))      return QStringLiteral("Cargo Rare Ore");
    if (cargo == QStringLiteral("CommonMetal"))  return QStringLiteral("Cargo Common Metals");
    if (cargo == QStringLiteral("RareMetal"))    return QStringLiteral("Cargo Rare Metals");
    if (cargo == QStringLiteral("CommonRubble")) return QStringLiteral("Cargo Common Rubble");
    if (cargo == QStringLiteral("RareRubble"))   return QStringLiteral("Cargo Rare Rubble");
    // The .opm stores enum names (the dropdown's "Starship"/"Wreckage"/"Gene Bank" are
    // UI-only and never written). cargoType 8 = "Spaceport", 9 = "Garbage"; cargoType 10
    // (Gene Bank) and any out-of-range numeric (e.g. "90") fall to the editor's else → GeneBank.
    if (cargo == QStringLiteral("Spaceport")) return QStringLiteral("Cargo Starship Components");
    if (cargo == QStringLiteral("Garbage"))   return QStringLiteral("Cargo Wreckage");
    return QStringLiteral("Cargo Gene Bank"); // GeneBank / numeric module id / editor else
}

// Our building-catalog name → building.txt Code_Name (the sheet key for stats lookup).
// Correlated 1:1 against OPU's building.txt by footprint (each footprint class matched
// exactly), e.g. CommandCenter→COMMAND(2500), GuardPost→TOWER_GUARD(500), MHDGenerator→ASE.
const QHash<QString, QString> kBuildingCodeName = {
    {QStringLiteral("CommonOreMine"),"MINE"},        {QStringLiteral("RareOreMine"),"MINE_ADV"},
    {QStringLiteral("GuardPost"),"TOWER_GUARD"},     {QStringLiteral("LightTower"),"TOWER_LIGHT"},
    {QStringLiteral("CommonStorage"),"STORAGE_ORE"}, {QStringLiteral("RareStorage"),"STORAGE_RARE"},
    {QStringLiteral("Forum"),"FORUM"},               {QStringLiteral("CommandCenter"),"COMMAND"},
    {QStringLiteral("MHDGenerator"),"ASE"},          {QStringLiteral("Residence"),"RESIDENCE"},
    {QStringLiteral("RobotCommand"),"ROBOT_COMMAND"},{QStringLiteral("TradeCenter"),"TRADE"},
    {QStringLiteral("BasicLab"),"LAB"},              {QStringLiteral("MedicalCenter"),"MED_CENTER"},
    {QStringLiteral("Nursery"),"NURSERY"},           {QStringLiteral("SolarPowerArray"),"SOLAR"},
    {QStringLiteral("RecFacility"),"RECREATION"},    {QStringLiteral("University"),"UNIVERSITY"},
    {QStringLiteral("Agridome"),"AGRIDOME"},         {QStringLiteral("DIRT"),"DIRT"},
    {QStringLiteral("Garage"),"GARAGE"},             {QStringLiteral("MagmaWell"),"MAGMA_WELL"},
    {QStringLiteral("MeteorDefense"),"METEOR_DEF"},  {QStringLiteral("GeothermalPlant"),"GEOTHERMAL"},
    {QStringLiteral("ArachnidFactory"),"FACT_ANDROID"}, {QStringLiteral("ConsumerFactory"),"FACT_LUXURY"},
    {QStringLiteral("StructureFactory"),"FACT_STRUCTURE"}, {QStringLiteral("VehicleFactory"),"FACT_VEHICLE"},
    {QStringLiteral("StandardLab"),"LAB_STANDARD"},  {QStringLiteral("AdvancedLab"),"LAB_ADV"},
    {QStringLiteral("Observatory"),"OBSERVATORY"},   {QStringLiteral("ReinforcedResidence"),"RESIDENCE_P"},
    {QStringLiteral("AdvancedResidence"),"RESIDENCE_E"}, {QStringLiteral("CommonOreSmelter"),"SMELTER"},
    {QStringLiteral("Spaceport"),"SPACEPORT"},       {QStringLiteral("RareOreSmelter"),"SMELTER_ADV"},
    {QStringLiteral("GORF"),"GORF"},                 {QStringLiteral("Tokamak"),"TOKAMAK"},
};

// Factory building types shown in the Factories Report, with their OP2 display names. These are
// the structures with a production queue (the report lists what each is producing).
const QHash<QString, QString> kFactoryDisplay = {
    {QStringLiteral("StructureFactory"), QStringLiteral("Structure Factory")},
    {QStringLiteral("VehicleFactory"),   QStringLiteral("Vehicle Factory")},
    {QStringLiteral("ArachnidFactory"),  QStringLiteral("Arachnid Factory")},
    {QStringLiteral("ConsumerFactory"),  QStringLiteral("Consumer Factory")},
    {QStringLiteral("Spaceport"),        QStringLiteral("Spaceport")},
};

// Lab building types shown in the Labs Report (each lists what it's researching).
const QHash<QString, QString> kLabDisplay = {
    {QStringLiteral("BasicLab"),    QStringLiteral("Basic Lab")},
    {QStringLiteral("StandardLab"), QStringLiteral("Standard Lab")},
    {QStringLiteral("AdvancedLab"), QStringLiteral("Advanced Lab")},
};

// Our unit name (the .opm map_id) → vehicles.txt Code_Name. (InterColonyShuttle is an
// unused easter-egg unit — it never appears in the shipped game and isn't in vehicles.txt,
// so it correctly has no entry / 0 HP. Not a missing mapping.)
const QHash<QString, QString> kVehicleCodeName = {
    {QStringLiteral("Lynx"),"LIGHT_TANK"},   {QStringLiteral("Panther"),"MED_TANK"},
    {QStringLiteral("Tiger"),"BIG_TANK"},    {QStringLiteral("ConVec"),"CON_TRUCK"},
    {QStringLiteral("CargoTruck"),"BIG_TRUCK"}, {QStringLiteral("RoboMiner"),"MINER"},
    {QStringLiteral("RoboSurveyor"),"SURVEYOR"}, {QStringLiteral("Scout"),"SCOUT"},
    {QStringLiteral("Earthworker"),"WALL_TRUCK"}, {QStringLiteral("RoboDozer"),"BULL_DOZER"},
    {QStringLiteral("RepairVehicle"),"REPAIR_TRUCK"}, {QStringLiteral("EvacuationTransport"),"MOBILE_HOME"},
    {QStringLiteral("GeoCon"),"GEO_METRO"},  {QStringLiteral("Spider"),"SPIDER"},
    {QStringLiteral("Scorpion"),"SCORPION"},
};
} // namespace

void TilesetView::populateType(quint16 typeId, const QString& name) {
    op2::sim::UnitType t;
    // Building? (in the building catalog, incl. GuardPost) → footprint + stats from building.txt.
    const BuildingDef* bd = nullptr;
    for (const BuildingDef& d : catalog_.buildings())
        if (d.name == name) { bd = &d; break; }
    if (bd) {
        t.kind = op2::sim::UnitKind::Building;
        t.fpX = static_cast<std::uint8_t>(bd->fpX);
        t.fpY = static_cast<std::uint8_t>(bd->fpY);
        const auto it = kBuildingCodeName.constFind(name);
        if (it != kBuildingCodeName.constEnd()) {
            const int row = buildingSheet_.findRow("Code_Name", it.value().toStdString());
            if (row >= 0) {
                t.maxHitPoints       = buildingSheet_.cellInt(row, "Hit Points", 0);
                t.armor              = buildingSheet_.cellInt(row, "Armor Class", 0);
                t.powerRequired      = buildingSheet_.cellInt(row, "Power required", 0);
                t.workersRequired    = buildingSheet_.cellInt(row, "Workers required", 0);
                t.scientistsRequired = buildingSheet_.cellInt(row, "Scientists required", 0);
                // The dock tile (bottom row, this column) stays drivable (World::holdsTile).
                t.dockEden     = static_cast<std::uint8_t>(std::clamp(buildingSheet_.cellInt(row, "Docking Loc E", 0), 0, 8));
                t.dockPlymouth = static_cast<std::uint8_t>(std::clamp(buildingSheet_.cellInt(row, "Docking Loc P", 0), 0, 8));
            }
        }
    } else {
        t.kind = op2::sim::UnitKind::Vehicle; // 1×1
        const auto it = kVehicleCodeName.constFind(name);
        if (it != kVehicleCodeName.constEnd()) {
            const int row = vehicleSheet_.findRow("Code_Name", it.value().toStdString());
            if (row >= 0) {
                t.maxHitPoints = vehicleSheet_.cellInt(row, "Hit Points", 0);
                t.armor        = vehicleSheet_.cellInt(row, "Armor Class", 0);
                // Movement (movement.h): track type, Move Points (lower is faster), Turn Rate.
                const std::string& track = vehicleSheet_.cell(row, "Track Type W/T/L/M");
                t.track     = op2::sim::trackFromLetter(track.empty() ? 'W' : track[0]);
                t.moveSpeed = vehicleSheet_.cellInt(row, "Move Points", 0);
                t.turnRate  = vehicleSheet_.cellInt(row, "Turn Rate", 0);
                t.sight     = vehicleSheet_.cellInt(row, "Sight range", 0);
            }
        }
    }
    world_.setType(typeId, t);
}

quint16 TilesetView::typeIdFor(const QString& name) {
    auto it = typeIdByName_.constFind(name);
    if (it != typeIdByName_.constEnd()) return it.value();
    const quint16 id = static_cast<quint16>(nameByTypeId_.size());
    typeIdByName_.insert(name, id);
    nameByTypeId_.append(name);
    return id;
}

const UnitDef* TilesetView::findUnitDef(const QString& name, const QString& faction) const {
    for (const UnitDef& u : unitCatalog_.units())
        if (u.kind != QStringLiteral("turret") && u.name == name &&
            (u.faction == faction || u.faction == QStringLiteral("both")))
            return &u;
    return nullptr;
}

const UnitDef* TilesetView::findTurretDef(const QString& name, const QString& faction) const {
    const UnitDef* shared = nullptr;
    for (const UnitDef& u : unitCatalog_.units()) {
        if (u.kind != QStringLiteral("turret") || u.name != name) continue;
        if (u.faction == faction) return &u;                  // exact faction art
        if (u.faction == QStringLiteral("both")) shared = &u;
    }
    return shared;
}

const UnitDef* TilesetView::resolveTurretDef(const QString& chassis, const QString& weapon,
                                             const QString& faction) const {
    if (weapon.isEmpty() || weapon == QStringLiteral("None") || weapon == QStringLiteral("Empty"))
        return nullptr;
    // Only the wheeled combat chassis mount a SEPARATE weapon turret. Spider/Scorpion are
    // self-contained robots (weapon baked into the body sprite) — drawing a turret on them
    // covers the small body and looks like "no chassis". ConVec/etc. likewise have no turret.
    if (chassis != QStringLiteral("Lynx") && chassis != QStringLiteral("Panther") &&
        chassis != QStringLiteral("Tiger"))
        return nullptr;
    struct W { const char* name; const char* single; const char* dual; };
    static const W kW[] = {
        {"Laser","Laser_Lynx","Laser_Tiger"},
        {"RailGun","RailGun_LynxTiger","RailGun_Tiger"},
        {"ESG","ESG_LynxTiger","ESG_Tiger"},
        {"ThorsHammer","ThorsHammer_LynxTiger","ThorsHammer_Tiger"},
        {"Stickyfoam","Stickyfoam_LynxTiger","Stickyfoam_Tiger"},
        {"AcidCloud","AcidCloud_LynxTiger","AcidCloud_Tiger"},
        {"EMP","EMP_Panther","EMP_Tiger"},
        {"RPG","RPG_Panther","RPG_Tiger"},
        {"Microwave","Microwave_Panther","Microwave_Tiger"},
        {"Supernova","Supernova_Panther",nullptr},
        {"Starflare","Starflare",nullptr},
    };
    for (const W& w : kW) {
        if (weapon != QString::fromLatin1(w.name)) continue;
        const bool tiger = (chassis == QStringLiteral("Tiger"));
        const char* tn = (tiger && w.dual) ? w.dual : w.single;
        return findTurretDef(QString::fromLatin1(tn), faction);
    }
    return nullptr;
}

bool TilesetView::loadMission(const QString& opmPath) {
    MissionData md;
    QString err;
    if (!loadOpmMission(opmPath, &md, &err)) {
        Logger::log(QStringLiteral("mission load failed: ") + err);
        return false;
    }
    world_.clear();
    typeIdByName_.clear();
    nameByTypeId_.clear();
    localPlayer_ = md.localPlayer;
    friendlyPlayers_ = md.friendlyPlayers;
    for (const MissionObject& mo : md.objects) {
        op2::sim::Unit u;
        u.typeId   = typeIdFor(mo.typeId);
        u.owner    = static_cast<std::uint8_t>(mo.owner & 0xFF);
        // Ensure the UnitType (kind, footprint, HP, armor) is built for this type, then read
        // from it — the per-type table is the single source of truth (no per-spawn re-lookup).
        populateType(u.typeId, mo.typeId);
        const op2::sim::UnitType& ut = world_.type(u.typeId);
        u.kind      = ut.kind;     // damage 0: spawn at full health
        u.rotation = static_cast<std::uint8_t>((mo.direction & 7) * 32); // 8-dir → 16-facing byte
        // .opm Position is 1-based "game coordinates" — the editor subtracts (1,1) to get the
        // 0-based map cell (UnitView.GetMapCoordinates). (The editor also inverts Y for Unity's
        // bottom-up tilemap; we're top-down, so no invert.) For a structure that cell is its
        // footprint's CENTRE tile; the unit's pixel is the footprint centre (coords.h), which
        // for a 1×1 vehicle is the tile centre.
        u.px = op2::sim::structurePx(mo.tileX - 1, ut.fpX);
        u.py = op2::sim::structurePx(mo.tileY - 1, ut.fpY);
        u.cargoAmount = mo.cargoAmount;
        if (mo.isEden) u.flags |= op2::sim::UnitFlag::IsEden;
        // We don't model weapon-as-field yet: stash the weapon/cargo name id (prefixed)
        // so the renderer can recover it. (cargo is otherwise a TruckCargo enum.)
        if (!mo.cargoType.isEmpty() && mo.cargoType != QStringLiteral("None"))
            u.cargo = typeIdFor(QStringLiteral("w:") + mo.cargoType);
        if (!world_.spawn(u).valid())  // OP2's limit: 0x3FF objects
            Logger::log(QStringLiteral("  skip %1: no free object index").arg(mo.typeId));
    }
    // Map resources/markers/wreckage aren't player units — keep them as their own
    // list (placeResources turns them into sprites; rebuilt on toggle).
    missionResources_ = md.resources;
    // A new game from tick 0. The local player is the only human (the others are the AI).
    // Fixed seed for now: the game's own seed source isn't modelled.
    world_.setHumanPlayers(static_cast<std::uint8_t>(1u << (localPlayer_ & 7)));
    game_.restart(1);
    Logger::log(QStringLiteral("mission '%1': %2 objects → World, %3 resources")
                    .arg(md.description).arg(world_.liveCount())
                    .arg(missionResources_.size()));
    return true;
}

// GuardPost weapon sprites — the per-direction .prt groups, extracted from the EXE's
// rotation tables (the weapon's "Init/default" table IS its GuardPost table). IMPORTANT:
// each group is the COMPLETE GuardPost for that direction — the disc base + weapon baked
// into ONE sprite (confirmed: group 966 = op2_art frame 14125, group 1084 = frame 14722).
// So GuardPosts are drawn as a single rotating sprite, NOT base + separate turret.
// facing 0..15 → groups[facing]. (Read from Outpost2.exe .data: fileoff = VA − 0x401800.)
namespace {
struct GpTurretArt { const char* weapon; int groups[16]; bool rotates; };
const GpTurretArt kGpTurrets[] = {
    {"Laser",       {966,981,980,979,978,977,976,975,974,973,972,971,970,969,968,967}, true},
    {"RailGun",     {982,997,996,995,994,993,992,991,990,989,988,987,986,985,984,983}, true},
    {"AcidCloud",   {1068,1083,1082,1081,1080,1079,1078,1077,1076,1075,1074,1073,1072,1071,1070,1069}, true},
    // Thor's Hammer is the odd one: its table block is 947–962 in an irregular order.
    {"ThorsHammer", {951,950,949,948,947,962,961,960,959,958,957,956,955,954,953,952}, true},
    {"ESG",         {1254,1269,1268,1267,1266,1265,1264,1263,1262,1261,1260,1259,1258,1257,1256,1255}, true},
    {"RPG",         {624,639,638,637,636,635,634,633,632,631,630,629,628,627,626,625}, true},
    {"Microwave",   {1011,1026,1025,1024,1023,1022,1021,1020,1019,1018,1017,1016,1015,1014,1013,1012}, true},
    {"Stickyfoam",  {1031,1046,1045,1044,1043,1042,1041,1040,1039,1038,1037,1036,1035,1034,1033,1032}, true},
    // (Supernova/Starflare bombs are handled separately — single static combined sprite.)
};
// EMP GuardPost is faction-split (distinct art, not just palette).
const int kGpEmpEden[16] = {1084,1099,1098,1097,1096,1095,1094,1093,1092,1091,1090,1089,1088,1087,1086,1085};
const int kGpEmpPlym[16] = {2043,2058,2057,2056,2055,2054,2053,2052,2051,2050,2049,2048,2047,2046,2045,2044};
// Bombs are a SINGLE static COMBINED sprite (disc + bomb in one group) — NOT the tiny
// vehicle bomb groups. Starflare is faction-split (Eden g1694 / Plymouth g938); Supernova
// shares one (g939). (Confirmed against the EXE bomb-GP selectors + the editor art.)
const int kGpStarflareEden = 1694, kGpStarflarePlym = 938;
const int kGpSupernova = 939;
} // namespace

const UnitDef* TilesetView::resolveGuardPostTurret(const QString& weapon,
                                                   const QString& faction, bool* rotates) {
    const bool eden = (faction == QStringLiteral("eden"));
    UnitDef def;
    def.kind = QStringLiteral("turret");
    def.name = weapon;
    def.faction = faction;
    bool rot = false, found = false;
    if (weapon == QStringLiteral("EMP")) {
        const int* a = eden ? kGpEmpEden : kGpEmpPlym;
        for (int r = 0; r < 16; ++r) def.groups[r] = a[r];
        rot = true; found = true;
    } else if (weapon == QStringLiteral("Starflare")) {
        const int g = eden ? kGpStarflareEden : kGpStarflarePlym;
        for (int r = 0; r < 16; ++r) def.groups[r] = g; // bomb: single static combined sprite
        rot = false; found = true;
    } else if (weapon == QStringLiteral("Supernova")) {
        for (int r = 0; r < 16; ++r) def.groups[r] = kGpSupernova; // bomb: shared, static
        rot = false; found = true;
    } else {
        for (const GpTurretArt& t : kGpTurrets) {
            if (weapon == QLatin1String(t.weapon)) {
                for (int r = 0; r < 16; ++r) def.groups[r] = t.groups[r];
                rot = t.rotates; found = true; break;
            }
        }
    }
    if (rotates) *rotates = rot;
    if (!found) return nullptr;
    const QString key = weapon + QLatin1Char('|') + faction;
    auto it = gpTurretCache_.find(key);
    if (it == gpTurretCache_.end())
        it = gpTurretCache_.emplace(key, def).first;
    return &it->second;
}

void TilesetView::placeGuardPost(const QString& faction, const QString& weapon,
                                 int cellX, int cellY, int facing, const QString& label,
                                 bool idleRotate) {
    // A GuardPost is ONE combined sprite per direction (disc base + weapon baked together
    // — the disc is part of the weapon sprite, there is no separate base group). We draw it
    // in the turret layer so gun GPs can idle-wander via advanceUnitTurrets; bombs are a
    // single static sprite. (The construction group is only the build animation, unused here.)
    bool rotates = false;
    const UnitDef* def = resolveGuardPostTurret(weapon, faction, &rotates);
    if (!def) {
        // A GuardPost always has a weapon in OP2; if the .opm left it unset (or names an
        // unknown weapon), fall back to the faction's basic gun so it still renders.
        const QString fallback = (faction == QStringLiteral("eden"))
                                     ? QStringLiteral("Laser") : QStringLiteral("ESG");
        def = resolveGuardPostTurret(fallback, faction, &rotates);
        if (!def) return;
    }
    const int g = def->groups[facing];
    PlacedUnit pu;
    if (!blitUnitSprite(g, &pu.turret, &pu.tAnchorX, &pu.tAnchorY)) return;
    // 1×1 footprint: unit pixel = the cell centre, seated via the sprite's group displacement.
    const int upX = cellX * T + T / 2;
    const int upY = cellY * T + T / 2;
    int tdx = 0, tdy = 0;
    art_.groupDisplacement(g, &tdx, &tdy);
    pu.tOriginX = upX - tdx; pu.tOriginY = upY - tdy;
    pu.isGuardPost = true;
    pu.centerX = upX; pu.centerY = upY; // footprint centre — where the off-light sits
    // Gun GPs idle-wander (rotate the whole combined sprite); bombs hold one facing.
    if (idleRotate && rotates) {
        pu.turretDef = def; pu.turretRot = facing; pu.bodyRot = facing;
        pu.seatUpX = upX; pu.seatUpY = upY;
        pu.chassisGroup = g; pu.chassisFrame = -1; // combined sprite has no mount offset
    }
    if (!label.isEmpty()) {
        pu.label = label;
        pu.labelX = cellX * T;
        pu.labelY = (cellY + 1) * T + 8;
    }
    placedUnits_.push_back(std::move(pu));
}

void TilesetView::placeFromWorld() {
    placed_.clear();
    placedUnits_.clear();
    placeResources(); // ore beacons / vents / fumaroles / markers / wreckage
    world_.forEach([&](op2::sim::UnitId id, const op2::sim::Unit& u) {
        if (u.typeId >= nameByTypeId_.size()) return;
        const QString rawName = nameByTypeId_[u.typeId];
        const QString faction = (u.flags & op2::sim::UnitFlag::IsEden)
                                    ? QStringLiteral("eden") : QStringLiteral("plymouth");
        const int facing = (u.rotation >> 4) & 0xF;

        // Decode the stashed cargo/weapon name ("w:<name>").
        QString cargoOrWeapon;
        if (u.cargo && u.cargo < nameByTypeId_.size()) {
            const QString c = nameByTypeId_[u.cargo];
            if (c.startsWith(QStringLiteral("w:"))) cargoOrWeapon = c.mid(2);
        }
        // A loaded cargo truck uses the pre-composited truck+cargo sprite as its body;
        // combat units keep their chassis and mount a weapon turret.
        QString bodyName = normalizeUnitName(rawName);
        QString weaponName;
        if (rawName == QStringLiteral("CargoTruck") && !cargoOrWeapon.isEmpty()) {
            const QString loaded = cargoTruckBodyName(cargoOrWeapon);
            if (!loaded.isEmpty()) bodyName = loaded;
        } else {
            weaponName = cargoOrWeapon;
        }

        // --- GuardPost? (weapon turret drawn like a fixed Lynx; a weapon-bearing building) ---
        if (rawName == QStringLiteral("GuardPost")) {
            const bool gpIdle     = (u.flags & op2::sim::UnitFlag::Idle);
            const bool gpDisabled = (u.flags & (op2::sim::UnitFlag::Disabled | op2::sim::UnitFlag::IsEMPed));
            const bool gpActive   = !gpIdle && !gpDisabled;
            placeGuardPost(faction, weaponName, op2::sim::pixelToTile(u.px), op2::sim::pixelToTile(u.py), facing,
                           QString(), /*idleRotate=*/gpActive); // off GPs don't wander
            if (!placedUnits_.empty()) {
                PlacedUnit& gpu = placedUnits_.back();
                gpu.active = gpActive; gpu.idle = gpIdle; gpu.disabled = gpDisabled;
                gpu.friendly = isFriendlyOwner(u.owner);
                gpu.weapon = weaponName;
                gpu.sourceId = id;
            }
            return;
        }

        // --- vehicle? (unit catalog) ---
        if (const UnitDef* body = findUnitDef(bodyName, faction)) {
            const int chassisGroup = body->groups[facing];
            PlacedUnit pu;
            if (!blitUnitSprite(chassisGroup, &pu.chassis, &pu.cAnchorX, &pu.cAnchorY)) {
                Logger::log(QStringLiteral("  skip vehicle %1 [%2] facing %3: chassis group %4 failed")
                                .arg(rawName, faction).arg(facing).arg(chassisGroup));
                return;
            }
            const int upX = u.px;
            const int upY = u.py;
            int cdx = 0, cdy = 0;
            art_.groupDisplacement(chassisGroup, &cdx, &cdy);
            pu.originX = upX - cdx;  pu.originY = upY - cdy;
            pu.tOriginX = pu.originX; pu.tOriginY = pu.originY;
            const UnitDef* turret = weaponName.isEmpty() ? nullptr
                : resolveTurretDef(normalizeUnitName(rawName), weaponName, faction);
            if (turret) {
                blitUnitSprite(turret->groups[facing], &pu.turret, &pu.tAnchorX, &pu.tAnchorY);
                int ox = 0, oy = 0, tdx = 0, tdy = 0;
                art_.frameTurretOffset(art_.groupFrameStart(chassisGroup), &ox, &oy);
                art_.groupDisplacement(turret->groups[facing], &tdx, &tdy);
                pu.tOriginX = upX + ox - tdx;  pu.tOriginY = upY + oy - tdy;
                // Supernova/Starflare are BOMB weapons: no rotating turret. On a Tiger
                // the single sprite is stacked (drawn twice, raised) like the in-game
                // twin bomb; otherwise it's a static single. Gun weapons idle-rotate.
                const bool isBomb = (weaponName == QStringLiteral("Supernova")
                                  || weaponName == QStringLiteral("Starflare"));
                const bool isTiger = (normalizeUnitName(rawName) == QStringLiteral("Tiger"));
                if (isBomb) {
                    if (isTiger) {
                        int fs = art_.groupFrameStart(turret->groups[0]), bax = 0, bay = 0;
                        QImage tmp = art_.frameToQImage(fs, &bax, &bay, false);
                        pu.turretStackDy = tmp.isNull() ? 8 : qMax(4, tmp.height() / 3);
                    }
                } else {
                    // Idle turret rotation (AnimationAmbientUnit): keep the re-seat inputs.
                    pu.turretDef = turret;
                    pu.turretRot = facing;
                    pu.bodyRot = facing;
                    pu.seatUpX = upX;
                    pu.seatUpY = upY;
                    pu.chassisGroup = chassisGroup;
                }
            }
            // syncVehicles' re-seat inputs (the sim moves and turns it).
            pu.bodyDef = body;
            pu.turretArt = turret;
            pu.bodyRot = facing;
            pu.turretRot = facing;
            pu.seatUpX = upX;
            pu.seatUpY = upY;
            pu.chassisGroup = chassisGroup;
            pu.weapon = weaponName; // for the detail panel (empty for cargo trucks/unarmed)
            pu.sourceId = id; // link back to the World unit (for hover HP, status, etc.)
            placedUnits_.push_back(std::move(pu));
            return;
        }

        // --- building? (building catalog) ---
        const BuildingDef* bd = nullptr;
        for (const BuildingDef& d : catalog_.buildings())
            if (d.name == rawName) { bd = &d; break; }
        if (!bd) {
            Logger::log(QStringLiteral("  skip %1 [%2]: not a known vehicle or building")
                            .arg(rawName, faction));
            return;
        }
        // The unit's pixel is the footprint centre (set from the .opm Position, its centre
        // tile, at load: OP2MissionEditor's StructureView.GetStructureArea takes
        // xMin = pos - size/2). The top-left tile follows from the pixel.
        const int tileX = op2::sim::structureTopLeft(u.px, bd->fpX);
        const int tileY = op2::sim::structureTopLeft(u.py, bd->fpY);
        const bool eden = (faction == QStringLiteral("eden"));
        const std::size_t before = placed_.size();
        // Per-faction art, special cases included (resolveBuildingArt — the SAME table
        // the showcase uses, so mission buildings animate identically to the parade).
        BuildingArt ba = resolveBuildingArt(*bd, eden);
        if (ba.normal < 0) { // no Normal for this faction: static construction sprite
            ba.normal = bd->displayGroup();
            ba.amb1 = ba.amb2 = -1;
        }
        placeBuildingInstance(*bd, ba.normal, rawName, tileX, tileY, ba.floor,
                              ba.lastFrameOnly, ba.amb1, ba.amb2);
        // Stamp the building's status from the World object: Idle → no animation, and keep
        // a link back so a click can persist the toggle. (Skip if nothing was placed.)
        if (placed_.size() > before) {
            PlacedBuilding& pb = placed_.back();
            pb.sourceId = id;
            // Two independent flags: player-Idle (gray) and condition-Disabled (red).
            // EMP also disables a STRUCTURE (OP2 "Disabled-EMP"), so it folds into disabled.
            // Operating only when neither; light: idle > disabled > active.
            pb.idle = (u.flags & op2::sim::UnitFlag::Idle);
            pb.disabled = (u.flags & (op2::sim::UnitFlag::Disabled | op2::sim::UnitFlag::IsEMPed));
            pb.active = !pb.idle && !pb.disabled;
            pb.friendly = isFriendlyOwner(u.owner); // overlay light only on own/allied buildings
            if (!pb.active) { pb.curAnim = 0; pb.frame = 0; pb.frameAccum = 0.0; pb.playIdx = 0; pb.gapRemaining = 0; }
        }
    });
    Logger::log(QStringLiteral("mission render: %1 buildings + %2 units + %3 sprites placed")
                    .arg(placed_.size()).arg(placedUnits_.size()).arg(placedSprites_.size()));
    // The placed lists were rebuilt: refresh the selection panel from the (stable) handle
    // so a re-render (anim toggle, survey, etc.) keeps the selection in sync.
    if (selectedId_.valid()) { rebuildSelectedInfo(); emit selectionChanged(); }
    emit reportsChanged(); // command-report panels (Factories, …) re-read from the World
}

// Shared command-report builder: the LOCAL player's buildings whose name is in displayMap, OP2-style.
// Each entry is "<Display Name> # <player>-<NN>" (NN = per-type instance counter) plus a status line
// "<verb> Nothing" (production/research is sim-blocked for now). Each carries the World handle so a
// double-click can focus the object. (Empty until such a building owned by the local player loads.)
QVariantList TilesetView::buildingReport(const QHash<QString, QString>& displayMap,
                                         const QString& verb) const {
    QVariantList list;
    QHash<QString, int> perType; // per-type instance counter for the "# n-NN" display
    world_.forEach([&](op2::sim::UnitId id, const op2::sim::Unit& u) {
        if (u.owner != localPlayer_) return;                 // your own buildings only
        if (u.typeId >= nameByTypeId_.size()) return;
        const QString name = nameByTypeId_[u.typeId];
        const auto it = displayMap.constFind(name);
        if (it == displayMap.constEnd()) return;             // not in this report
        const int inst = ++perType[name];
        QVariantMap m;
        m[QStringLiteral("title")] = it.value() +
            QStringLiteral(" # %1-%2").arg(u.owner + 1).arg(inst, 2, 10, QLatin1Char('0'));
        m[QStringLiteral("status")] = verb + QStringLiteral(" Nothing"); // TODO: production/research sim
        m[QStringLiteral("idx")] = int(id.index);            // World handle → double-click to focus
        m[QStringLiteral("gen")] = int(id.generation);
        list.append(m);
    });
    return list;
}

QVariantList TilesetView::factoriesReport() const {
    return buildingReport(kFactoryDisplay, QStringLiteral("Producing"));
}

QVariantList TilesetView::labsReport() const {
    return buildingReport(kLabDisplay, QStringLiteral("Researching"));
}

// Colony economy summary (Resources Report). Only the mine/smelter COUNTS are live (scanned from the
// local player's buildings); the stored amounts, colonists, morale, power and food are 0/placeholder
// until the economy sim exists. Keeps OP2's field set so the panel layout matches now.
QVariantMap TilesetView::resourcesReport() const {
    int commonMines = 0, commonSmelters = 0, rareMines = 0, rareSmelters = 0;
    world_.forEach([&](op2::sim::UnitId, const op2::sim::Unit& u) {
        if (u.owner != localPlayer_) return;
        if (u.typeId >= nameByTypeId_.size()) return;
        const QString name = nameByTypeId_[u.typeId];
        if      (name == QStringLiteral("CommonOreMine"))    ++commonMines;
        else if (name == QStringLiteral("CommonOreSmelter")) ++commonSmelters;
        else if (name == QStringLiteral("RareOreMine"))      ++rareMines;
        else if (name == QStringLiteral("RareOreSmelter"))   ++rareSmelters;
    });
    QVariantMap m;
    // Live (derived from the World):
    m[QStringLiteral("commonMines")] = commonMines;
    m[QStringLiteral("commonSmelters")] = commonSmelters;
    m[QStringLiteral("rareMines")] = rareMines;
    m[QStringLiteral("rareSmelters")] = rareSmelters;
    // Sim-blocked (0/placeholder until the economy sim):
    for (const char* k : {"colonists", "workers", "scientists", "morale", "powerUsed", "powerMax",
                          "unpowered", "foodStored", "foodProduction", "commonMetals", "rareMetals"})
        m[QString::fromLatin1(k)] = 0;
    m[QStringLiteral("trend")] = QStringLiteral("Steady");
    return m;
}

// "Colonists Status" sub-report (Alt+C) — population / worker / scientist breakdown. All fields are
// sim-blocked (no population sim) → 0 for now; keeps OP2's field set so the layout matches.
QVariantMap TilesetView::colonistsReport() const {
    QVariantMap m;
    for (const char* k : {"totalPopulation", "residenceCapacity", "children",
                          "workers", "workersAssigned", "workersAssignedMax", "workersAvailable",
                          "scientists", "sciAsScientists", "sciAsScientistsMax",
                          "sciAsWorkers", "sciToResearch", "sciAvailable"})
        m[QString::fromLatin1(k)] = 0;
    return m;
}

// Morale structure types (the STRUCTURES drill-down of Morale Status) → display names. The buildings
// that drive morale via capacity/demand: residences, recreation, consumer goods, medical.
const QHash<QString, QString> kMoraleStructDisplay = {
    {QStringLiteral("Residence"),           QStringLiteral("Residence")},
    {QStringLiteral("ReinforcedResidence"), QStringLiteral("Reinforced Residence")},
    {QStringLiteral("AdvancedResidence"),   QStringLiteral("Advanced Residence")},
    {QStringLiteral("RecFacility"),         QStringLiteral("Recreation Facility")},
    {QStringLiteral("ConsumerFactory"),     QStringLiteral("Consumer Factory")},
    {QStringLiteral("MedicalCenter"),       QStringLiteral("Medical Center")},
};

// Like buildingReport but status = the building's live operating state (Active/Idle/Disabled).
QVariantList TilesetView::structureList(const QHash<QString, QString>& displayMap) const {
    QVariantList list;
    QHash<QString, int> perType;
    world_.forEach([&](op2::sim::UnitId id, const op2::sim::Unit& u) {
        if (u.owner != localPlayer_) return;
        if (u.typeId >= nameByTypeId_.size()) return;
        const QString name = nameByTypeId_[u.typeId];
        const auto it = displayMap.constFind(name);
        if (it == displayMap.constEnd()) return;
        const bool idle = (u.flags & op2::sim::UnitFlag::Idle);
        const bool disabled = (u.flags & (op2::sim::UnitFlag::Disabled | op2::sim::UnitFlag::IsEMPed));
        const int inst = ++perType[name];
        QVariantMap m;
        m[QStringLiteral("title")] = it.value() +
            QStringLiteral(" # %1-%2").arg(u.owner + 1).arg(inst, 2, 10, QLatin1Char('0'));
        m[QStringLiteral("status")] = idle ? QStringLiteral("Idle")
                                  : (disabled ? QStringLiteral("Disabled") : QStringLiteral("Active"));
        m[QStringLiteral("warn")] = idle || disabled;  // not-operating → coloured differently
        m[QStringLiteral("idx")] = int(id.index);
        m[QStringLiteral("gen")] = int(id.generation);
        list.append(m);
    });
    return list;
}

bool TilesetView::hasActive(const QString& name) const {
    bool found = false;
    world_.forEach([&](op2::sim::UnitId, const op2::sim::Unit& u) {
        if (found || u.owner != localPlayer_) return;
        if (u.typeId >= nameByTypeId_.size() || nameByTypeId_[u.typeId] != name) return;
        const bool off = (u.flags & (op2::sim::UnitFlag::Idle | op2::sim::UnitFlag::Disabled
                                     | op2::sim::UnitFlag::IsEMPed));
        if (!off) found = true;
    });
    return found;
}

QVariantList TilesetView::moraleStructures() const {
    return structureList(kMoraleStructDisplay);
}

// Colonist structure types (the STRUCTURES drill-down of Colonists Status) → display names: the
// buildings that train/sustain colonists.
const QHash<QString, QString> kColonistStructDisplay = {
    {QStringLiteral("University"),    QStringLiteral("University")},
    {QStringLiteral("Nursery"),       QStringLiteral("Nursery")},
    {QStringLiteral("MedicalCenter"), QStringLiteral("Medical Center")},
};

QVariantList TilesetView::colonistStructures() const {
    return structureList(kColonistStructDisplay);
}

// "Morale Status" sub-report (Alt+M). Mostly sim-blocked; the Nursery/University/GORF "Operational"
// flags are DERIVED (player owns an active one). Level/Trend/demands/food/etc. need the colony sim.
QVariantMap TilesetView::moraleReport() const {
    QVariantMap m;
    m[QStringLiteral("level")] = QStringLiteral("Fair");   // morale band (sim → for now a default)
    m[QStringLiteral("trend")] = QStringLiteral("Steady");
    m[QStringLiteral("morale")] = 0;                       // 0..100 (sim)
    for (const char* k : {"residenceDemand", "netFood", "disabledRatio", "unoccupied",
                          "sciAsWorkers", "recDemand", "medDemand", "disasterWarn", "dirtPrevent"})
        m[QString::fromLatin1(k)] = 0;
    // Derived: does the player have an active one of these?
    m[QStringLiteral("nurseryOp")]    = hasActive(QStringLiteral("Nursery"));
    m[QStringLiteral("universityOp")] = hasActive(QStringLiteral("University"));
    m[QStringLiteral("gorfOp")]       = hasActive(QStringLiteral("GORF"));
    return m;
}

// Communications event log — newest-first { mark, text, warn } messages. Empty until there's an
// event/message system (births/deaths/training/structure-disabled/morale/power/food events). The
// QML delegate already colour-codes by `warn` (green info / red warning), so this just needs a
// message buffer to read from once the sim generates events.
QVariantList TilesetView::commsMessages() const {
    return QVariantList();
}

// Space Program satellite status — { name, operational } per satellite. Sim-blocked (no
// space-program tracking yet), so each defaults to NOT operational; the space sim will flip these
// as the player launches satellites from the Spaceport.
QVariantList TilesetView::spaceReport() const {
    QVariantList list;
    const char* sats[] = { "Communications Satellite", "Solar Satellite" };
    for (const char* s : sats) {
        QVariantMap m;
        m[QStringLiteral("name")] = QString::fromLatin1(s);
        m[QStringLiteral("operational")] = false;
        list.append(m);
    }
    return list;
}

// space.txt Code_Name → display name. The codes are generic; the names come from the EXE's object-
// name POINTER TABLE (Outpost2.exe @ VA 0x4E9638), whose entries are in exactly space.txt's order —
// EDWARD, SOLAR_SAT, SPACE_1..15, SULV, RLV, EMP_MISSILE, LUX_1..3 (authoritative; also cross-checks
// against each code's research topic in the tech tree). See Review/findings/SPACE_PROGRAM.md.
namespace {
const QHash<QString, QString> kSpaceObjectName = {
    {QStringLiteral("EDWARD"),      QStringLiteral("EDWARD Satellite")},
    {QStringLiteral("SOLAR_SAT"),   QStringLiteral("Solar Satellite")},
    {QStringLiteral("SPACE_1"),     QStringLiteral("Ion Drive Module")},
    {QStringLiteral("SPACE_2"),     QStringLiteral("Fusion Drive Module")},
    {QStringLiteral("SPACE_3"),     QStringLiteral("Command Module")},
    {QStringLiteral("SPACE_4"),     QStringLiteral("Fueling Systems")},
    {QStringLiteral("SPACE_5"),     QStringLiteral("Habitat Ring")},
    {QStringLiteral("SPACE_6"),     QStringLiteral("Sensor Package")},
    {QStringLiteral("SPACE_7"),     QStringLiteral("Skydock")},
    {QStringLiteral("SPACE_8"),     QStringLiteral("Stasis Systems")},
    {QStringLiteral("SPACE_9"),     QStringLiteral("Orbital Package")},
    {QStringLiteral("SPACE_10"),    QStringLiteral("Phoenix Module")},
    {QStringLiteral("SPACE_11"),    QStringLiteral("Rare Metals Cargo")},
    {QStringLiteral("SPACE_12"),    QStringLiteral("Common Metals Cargo")},
    {QStringLiteral("SPACE_13"),    QStringLiteral("Food Cargo")},
    {QStringLiteral("SPACE_14"),    QStringLiteral("Evacuation Module")},
    {QStringLiteral("SPACE_15"),    QStringLiteral("Children Module")},
    {QStringLiteral("SULV"),        QStringLiteral("SULV")},
    {QStringLiteral("RLV"),         QStringLiteral("RLV")},
    {QStringLiteral("EMP_MISSILE"), QStringLiteral("EMP Missile")},
    {QStringLiteral("LUX_1"),       QStringLiteral("Impulse Items")},
    {QStringLiteral("LUX_2"),       QStringLiteral("Wares")},
    {QStringLiteral("LUX_3"),       QStringLiteral("Luxury Wares")},
};
} // namespace

void TilesetView::populateSpaceObjects() {
    spaceObjects_.clear();
    spaceObjectByName_.clear();
    const int rows = spaceSheet_.rowCount();
    for (int r = 0; r < rows; ++r) {
        const QString code = QString::fromStdString(spaceSheet_.cell(r, "Code_Name"));
        const auto it = kSpaceObjectName.constFind(code);
        if (it == kSpaceObjectName.constEnd()) continue; // skip header/garbage rows
        SpaceObject o;
        o.code = code;
        o.name = it.value();
        o.commonCost = spaceSheet_.cellInt(r, "Common Ore Cost", 0);
        o.rareCost   = spaceSheet_.cellInt(r, "Rare Ore Cost", 0);
        o.buildTime  = spaceSheet_.cellInt(r, "Build Time", 0);
        o.research   = QString::fromStdString(spaceSheet_.cell(r, "Research Topic"));
        spaceObjectByName_.insert(o.name, static_cast<int>(spaceObjects_.size()));
        spaceObjects_.push_back(o);
    }
    Logger::log(QStringLiteral("space.txt: %1 space objects loaded").arg(spaceObjects_.size()));
}

const TilesetView::SpaceObject* TilesetView::spaceObject(const QString& name) const {
    const auto it = spaceObjectByName_.constFind(name);
    return it == spaceObjectByName_.constEnd() ? nullptr : &spaceObjects_[it.value()];
}

// Space Program → Starship: the starship-operation components + research/build state (sim-blocked →
// "Not Researched") with their costs from space.txt (loaded into spaceObjects_).
QVariantList TilesetView::starshipReport() const {
    QVariantList list;
    const char* parts[] = { "Ion Drive Module", "Fusion Drive Module",
                            "Fueling Systems", "Skydock" };
    for (const char* p : parts) {
        const QString name = QString::fromLatin1(p);
        const SpaceObject* o = spaceObject(name);
        QVariantMap m;
        m[QStringLiteral("name")] = name;
        m[QStringLiteral("status")] = QStringLiteral("Not Researched"); // sim-blocked
        m[QStringLiteral("done")] = false;                              // done → green, else red
        m[QStringLiteral("common")] = o ? o->commonCost : 0;
        m[QStringLiteral("rare")]   = o ? o->rareCost : 0;
        m[QStringLiteral("build")]  = o ? o->buildTime : 0;
        list.append(m);
    }
    return list;
}

// Turn the parsed map resources (ore beacons, magma vents, fumaroles, objective
// markers, wreckage) into positioned sprites. These map to fixed op2_art FRAME indices
// (cross-checked against OP2MissionEditor's extracted frm-NNNNN art): ore beacons are a
// single static frame chosen by ore type + bar yield; vents/fumaroles/markers/wreckage
// are continuously-looping animations. We resolve the owning group from a representative
// frame so the whole animation + its pixel displacement come along for free.
void TilesetView::placeResources() {
    placedSprites_.clear();
    if (!mapLoaded_) return;

    for (int i = 0; i < static_cast<int>(missionResources_.size()); ++i) {
        const MissionResource& r = missionResources_[i];
        // .opm Position is 1-based game coords → 0-based cell (same as units).
        const int cellX = r.tileX - 1;
        const int cellY = r.tileY - 1;

        // Pick a representative frame + whether the object animates (loops the whole
        // owning group) or is a single static frame.
        int repFrame = -1;
        bool animate = false;
        bool isMine = false;
        if (r.kind == QStringLiteral("Beacon")) {
            if (r.subType == QStringLiteral("Fumarole")) { repFrame = 132;   animate = true; }
            else if (r.subType == QStringLiteral("MagmaVent")) { repFrame = 10007; animate = true; }
            else { // MiningBeacon
                isMine = true;
                if (!r.surveyed) {
                    // Unsurveyed: the animated WHITE "pre-surveyed" mine (group 1755, multiple
                    // frames; 18551 is its first). Type/yield unknown until a Robo-Surveyor reveals it.
                    repFrame = 18551; animate = true;
                } else {
                    // Surveyed: ore type + bar count. Bar count = sprite width (verified by
                    // frame inspection): 1 bar = g1749/1752 (8px) → frame 18480/18515,
                    // 2 bars = g1750/1753 (13px) → 18491/18527, 3 bars = g1751/1754 (18px) →
                    // 18503/18539. Each group's frames are the per-bar VARIANTs (mines.txt
                    // YIELD×VARIANT); we loop the group for now (variant-exact = later).
                    const bool rare = (r.oreType == QStringLiteral("Rare"));
                    int frame = rare ? 18539 : 18503;             // Bar3 (3 bars, High) = default
                    if (r.barYield == QStringLiteral("Bar2")) frame = rare ? 18527 : 18491;
                    else if (r.barYield == QStringLiteral("Bar1")) frame = rare ? 18515 : 18480;
                    repFrame = frame; animate = true;
                }
            }
        } else if (r.kind == QStringLiteral("Marker")) {
            if (r.subType == QStringLiteral("DNA")) repFrame = 23836;
            else if (r.subType == QStringLiteral("Beaker")) repFrame = 23854;
            else repFrame = 18608;                                // Circle (default)
            animate = true;
        } else if (r.kind == QStringLiteral("Wreckage")) {
            repFrame = 20338; animate = true;
        }
        if (repFrame < 0 || repFrame >= art_.frameCount()) continue;

        // Frame span: the whole owning group when animated, else just the chosen frame.
        const int grp = art_.groupOfFrame(repFrame);
        int first = repFrame, count = 1;
        if (animate && grp >= 0) { first = art_.groupFrameStart(grp); count = art_.groupFrameCount(grp); }
        int gdx = 0, gdy = 0;
        if (grp >= 0) art_.groupDisplacement(grp, &gdx, &gdy);

        PlacedSprite ps;
        ps.name = r.subType.isEmpty() ? r.kind : r.subType;
        ps.cellX = cellX; ps.cellY = cellY; ps.resourceIndex = i; ps.isMine = isMine;
        // Seat at the tile centre, displaced by the group anchor (like buildings/units):
        // each frame then draws at originX - anchor[f].
        ps.originX = cellX * T + T / 2 - gdx;
        ps.originY = cellY * T + T / 2 - gdy;
        for (int f = first; f < first + count; ++f) {
            int ax = 0, ay = 0;
            QImage img = art_.frameToQImage(f, &ax, &ay, shadowsEnabled_);
            if (img.isNull()) continue;
            ps.frames.push_back(std::move(img));
            ps.delays.push_back(animate ? qMax(1, art_.frameDelay(f)) : 0);
            ps.anchorX.push_back(ax);
            ps.anchorY.push_back(ay);
        }
        if (!ps.frames.empty()) placedSprites_.push_back(std::move(ps));
    }
}
