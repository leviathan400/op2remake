#pragma once
#include <cstdint>
#include <string>
#include <vector>

// =============================================================================
// Map  —  the in-memory tile grid (pure sim data)
// =============================================================================
//
// The common representation every map-format loader produces. Lives in op2sim
// (the deterministic core) because the simulation owns the map at runtime
// (pathfinding reads cell types, placement reads tiles). Pure integer/STL data —
// no I/O, no Qt. Loaders live in the separate `op2maps` library.

namespace op2::sim {

// Terrain class of a tile. Values match OP2Utility's CellType (a 5-bit field).
enum class CellType : std::uint8_t {
    FastPassible1     = 0,  // rock vegetation
    Impassible2       = 1,  // meteor craters, cracks/crevasses
    SlowPassible1     = 2,  // lava rock (dark)
    SlowPassible2     = 3,  // rippled dirt / lava-rock bumps
    MediumPassible1   = 4,  // dirt
    MediumPassible2   = 5,  // lava rock
    Impassible1       = 6,  // mound/ice/volcano
    FastPassible2     = 7,  // rock
    NorthCliffs       = 8,
    CliffsHighSide    = 9,
    CliffsLowSide     = 10,
    VentsAndFumaroles = 11,
    zPad12 = 12, zPad13 = 13, zPad14 = 14, zPad15 = 15,
    zPad16 = 16, zPad17 = 17, zPad18 = 18, zPad19 = 19, zPad20 = 20,
    DozedArea   = 21,
    Rubble      = 22,
    NormalWall  = 23,
    MicrobeWall = 24,
    LavaWall    = 25,
    Tube0 = 26, Tube1 = 27, Tube2 = 28, Tube3 = 29, Tube4 = 30, Tube5 = 31,
};

struct Tile {
    // Which tileset source (well) this tile comes from — index into
    // Map::tilesetSources. With imageIndex, identifies the graphic.
    std::uint16_t tilesetIndex = 0;
    // Tile index within that well.
    std::uint16_t imageIndex = 0;
    // Terrain class (passability) — what the sim cares about.
    CellType cellType = CellType::FastPassible1;
};

class Map {
public:
    // Tileset source filenames the map references (e.g. "well0001", no ext).
    // A Tile::tilesetIndex indexes into this list.
    std::vector<std::string> tilesetSources;

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    bool empty() const noexcept { return tiles_.empty(); }

    void resize(int w, int h) {
        width_ = w;
        height_ = h;
        tiles_.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), Tile{});
    }

    bool inBounds(int x, int y) const noexcept {
        return x >= 0 && y >= 0 && x < width_ && y < height_;
    }

    Tile& at(int x, int y) { return tiles_[index(x, y)]; }
    const Tile& at(int x, int y) const { return tiles_[index(x, y)]; }

    const std::vector<Tile>& tiles() const noexcept { return tiles_; }

private:
    std::size_t index(int x, int y) const noexcept {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(x);
    }

    int width_ = 0;
    int height_ = 0;
    std::vector<Tile> tiles_;
};

} // namespace op2::sim
