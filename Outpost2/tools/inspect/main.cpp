// op2inspect — play and inspect the remake's simulation headless.
//
//   op2inspect load --scenario FILE [--game-dir DIR]   start a session at tick 0
//   op2inspect step [--ticks N]                        run N ticks (default 1)
//   op2inspect units                                   live units in processing order
//   op2inspect region [--x X --y Y --w W --h H]        terrain + units as text
//   op2inspect info                                    tick, RNG, checksum, allocator
//   op2inspect order --unit N --cmd Move --x X --y Y   queue a command for the next tick
//                                    (or --cmd Stop, Idle, Unidle)
//
// Every call reads and rewrites a session file (--session, default
// op2inspect.session in the current directory). The session holds the scenario
// path, the game folder, the spawn-index -> unit handle table and a Game save
// (Game::serialize), so each call is also a save/restore round trip; the map and
// unit types are rebuilt from the scenario and the sheets.
//
// Scenarios use op2-rs's format (ENGINE_PLAN.md there): map, seed, setcell,
// spawn [{type, owner, tile, dir, weapon}], orders [{tick, unit, cmd, ...}].
// Tiles are map-file tiles; a structure's tile is its footprint's centre tile.
// Only orders the remake's command layer implements are carried out (Move,
// Stop, Idle, Unidle); the rest are reported as not simulated yet.
//
// No Qt: links op2sim + op2maps only.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "json.h"
#include "op2/maps/maploader.h"
#include "op2/sim/combat.h"
#include "op2/sim/command.h"
#include "op2/sim/coords.h"
#include "op2/sim/game.h"
#include "op2/sim/movement.h"
#include "op2/sim/sheet.h"

namespace fs = std::filesystem;
using op2::inspect::Json;
using namespace op2::sim;

namespace {

[[noreturn]] void die(const std::string& msg) {
    std::fprintf(stderr, "op2inspect: %s\n", msg.c_str());
    std::exit(1);
}

std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) die("cannot read " + p.string());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// --- command line --------------------------------------------------------------

struct Args {
    std::string command;
    std::map<std::string, std::string> opt; // --name value

    bool has(const std::string& name) const { return opt.count(name) > 0; }
    std::string get(const std::string& name, const std::string& def = {}) const {
        const auto it = opt.find(name);
        return it == opt.end() ? def : it->second;
    }
    int getInt(const std::string& name, int def) const {
        const std::string v = get(name);
        if (v.empty()) return def;
        try {
            return std::stoi(v);
        } catch (...) {
            die("--" + name + " takes a number");
        }
    }
};

Args parseArgs(int argc, char** argv) {
    Args a;
    if (argc < 2) return a;
    a.command = argv[1];
    for (int k = 2; k < argc; ++k) {
        std::string s = argv[k];
        if (s.rfind("--", 0) != 0 || k + 1 >= argc) die("expected --option value, got " + s);
        a.opt[s.substr(2)] = argv[++k];
    }
    return a;
}

// --- game data ---------------------------------------------------------------------

struct TypeInfo {
    std::string code;
    UnitType type;
};

// The first existing directory among the candidates under the game folder.
fs::path findDir(const fs::path& gameDir, std::initializer_list<const char*> rel) {
    for (const char* r : rel) {
        const fs::path p = gameDir / r;
        if (fs::is_directory(p)) return p;
    }
    return {};
}

// Unit types from vehicles.txt and building.txt (OPU 1.4.1 keeps them in
// OPU/base/sheets). typeId = position in this list: vehicles, then buildings,
// in sheet order, so every session rebuilds the same ids.
std::vector<TypeInfo> loadTypes(const fs::path& gameDir) {
    const fs::path dir = findDir(gameDir, {"OPU/base/sheets", "OPU/sheets", "sheets"});
    if (dir.empty()) die("no sheets folder under " + gameDir.string() + " (looked for OPU/base/sheets, OPU/sheets, sheets)");
    std::vector<TypeInfo> types;
    auto load = [&](const char* file, UnitKind kind) {
        Sheet sh;
        std::string err;
        if (!sh.load((dir / file).string(), &err)) die(err.empty() ? "cannot parse " + (dir / file).string() : err);
        for (std::size_t r = 0; r < sh.rowCount(); ++r) {
            TypeInfo t;
            t.code = sh.cell(r, "Code_Name");
            if (t.code.empty()) continue;
            t.type.kind = kind;
            t.type.maxHitPoints = sh.cellInt(r, "Hit Points", 0);
            t.type.armor = sh.cellInt(r, "Armor Class", 0);
            if (kind == UnitKind::Vehicle) {
                const std::string& track = sh.cell(r, "Track Type W/T/L/M");
                t.type.track = trackFromLetter(track.empty() ? 'W' : track[0]);
                t.type.moveSpeed = sh.cellInt(r, "Move Points", 0);
                t.type.turnRate = sh.cellInt(r, "Turn Rate", 0);
                t.type.sight = sh.cellInt(r, "Sight range", 0);
            } else {
                t.type.autoTarget = sh.cell(r, "Can be AutoTargeted") == "Y";
                t.type.explosionSize = static_cast<std::uint8_t>(std::clamp(sh.cellInt(r, "Explosion Size", 0), 0, 3));
                t.type.rubbleCommon = static_cast<std::uint8_t>(std::clamp(sh.cellInt(r, "Normal Rubble", 0), 0, 255));
                t.type.rubbleRare = static_cast<std::uint8_t>(std::clamp(sh.cellInt(r, "Rare Ore Rubble", 0), 0, 255));
                t.type.dockEden = static_cast<std::uint8_t>(std::clamp(sh.cellInt(r, "Docking Loc E", 0), 0, 8));
                t.type.dockPlymouth = static_cast<std::uint8_t>(std::clamp(sh.cellInt(r, "Docking Loc P", 0), 0, 8));
            }
            if (kind == UnitKind::Building) {
                t.type.fpX = static_cast<std::uint8_t>(std::max(1, sh.cellInt(r, "X-size", 1)));
                t.type.fpY = static_cast<std::uint8_t>(std::max(1, sh.cellInt(r, "Y-size", 1)));
                t.type.powerRequired = sh.cellInt(r, "Power required", 0);
                t.type.workersRequired = sh.cellInt(r, "Workers required", 0);
                t.type.scientistsRequired = sh.cellInt(r, "Scientists required", 0);
            }
            types.push_back(std::move(t));
        }
    };
    load("vehicles.txt", UnitKind::Vehicle);
    load("building.txt", UnitKind::Building);
    if (types.size() > 0xFFFF) die("too many unit types");
    return types;
}

struct WeaponInfo {
    std::string code;
    WeaponType type;
};

// Weapon types from weapons.txt; Unit::weapon is the position here + 1.
std::vector<WeaponInfo> loadWeapons(const fs::path& gameDir) {
    const fs::path dir = findDir(gameDir, {"OPU/base/sheets", "OPU/sheets", "sheets"});
    Sheet sh;
    std::string err;
    if (!sh.load((dir / "weapons.txt").string(), &err)) die(err.empty() ? "cannot parse weapons.txt" : err);
    std::vector<WeaponInfo> out;
    for (std::size_t r = 0; r < sh.rowCount() && out.size() < 250; ++r) {
        WeaponInfo w;
        w.code = sh.cell(r, "Code_Name");
        if (w.code.empty()) continue;
        w.type.kind = static_cast<std::uint8_t>(weaponKindFromCode(w.code));
        w.type.radius = sh.cellInt(r, "Damage Radius", 16);
        w.type.pixelsSkipped = sh.cellInt(r, "Pixels Skipped When Firing", 0);
        w.type.range = sh.cellInt(r, "Weapon Range in tiles", 1);
        w.type.turretRate = std::clamp(sh.cellInt(r, "Turret Turn Rate", 1), 1, 16); // clamped at load
        w.type.concussion = sh.cellInt(r, "Concussion Damage", 0);
        w.type.penetration = sh.cellInt(r, "Penetration Damage", 0);
        w.type.reload = sh.cellInt(r, "Reload Time", 0);
        out.push_back(std::move(w));
    }
    return out;
}

// A map by name: the path itself (or relative to the scenario), else the first
// match (sorted, case-insensitive) under OPU/base/maps, OPU/maps, then the game
// folder itself.
fs::path findMap(const std::string& name, const fs::path& scenarioDir, const fs::path& gameDir) {
    for (const fs::path& p : {fs::path(name), scenarioDir / name}) {
        if (fs::is_regular_file(p)) return p;
    }
    const std::string want = lower(fs::path(name).filename().string());
    for (const char* rel : {"OPU/base/maps", "OPU/maps"}) {
        const fs::path root = gameDir / rel;
        if (!fs::is_directory(root)) continue;
        std::vector<fs::path> found;
        for (const auto& e : fs::recursive_directory_iterator(root)) {
            if (e.is_regular_file() && lower(e.path().filename().string()) == want) found.push_back(e.path());
        }
        if (!found.empty()) {
            std::sort(found.begin(), found.end());
            return found.front();
        }
    }
    for (const auto& e : fs::directory_iterator(gameDir)) {
        if (e.is_regular_file() && lower(e.path().filename().string()) == want) return e.path();
    }
    die("map " + name + " not found (looked beside the scenario and under " + gameDir.string() +
        "/OPU/base/maps, OPU/maps and the game folder; is it an OPU 1.4.1 install?)");
}

// --- the session -------------------------------------------------------------------

struct Session {
    fs::path scenarioPath;
    fs::path gameDir;
    Json scenario;
    std::vector<TypeInfo> types;
    std::vector<WeaponInfo> weapons;
    std::vector<UnitId> ids; // spawn index -> handle
    fs::path mapPath;
    Game game;

    const std::string& typeName(std::uint16_t typeId) const {
        static const std::string unknown = "?";
        return typeId < types.size() ? types[typeId].code : unknown;
    }
    int spawnIndexOf(UnitId id) const {
        for (std::size_t k = 0; k < ids.size(); ++k)
            if (ids[k] == id) return static_cast<int>(k);
        return -1;
    }
};

// The inputs every call rebuilds: scenario, types, map (with setcell applied).
void buildInputs(Session& s) {
    s.scenario = [&] {
        try {
            return Json::parse(readFile(s.scenarioPath));
        } catch (const std::exception& e) {
            die(s.scenarioPath.string() + ": " + e.what());
        }
    }();
    s.types = loadTypes(s.gameDir);
    for (std::size_t k = 0; k < s.types.size(); ++k) s.game.world().setType(static_cast<std::uint16_t>(k), s.types[k].type);
    s.weapons = loadWeapons(s.gameDir);
    for (std::size_t k = 0; k < s.weapons.size(); ++k) s.game.world().setWeaponType(static_cast<std::uint8_t>(k), s.weapons[k].type);

    s.mapPath = findMap(s.scenario.at("map").asString(), s.scenarioPath.parent_path(), s.gameDir);
    const auto loaded = op2::maps::MapLoaderRegistry::withDefaults().load(s.mapPath.string());
    if (!loaded.ok) die("loading " + s.mapPath.string() + ": " + loaded.error);
    s.game.map() = loaded.map;
    Map& m = s.game.map();
    if (s.scenario.has("setcell")) {
        for (const Json& sc : s.scenario.at("setcell").asArray()) {
            const auto& r = sc.at("rect").asArray();
            if (r.size() != 4) die("setcell rect needs 4 numbers");
            const auto cell = static_cast<CellType>(sc.at("cell").asInt() & 31);
            for (std::int64_t y = r[1].asInt(); y <= r[3].asInt(); ++y)
                for (std::int64_t x = r[0].asInt(); x <= r[2].asInt(); ++x)
                    if (m.inBounds(static_cast<int>(x), static_cast<int>(y)))
                        m.at(static_cast<int>(x), static_cast<int>(y)).cellType = cell;
        }
    }
}

std::uint16_t typeIdFor(const Session& s, const std::string& code) {
    for (std::size_t k = 0; k < s.types.size(); ++k)
        if (s.types[k].code == code) return static_cast<std::uint16_t>(k);
    die("unknown unit type " + code + " (not in vehicles.txt or building.txt)");
}

void spawnAll(Session& s) {
    if (!s.scenario.has("spawn")) return;
    for (const Json& sp : s.scenario.at("spawn").asArray()) {
        Unit u;
        u.typeId = typeIdFor(s, sp.at("type").asString());
        const UnitType& t = s.game.world().type(u.typeId);
        u.kind = t.kind;
        u.owner = static_cast<std::uint8_t>(sp.intOr("owner", 0));
        // The op2-rs harness makes player 0 Eden and player 1 Plymouth.
        if (u.owner == 0) u.flags |= UnitFlag::IsEden;
        u.rotation = static_cast<std::uint8_t>(sp.intOr("dir", 0));
        const auto& tile = sp.at("tile").asArray();
        if (tile.size() != 2) die("spawn tile needs 2 numbers");
        u.px = structurePx(static_cast<int>(tile[0].asInt()), t.fpX);
        u.py = structurePx(static_cast<int>(tile[1].asInt()), t.fpY);
        if (sp.has("weapon")) {
            const std::string& code = sp.at("weapon").asString();
            for (std::size_t k = 0; k < s.weapons.size(); ++k)
                if (s.weapons[k].code == code) u.weapon = static_cast<std::uint8_t>(k + 1);
            if (!u.weapon) die("unknown weapon " + code);
            if (!weaponSupported(static_cast<WeaponKind>(s.weapons[u.weapon - 1].type.kind)))
                std::printf("note: %s isn't simulated yet; spawn %zu won't fire\n", code.c_str(), s.ids.size());
        }
        // The Tiger reloads twice as fast (op2-rs measured 35 >> 1; OP2's flag 0x20).
        if (sp.at("type").asString() == "BIG_TANK") u.flags |= UnitFlag::DoubleFire;
        // op2-rs's "powered": the structure has power, workers and scientists (the
        // remake has no colony economy to work that out).
        if (sp.has("powered") && sp.at("powered").kind == Json::Kind::Bool && sp.at("powered").b)
            u.flags |= UnitFlag::Powered;
        const UnitId id = s.game.world().spawn(u);
        if (!id.valid()) die("spawn failed: no free object index");
        s.ids.push_back(id);
    }
    // Linked (a tube group with an operational Command Center): with no tube
    // networks yet, a powered structure counts as linked when its footprint
    // touches one of its owner's Command Centers. An approximation.
    World& w = s.game.world();
    for (const UnitId& id : s.ids) {
        Unit* u = w.get(id);
        if (u->kind != UnitKind::Building || !(u->flags & UnitFlag::Powered)) continue;
        const UnitType& t = w.type(u->typeId);
        const int ux = structureTopLeft(u->px, t.fpX), uy = structureTopLeft(u->py, t.fpY);
        for (const UnitId& cid : s.ids) {
            const Unit* cc = w.get(cid);
            if (cid == id || cc->owner != u->owner || s.typeName(cc->typeId) != "COMMAND") continue;
            const UnitType& ct = w.type(cc->typeId);
            const int cx = structureTopLeft(cc->px, ct.fpX), cy = structureTopLeft(cc->py, ct.fpY);
            if (ux <= cx + ct.fpX && cx <= ux + t.fpX && uy <= cy + ct.fpY && cy <= uy + t.fpY) u->flags |= UnitFlag::Linked;
        }
    }
}

// Scenario orders due at the current tick become command packets.
void queueDueOrders(Session& s) {
    if (!s.scenario.has("orders")) return;
    for (const Json& o : s.scenario.at("orders").asArray()) {
        if (o.at("tick").asInt() != static_cast<std::int64_t>(s.game.tick())) continue;
        const std::string cmd = o.at("cmd").asString();
        const std::int64_t unit = o.at("unit").asInt();
        if (unit < 0 || unit >= static_cast<std::int64_t>(s.ids.size())) die("order for spawn " + std::to_string(unit) + " that doesn't exist");
        const UnitId id = s.ids[static_cast<std::size_t>(unit)];
        if (cmd == "Idle" || cmd == "Unidle") {
            s.game.queue(makeIdleCommand(s.game.tick(), 0, id, cmd == "Idle"));
        } else if (cmd == "Move") {
            const auto& tile = o.at("tile").asArray();
            if (tile.size() != 2) die("Move tile needs 2 numbers");
            s.game.queue(makeMoveCommand(s.game.tick(), 0, id, static_cast<int>(tile[0].asInt()), static_cast<int>(tile[1].asInt())));
        } else if (cmd == "Stop") {
            s.game.queue(makeStopCommand(s.game.tick(), 0, id));
        } else if (cmd == "Attack" && o.has("target")) {
            const std::int64_t t = o.at("target").asInt();
            if (t < 0 || t >= static_cast<std::int64_t>(s.ids.size())) die("Attack target spawn " + std::to_string(t) + " doesn't exist");
            s.game.queue(makeAttackCommand(s.game.tick(), 0, id, s.ids[static_cast<std::size_t>(t)]));
        } else {
            std::printf("tick %u: %s for spawn %lld not simulated yet\n", s.game.tick(), cmd.c_str(),
                        static_cast<long long>(unit));
        }
    }
}

// Session file: "O2IS", version, scenario path, game folder, spawn handles, Game save.
constexpr std::uint32_t kSessionMagic = 0x5349324F;
constexpr std::uint32_t kSessionVersion = 1;

void putU32(std::string& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b += static_cast<char>(v >> (8 * i));
}
void putStr(std::string& b, const std::string& s) {
    putU32(b, static_cast<std::uint32_t>(s.size()));
    b += s;
}

void saveSession(const Session& s, const fs::path& file) {
    std::string b;
    putU32(b, kSessionMagic);
    putU32(b, kSessionVersion);
    putStr(b, fs::absolute(s.scenarioPath).string());
    putStr(b, fs::absolute(s.gameDir).string());
    putU32(b, static_cast<std::uint32_t>(s.ids.size()));
    for (const UnitId& id : s.ids) {
        putU32(b, id.index);
        putU32(b, id.generation);
    }
    const std::vector<std::uint8_t> g = s.game.serialize();
    putU32(b, static_cast<std::uint32_t>(g.size()));
    b.append(g.begin(), g.end());
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    if (!f || !f.write(b.data(), static_cast<std::streamsize>(b.size()))) die("cannot write " + file.string());
}

Session loadSession(const fs::path& file) {
    if (!fs::is_regular_file(file)) die("no session (" + file.string() + "): run `op2inspect load --scenario FILE` first");
    const std::string b = readFile(file);
    std::size_t pos = 0;
    auto u32 = [&]() -> std::uint32_t {
        if (b.size() - pos < 4) die("session file is truncated");
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(static_cast<unsigned char>(b[pos + i])) << (8 * i);
        pos += 4;
        return v;
    };
    auto str = [&]() {
        const std::uint32_t n = u32();
        if (n > b.size() - pos) die("session file is truncated");
        std::string s = b.substr(pos, n);
        pos += n;
        return s;
    };
    if (u32() != kSessionMagic || u32() != kSessionVersion) die(file.string() + " is not an op2inspect session (or an older one)");
    Session s;
    s.scenarioPath = str();
    s.gameDir = str();
    const std::uint32_t n = u32();
    if (n > b.size() / 8) die("session file is corrupt");
    for (std::uint32_t k = 0; k < n; ++k) {
        UnitId id;
        id.index = u32();
        id.generation = u32();
        s.ids.push_back(id);
    }
    const std::uint32_t gn = u32();
    if (gn != b.size() - pos) die("session file is corrupt");
    const std::vector<std::uint8_t> g(b.begin() + static_cast<std::ptrdiff_t>(pos), b.end());
    buildInputs(s);
    if (!s.game.deserialize(g)) die("session save doesn't restore (corrupt, or from an older build)");
    return s;
}

// --- commands ------------------------------------------------------------------------

char cellChar(CellType c) {
    switch (c) {
        case CellType::FastPassible1: case CellType::FastPassible2: return '.';
        case CellType::MediumPassible1: case CellType::MediumPassible2: return ',';
        case CellType::SlowPassible1: case CellType::SlowPassible2: return ':';
        case CellType::Impassible1: case CellType::Impassible2: return '#';
        case CellType::NorthCliffs: case CellType::CliffsHighSide: case CellType::CliffsLowSide: return '^';
        case CellType::VentsAndFumaroles: return '*';
        case CellType::DozedArea: return '_';
        case CellType::Rubble: return '%';
        case CellType::NormalWall: case CellType::MicrobeWall: case CellType::LavaWall: return 'W';
        default: return static_cast<int>(c) >= 26 ? '=' : '?'; // tubes
    }
}

char spawnChar(int k) {
    if (k < 0) return '@';
    if (k < 10) return static_cast<char>('0' + k);
    if (k < 36) return static_cast<char>('a' + k - 10);
    return '@';
}

void cmdUnits(const Session& s) {
    const World& w = s.game.world();
    std::printf("tick %u\n", s.game.tick());
    w.forEachInOrder([&](UnitId id, const Unit& u) {
        if (u.kind == UnitKind::Weapon) {
            const WeaponInfo* wi = u.weapon ? &s.weapons[u.weapon - 1] : nullptr;
            std::printf("  shot (index %u) %s from spawn %d at spawn %d, impact (%d, %d)%s\n", id.index,
                        wi ? wi->code.c_str() : "?", s.spawnIndexOf(u.shotParent), s.spawnIndexOf(u.shotTarget),
                        u.impactX, u.impactY, u.shotDamaged ? " (hit)" : "");
            return;
        }
        if (u.kind == UnitKind::Other) {
            if (u.typeId == kExplosionType)
                std::printf("  explosion (index %u) px (%d, %d) %d ticks left\n", id.index, u.px, u.py, u.actionTimer);
            return;
        }
        const UnitType& t = w.type(u.typeId);
        const int k = s.spawnIndexOf(id);
        std::string route;
        if (u.command == Opcode::ctMoMove) {
            route = " -> (" + std::to_string(u.goalX) + ", " + std::to_string(u.goalY) + ") route " +
                    std::to_string(u.planPos) + "/" + std::to_string(u.planLen);
            if (u.stepTicks > 0) route += ", stepping to (" + std::to_string(u.destX) + ", " + std::to_string(u.destY) +
                                          ") " + std::to_string(u.stepTicks) + " ticks";
        }
        std::string gun;
        if (u.weapon) {
            gun = " " + s.weapons[u.weapon - 1].code + " tur " + std::to_string(u.turret) + " reload " +
                  std::to_string(u.reloadTimer);
            if (u.fireTarget.valid()) gun += " tgt " + std::to_string(s.spawnIndexOf(u.fireTarget));
        }
        std::printf("  spawn %d %s (index %u) owner %u px (%d, %d) tile (%d, %d) dir %u hp %d/%d %s%s%s%s%s%s\n", k,
                    s.typeName(u.typeId).c_str(), id.index, u.owner, u.px, u.py, pixelToTile(u.px), pixelToTile(u.py),
                    u.rotation, w.hitPoints(u), t.maxHitPoints, opcodeName(u.command).c_str(), route.c_str(), gun.c_str(),
                    (u.flags & UnitFlag::IsDead) ? " DEAD"
                    : (u.kind == UnitKind::Building && (u.flags & UnitFlag::Linked)) ? " linked"
                    : (u.kind == UnitKind::Building && (u.flags & UnitFlag::Powered)) ? " powered" : "",
                    (u.flags & UnitFlag::Idle) ? " idle" : "", (u.flags & UnitFlag::Disabled) ? " disabled" : "");
    });
}

void cmdRegion(const Session& s, const Args& a) {
    const Map& m = s.game.map();
    const World& w = s.game.world();
    // Default: the units' bounding box, 3 tiles of margin.
    int x0 = m.width(), y0 = m.height(), x1 = -1, y1 = -1;
    w.forEach([&](UnitId, const Unit& u) {
        x0 = std::min(x0, pixelToTile(u.px)); x1 = std::max(x1, pixelToTile(u.px));
        y0 = std::min(y0, pixelToTile(u.py)); y1 = std::max(y1, pixelToTile(u.py));
    });
    if (x1 < 0) { x0 = 0; y0 = 0; x1 = std::min(m.width(), 40) - 1; y1 = std::min(m.height(), 20) - 1; }
    else { x0 -= 3; y0 -= 3; x1 += 3; y1 += 3; }
    x0 = a.getInt("x", x0); y0 = a.getInt("y", y0);
    x1 = a.has("w") ? x0 + a.getInt("w", 1) - 1 : x1;
    y1 = a.has("h") ? y0 + a.getInt("h", 1) - 1 : y1;
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, m.width() - 1); y1 = std::min(y1, m.height() - 1);

    std::vector<std::string> rows;
    for (int y = y0; y <= y1; ++y) {
        std::string row;
        for (int x = x0; x <= x1; ++x) row += cellChar(m.at(x, y).cellType);
        rows.push_back(row);
    }
    w.forEach([&](UnitId id, const Unit& u) {
        if (u.kind != UnitKind::Vehicle && u.kind != UnitKind::Building) return; // shots, explosions
        const UnitType& t = w.type(u.typeId);
        const int tx = structureTopLeft(u.px, t.fpX), ty = structureTopLeft(u.py, t.fpY);
        for (int y = ty; y < ty + t.fpY; ++y)
            for (int x = tx; x < tx + t.fpX; ++x)
                if (x >= x0 && x <= x1 && y >= y0 && y <= y1) rows[y - y0][x - x0] = spawnChar(s.spawnIndexOf(id));
    });
    std::printf("tick %u, tiles (%d, %d)-(%d, %d); units by spawn index; . fast , medium : slow # impassable ^ cliff "
                "* vent _ dozed %% rubble W wall = tube\n", s.game.tick(), x0, y0, x1, y1);
    for (int y = y0; y <= y1; ++y) std::printf("  %4d %s\n", y, rows[y - y0].c_str());
}

void cmdInfo(const Session& s) {
    const Prng::State st = s.game.prng().state();
    std::printf("scenario %s\nmap      %s (%dx%d)\ntick     %u\nrng      [%u, %u]\nchecksum %08x\nunits    %d live\n",
                s.scenarioPath.string().c_str(), s.mapPath.string().c_str(), s.game.map().width(), s.game.map().height(),
                s.game.tick(), st.s0, st.s1, s.game.checksum(), s.game.world().liveCount());
}

} // namespace

int main(int argc, char** argv) {
    const Args a = parseArgs(argc, argv);
    const fs::path sessionFile = a.get("session", "op2inspect.session");

    if (a.command.empty() || a.command == "help" || a.command == "--help") {
        std::printf("usage: op2inspect load --scenario FILE [--game-dir DIR] | step [--ticks N] | units | region "
                    "[--x X --y Y --w W --h H] | info | order --unit N --cmd Move --x X --y Y|Stop|Idle|Unidle   "
                    "(all take --session FILE)\n");
        return a.command.empty() ? 1 : 0;
    }
    if (a.command == "load") {
        Session s;
        s.scenarioPath = a.get("scenario");
        if (s.scenarioPath.empty()) die("load needs --scenario FILE");
        std::string dir = a.get("game-dir");
        if (dir.empty()) {
            const char* env = std::getenv("OP2_DIR");
            if (env && *env) dir = env;
        }
        if (dir.empty()) die("load needs --game-dir DIR (an OPU 1.4.1 install) or $OP2_DIR");
        s.gameDir = dir;
        buildInputs(s);
        s.game.setSeed(static_cast<std::uint32_t>(s.scenario.intOr("seed", 1)));
        s.game.world().setHumanPlayers(0x01); // the op2-rs harness: player 0 a person, player 1 the computer
        spawnAll(s);
        saveSession(s, sessionFile);
        std::printf("loaded %s: map %s, %zu units, tick 0\n", s.scenarioPath.string().c_str(),
                    s.mapPath.filename().string().c_str(), s.ids.size());
        return 0;
    }

    Session s = loadSession(sessionFile);
    if (a.command == "step") {
        const int n = a.getInt("ticks", 1);
        if (n < 0) die("--ticks must be >= 0");
        for (int k = 0; k < n; ++k) {
            queueDueOrders(s);
            for (const CommandPacket& p : s.game.step())
                std::printf("tick %u: %s rejected (stale unit or bad packet)\n", p.tick, opcodeName(p.op).c_str());
        }
        saveSession(s, sessionFile);
        std::printf("tick %u\n", s.game.tick());
    } else if (a.command == "units") {
        cmdUnits(s);
    } else if (a.command == "region" || a.command == "sim_region") {
        cmdRegion(s, a);
    } else if (a.command == "info") {
        cmdInfo(s);
    } else if (a.command == "order") {
        const int k = a.getInt("unit", -1);
        if (k < 0 || k >= static_cast<int>(s.ids.size())) die("order needs --unit N, a spawn index");
        const std::string cmd = a.get("cmd");
        const UnitId id = s.ids[static_cast<std::size_t>(k)];
        if (cmd == "Idle" || cmd == "Unidle") {
            s.game.queue(makeIdleCommand(s.game.tick(), 0, id, cmd == "Idle"));
        } else if (cmd == "Move") {
            if (!a.has("x") || !a.has("y")) die("Move needs --x X --y Y (a map-file tile)");
            s.game.queue(makeMoveCommand(s.game.tick(), 0, id, a.getInt("x", 0), a.getInt("y", 0)));
        } else if (cmd == "Stop") {
            s.game.queue(makeStopCommand(s.game.tick(), 0, id));
        } else {
            die("--cmd Move (--x --y), Stop, Idle or Unidle (the orders the remake simulates so far)");
        }
        saveSession(s, sessionFile);
        std::printf("queued %s for spawn %d, applied on the next step\n", cmd.c_str(), k);
    } else {
        die("unknown command " + a.command + " (try help)");
    }
    return 0;
}
