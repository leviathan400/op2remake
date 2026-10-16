#include "op2/maps/op2maploader.h"

#include "op2/sim/map.h"
#include "op2utility_c.h"

namespace op2::maps {

LoadResult Op2MapLoader::load(const std::string& path) const {
    Op2Result err = OP2_OK;
    Op2Map* m = op2_map_read(path.c_str(), &err);
    if (!m) {
        LoadResult r;
        r.ok = false;
        r.error = std::string("OP2Utility: ") + op2_last_error();
        return r;
    }

    LoadResult r;
    const std::uint32_t w = op2_map_width(m);
    const std::uint32_t h = op2_map_height(m);
    r.map.resize(static_cast<int>(w), static_cast<int>(h));

    // Tileset source filenames (well names the map references).
    const std::uint32_t nsrc = op2_map_tileset_source_count(m);
    r.map.tilesetSources.reserve(nsrc);
    for (std::uint32_t i = 0; i < nsrc; ++i) {
        char buf[64] = {0};
        if (op2_map_tileset_source_name(m, i, buf, sizeof(buf)) == OP2_OK) {
            r.map.tilesetSources.emplace_back(buf);
        } else {
            r.map.tilesetSources.emplace_back();
        }
    }

    // Per-cell: graphic (tileset + image index) and terrain (cell type).
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            op2::sim::Tile& t = r.map.at(static_cast<int>(x), static_cast<int>(y));
            std::int32_t cellType = 0;
            std::uint32_t tilesetIndex = 0;
            std::uint32_t imageIndex = 0;
            op2_map_cell_type(m, x, y, &cellType);
            op2_map_tileset_index(m, x, y, &tilesetIndex);
            op2_map_image_index(m, x, y, &imageIndex);
            t.cellType = static_cast<op2::sim::CellType>(cellType);
            t.tilesetIndex = static_cast<std::uint16_t>(tilesetIndex);
            t.imageIndex = static_cast<std::uint16_t>(imageIndex);
        }
    }

    op2_map_free(m);
    r.ok = true;
    return r;
}

} // namespace op2::maps
