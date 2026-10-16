#pragma once
#include "op2/maps/maploader.h"

// Loader for the original Outpost 2 binary ".map" format.
//
// Implementation plan: parse via OP2Utility
//   https://github.com/OutpostUniverse/OP2Utility
// OP2Utility already round-trips the .map container (tile grid, tileset mapping,
// cell-type "tile group" data). We'll vendor/submodule it and convert its
// MapData into op2::sim::Map. Until then, load() returns a not-implemented
// LoadResult.

namespace op2::maps {

class Op2MapLoader : public IMapLoader {
public:
    const char* name() const override { return "Outpost 2 .map (OP2Utility)"; }
    const char* extension() const override { return ".map"; }
    LoadResult load(const std::string& path) const override;
};

} // namespace op2::maps
