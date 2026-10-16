#pragma once
#include "op2/maps/maploader.h"

// Loader for the ".opm" format used by OP2MissionEditor.
//
//   https://github.com/leviathan400/OP2MissionEditor
//
// .opm is a JSON document holding MISSION-level data (players, units, triggers,
// resources, ...) plus a reference to the underlying .map to load. So load()
// will: parse the JSON, populate mission data (later, once we model missions),
// and set LoadResult.referencedMapFile so the caller chains to the .map loader.
// Until then, load() returns a not-implemented LoadResult.

namespace op2::maps {

class OpmLoader : public IMapLoader {
public:
    const char* name() const override { return "OP2MissionEditor .opm"; }
    const char* extension() const override { return ".opm"; }
    LoadResult load(const std::string& path) const override;
};

} // namespace op2::maps
