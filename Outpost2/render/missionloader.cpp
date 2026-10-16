#include "missionloader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

int opmDirectionIndex(const QString& name) {
    // Editor camelCase names; reads are case-insensitive (see OPM_FORMAT.md).
    const QString d = name.trimmed().toLower();
    if (d == QStringLiteral("east"))      return 0;
    if (d == QStringLiteral("southeast")) return 1;
    if (d == QStringLiteral("south"))     return 2;
    if (d == QStringLiteral("southwest")) return 3;
    if (d == QStringLiteral("west"))      return 4;
    if (d == QStringLiteral("northwest")) return 5;
    if (d == QStringLiteral("north"))     return 6;
    if (d == QStringLiteral("northeast")) return 7;
    return 0;
}

bool loadOpmMission(const QString& path, MissionData* out, QString* error) {
    if (!out) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("cannot open ") + path;
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (doc.isNull() || !doc.isObject()) {
        if (error) *error = QStringLiteral("JSON parse: ") + pe.errorString();
        return false;
    }
    const QJsonObject root = doc.object();
    const QJsonObject ld = root.value(QStringLiteral("LevelDetails")).toObject();
    out->mapName = ld.value(QStringLiteral("MapName")).toString();
    out->description = ld.value(QStringLiteral("LevelDescription")).toString();

    // Both MasterVariant and any MissionVariants carry Players; the master is the
    // base every mission uses, which is all our test files populate.
    const QJsonObject mv = root.value(QStringLiteral("MasterVariant")).toObject();
    const QJsonArray players = mv.value(QStringLiteral("Players")).toArray();
    // First pass for ownership: the "local" player is the first human; friendly = it + its
    // allies (the computer overlay only lights our own + allied buildings).
    int localPlayer = -1;
    for (const QJsonValue& pv : players) {
        const QJsonObject p = pv.toObject();
        if (localPlayer < 0 && p.value(QStringLiteral("IsHuman")).toBool(false))
            localPlayer = p.value(QStringLiteral("ID")).toInt();
    }
    if (localPlayer < 0) localPlayer = 0; // no human marked → assume player 0
    out->localPlayer = localPlayer;
    out->friendlyPlayers.push_back(localPlayer);
    for (const QJsonValue& pv : players) {
        const QJsonObject p = pv.toObject();
        if (p.value(QStringLiteral("ID")).toInt() != localPlayer) continue;
        for (const QJsonValue& av : p.value(QStringLiteral("Allies")).toArray())
            out->friendlyPlayers.push_back(av.toInt());
    }
    for (const QJsonValue& pv : players) {
        const QJsonObject p = pv.toObject();
        const int owner = p.value(QStringLiteral("ID")).toInt();
        const bool isEden = p.value(QStringLiteral("IsEden")).toBool(true);
        const QJsonArray units = p.value(QStringLiteral("Resources")).toObject()
                                     .value(QStringLiteral("Units")).toArray();
        for (const QJsonValue& uv : units) {
            const QJsonObject u = uv.toObject();
            MissionObject mo;
            mo.typeId = u.value(QStringLiteral("TypeID")).toString();
            if (mo.typeId.isEmpty()) continue;
            mo.owner = owner;
            mo.isEden = isEden;
            const QJsonObject pos = u.value(QStringLiteral("Position")).toObject();
            mo.tileX = pos.value(QStringLiteral("X")).toInt();
            mo.tileY = pos.value(QStringLiteral("Y")).toInt();
            mo.direction = opmDirectionIndex(u.value(QStringLiteral("Direction")).toString());
            mo.cargoType = u.value(QStringLiteral("CargoType")).toString();
            mo.cargoAmount = u.value(QStringLiteral("CargoAmount")).toInt();
            out->objects.push_back(std::move(mo));
        }
    }

    // Map resources/markers/wreckage live under MasterVariant.TethysGame (NOT per-player).
    // Position is the same 1-based "game coordinate" as units; the caller subtracts (1,1).
    const QJsonObject tethys = mv.value(QStringLiteral("TethysGame")).toObject();
    auto readPos = [](const QJsonObject& o, MissionResource& r) {
        const QJsonObject pos = o.value(QStringLiteral("Position")).toObject();
        r.tileX = pos.value(QStringLiteral("X")).toInt();
        r.tileY = pos.value(QStringLiteral("Y")).toInt();
    };
    for (const QJsonValue& bv : tethys.value(QStringLiteral("Beacons")).toArray()) {
        const QJsonObject b = bv.toObject();
        MissionResource r;
        r.kind = QStringLiteral("Beacon");
        r.subType = b.value(QStringLiteral("MapID")).toString();   // MiningBeacon/MagmaVent/Fumarole
        r.oreType = b.value(QStringLiteral("OreType")).toString();
        r.barYield = b.value(QStringLiteral("BarYield")).toString();
        readPos(b, r);
        out->resources.push_back(std::move(r));
    }
    for (const QJsonValue& mvw : tethys.value(QStringLiteral("Markers")).toArray()) {
        const QJsonObject m = mvw.toObject();
        MissionResource r;
        r.kind = QStringLiteral("Marker");
        r.subType = m.value(QStringLiteral("MarkerType")).toString(); // Circle/DNA/Beaker
        readPos(m, r);
        out->resources.push_back(std::move(r));
    }
    for (const QJsonValue& wv : tethys.value(QStringLiteral("Wreckage")).toArray()) {
        const QJsonObject w = wv.toObject();
        MissionResource r;
        r.kind = QStringLiteral("Wreckage");                          // single sprite (TechID ignored)
        readPos(w, r);
        out->resources.push_back(std::move(r));
    }
    return true;
}
