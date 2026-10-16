#pragma once
#include "op2/maps/maploader.h"

// Loader for the OP2Mapper ".json" format.
//
//   https://github.com/leviathan400/OP2Mapper
//
// A self-contained JSON map: full tile grid and cell types, no external .map
// needed. load() will parse the JSON directly into op2::sim::Map. Until then,
// load() returns a not-implemented LoadResult.

namespace op2::maps {

class Op2MapperJsonLoader : public IMapLoader {
public:
    const char* name() const override { return "OP2Mapper .json"; }
    const char* extension() const override { return ".json"; }
    LoadResult load(const std::string& path) const override;
};

} // namespace op2::maps
