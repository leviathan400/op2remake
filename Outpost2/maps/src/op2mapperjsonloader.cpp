#include "op2/maps/op2mapperjsonloader.h"

namespace op2::maps {

LoadResult Op2MapperJsonLoader::load(const std::string& path) const {
    // TODO: parse the OP2Mapper .json directly into op2::sim::Map.
    //   1. Read JSON document (self-contained: tiles + cell types).
    //   2. map.resize(width, height).
    //   3. For each cell: set Tile.tileIndex + Tile.cellType.
    // OP2Mapper: https://github.com/leviathan400/OP2Mapper
    LoadResult r;
    r.ok = false;
    r.error = "Op2MapperJsonLoader: .json parsing not implemented yet (path: " + path + ")";
    return r;
}

} // namespace op2::maps
