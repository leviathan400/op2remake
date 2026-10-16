#pragma once
#include <QHash>
#include <QImage>
#include <QString>
#include <QVector>

class QPainter;
class QRect;

// =============================================================================
// TilesetManager  —  loads OP2 "well" tileset bitmaps (render layer)
// =============================================================================
//
// OP2 tilesets are 8-bit indexed BMPs, 32 px wide, stored top-down. Each well
// (well0000.bmp, well0001.bmp, ...) is a vertical strip of N 32x32 tiles; a
// tile is the sub-rectangle (0, index*32, 32, 32). Qt's QImage reads these
// directly (palette + top-down handled for us).
//
// This is render-side only. The simulation deals in tile INDICES; the pixels
// live here.

class TilesetManager {
public:
    static constexpr int kTileSize = 32;

    // Load every well*.bmp in `dir` (sorted by name). Returns the well count.
    int loadFromDirectory(const QString& dir);

    int wellCount() const { return static_cast<int>(wells_.size()); }
    int tileCount(int wellIndex) const;   // tiles in one well
    int totalTileCount() const;

    QString wellName(int wellIndex) const;

    // Find a well by base name (no extension), e.g. "well0001". -1 if absent.
    int wellIndexByName(const QString& baseName) const;

    // Blit tile [tileIndex] of well [wellIndex] into `dest` (scaled to fit).
    void drawTile(QPainter& painter, const QRect& dest,
                  int wellIndex, int tileIndex) const;

    // Average colour of a tile (for the minimap). Cached per unique tile.
    QRgb tileAverageColor(int wellIndex, int tileIndex) const;

private:
    struct Well {
        QString name;   // base name without extension, e.g. "well0001"
        QImage image;   // the full 32 x (N*32) strip
        int tiles = 0;
    };
    QVector<Well> wells_;
    QHash<QString, int> nameToIndex_;
    mutable QHash<quint32, QRgb> avgCache_; // (well<<16|tile) -> average colour
};
