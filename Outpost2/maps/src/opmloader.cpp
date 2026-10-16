#include "op2/maps/opmloader.h"

#include <cstdio>
#include <string>

namespace op2::maps {
namespace {

// Minimal JSON helper — enough to pull a top-level string field out of an
// OP2MissionEditor .opm. The .opm is machine-generated JSON, so a targeted scan
// is reliable; full mission parsing (players/units) will want a real JSON parser.
std::string jsonStringField(const std::string& doc, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    std::size_t p = doc.find(needle);
    if (p == std::string::npos) return {};
    p += needle.size();
    while (p < doc.size() && (doc[p] == ' ' || doc[p] == '\t' || doc[p] == '\r' ||
                              doc[p] == '\n' || doc[p] == ':'))
        ++p;
    if (p >= doc.size() || doc[p] != '"') return {};
    ++p;
    std::string val;
    while (p < doc.size() && doc[p] != '"') {
        if (doc[p] == '\\' && p + 1 < doc.size()) {
            const char n = doc[p + 1];
            switch (n) {
                case 'n': val += '\n'; break;
                case 't': val += '\t'; break;
                case 'r': val += '\r'; break;
                default:  val += n;    break; // covers \" \\ \/
            }
            p += 2;
        } else {
            val += doc[p++];
        }
    }
    return val;
}

std::string directoryOf(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? std::string() : path.substr(0, slash + 1);
}

} // namespace

// The .opm carries no tiles itself — it names a companion .map (LevelDetails.MapName)
// that holds the terrain. We resolve that .map (relative to the .opm's own folder) and
// hand it back via referencedMapFile; the registry then loads it. Mission content
// (players, pre-placed units, beacons) is NOT parsed yet — that lands once the remake
// models missions and we add a real JSON parser. See OPM_FORMAT.md.
LoadResult OpmLoader::load(const std::string& path) const {
    LoadResult r;

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        r.ok = false;
        r.error = "OpmLoader: cannot open " + path;
        return r;
    }
    std::string doc;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) doc.append(buf, n);
    std::fclose(f);

    const std::string mapName = jsonStringField(doc, "MapName");
    if (mapName.empty()) {
        r.ok = false;
        r.error = "OpmLoader: no LevelDetails.MapName in " + path;
        return r;
    }

    // Resolve the .map next to the .opm. (If MapName is already absolute, use it.)
    const bool absolute = mapName.size() > 1 &&
                          (mapName[1] == ':' || mapName[0] == '/' || mapName[0] == '\\');
    r.referencedMapFile = absolute ? mapName : directoryOf(path) + mapName;
    r.ok = false; // no tiles here — the registry follows referencedMapFile to the .map
    return r;
}

} // namespace op2::maps
