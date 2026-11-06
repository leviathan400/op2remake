#pragma once
#include <cstdint>

// =============================================================================
// Coordinates, headings and turning — the sim's spatial conventions
// =============================================================================
//
// Facts measured against the original game (op2-rs harness oracle,
// harness/oracle/FINDINGS.md and notes/; reimplemented here, no code taken):
//
//   * World pixel = tile * 32 + offset (0..31), origin at the top-left of FILE
//     tile (0,0), y down. Every sim position is in this file frame.
//   * A vehicle's pixel is its tile centre (+16, +16) when standing. A
//     structure's pixel is the centre of its footprint: for a footprint whose
//     top-left is (tx, ty) and size (w, h), px = tx*32 + w*16 (so a 2-wide one
//     sits on a tile EDGE). The .opm Position is the footprint's centre tile,
//     top-left = centre - size/2 (integer division).
//   * The engine's own tile x is file x + 32 on non-wrapping maps and file x on
//     512-wide maps, which wrap at 512; y is the same in both. Only needed when
//     comparing against engine memory or traces.
//   * Heading (MapObject rotation_, a byte): 0 = east, 64 = south, clockwise.
//     Units move in 8 directions, tile centre to tile centre.
//   * Turning happens on a tile centre at `turnRate` per tick, the shorter way;
//     an exact half turn goes counter-clockwise; within one step it snaps.

namespace op2::sim {

inline constexpr int kTilePx = 32;
inline constexpr int kTileCentrePx = 16;
inline constexpr int kWrapWidth = 512; // maps this wide wrap in x

// Floor division by the tile size (correct for negative pixels too).
constexpr int pixelToTile(int px) noexcept {
    return px >= 0 ? px / kTilePx : -((-px + kTilePx - 1) / kTilePx);
}

constexpr int tileCentrePx(int tile) noexcept { return tile * kTilePx + kTileCentrePx; }

// A structure's pixel on one axis, from its footprint's centre tile (.opm
// Position) and its size on that axis.
constexpr int structurePx(int centreTile, int size) noexcept {
    return (centreTile - size / 2) * kTilePx + size * (kTilePx / 2);
}

// A structure's top-left footprint tile on one axis, from its pixel.
constexpr int structureTopLeft(int px, int size) noexcept {
    return pixelToTile(px - size * (kTilePx / 2));
}

constexpr bool mapWraps(int mapWidth) noexcept { return mapWidth == kWrapWidth; }

// File tile x <-> the engine's internal tile x.
constexpr int internalTileX(int fileX, int mapWidth) noexcept {
    return mapWraps(mapWidth) ? fileX : fileX + 32;
}
constexpr int fileTileX(int internalX, int mapWidth) noexcept {
    return mapWraps(mapWidth) ? internalX : internalX - 32;
}

// A tile x brought onto the map: wraps on a 512-wide map, unchanged otherwise.
constexpr int wrapTileX(int x, int mapWidth) noexcept {
    if (!mapWraps(mapWidth)) return x;
    const int m = x % kWrapWidth;
    return m < 0 ? m + kWrapWidth : m;
}

// --- headings ---------------------------------------------------------------

namespace Heading {
    inline constexpr std::uint8_t East = 0;
    inline constexpr std::uint8_t South = 64;
    inline constexpr std::uint8_t West = 128;
    inline constexpr std::uint8_t North = 192;
}

// The 8 movement directions, in heading order (direction d = heading d*32).
struct TileStep { int dx, dy; };
inline constexpr TileStep kDirSteps[8] = {
    {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1},
};

constexpr std::uint8_t headingOfDir(int dir8) noexcept {
    return static_cast<std::uint8_t>((dir8 & 7) * 32);
}

// The 16 sprite facings (rotation >> 4), as the renderer picks chassis frames.
constexpr int facing16(std::uint8_t heading) noexcept { return (heading >> 4) & 0xF; }

// Signed shortest difference target - current, in -128..127 (a half turn is -128).
constexpr int headingDelta(std::uint8_t current, std::uint8_t target) noexcept {
    return static_cast<std::int8_t>(static_cast<std::uint8_t>(target - current));
}

// One tick of turning towards `target` at `rate`: snaps when within one step,
// else turns the shorter way, a half turn counter-clockwise.
constexpr std::uint8_t turnTowards(std::uint8_t current, std::uint8_t target, int rate) noexcept {
    const int d = headingDelta(current, target);
    if ((d < 0 ? -d : d) <= rate) return target;
    return static_cast<std::uint8_t>(d < 0 ? current - rate : current + rate);
}

// Sub-pixel position: while a unit steps, the 16.16 fraction of its pixel is
// kept beside it, biased by 0x8000 (half a pixel); a standing unit's is 0x8000.
inline constexpr std::uint16_t kSubPixelRest = 0x8000;

} // namespace op2::sim
