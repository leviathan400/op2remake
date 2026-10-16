#include "tileset.h"

#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QRect>
#include <QtGlobal>

int TilesetManager::loadFromDirectory(const QString& dir) {
    wells_.clear();
    nameToIndex_.clear();

    QDir d(dir);
    const QStringList files =
        d.entryList(QStringList() << QStringLiteral("well*.bmp"), QDir::Files, QDir::Name);

    for (const QString& f : files) {
        const QString path = d.filePath(f);
        QImage img(path);
        if (img.isNull()) {
            qWarning("TilesetManager: failed to load %s", qUtf8Printable(path));
            continue;
        }
        Well w;
        w.name = QFileInfo(f).completeBaseName(); // "well0001.bmp" -> "well0001"
        w.image = std::move(img);
        w.tiles = w.image.height() / kTileSize;
        nameToIndex_.insert(w.name, static_cast<int>(wells_.size()));
        wells_.push_back(std::move(w));
    }

    if (wells_.isEmpty()) {
        qWarning("TilesetManager: no wells loaded from %s", qUtf8Printable(dir));
    }
    return wellCount();
}

int TilesetManager::tileCount(int wellIndex) const {
    if (wellIndex < 0 || wellIndex >= wells_.size()) return 0;
    return wells_[wellIndex].tiles;
}

int TilesetManager::totalTileCount() const {
    int n = 0;
    for (const Well& w : wells_) n += w.tiles;
    return n;
}

QString TilesetManager::wellName(int wellIndex) const {
    if (wellIndex < 0 || wellIndex >= wells_.size()) return {};
    return wells_[wellIndex].name;
}

int TilesetManager::wellIndexByName(const QString& baseName) const {
    return nameToIndex_.value(baseName, -1);
}

void TilesetManager::drawTile(QPainter& painter, const QRect& dest,
                              int wellIndex, int tileIndex) const {
    if (wellIndex < 0 || wellIndex >= wells_.size()) return;
    const Well& w = wells_[wellIndex];
    if (tileIndex < 0 || tileIndex >= w.tiles) return;

    const QRect src(0, tileIndex * kTileSize, kTileSize, kTileSize);
    painter.drawImage(dest, w.image, src);
}

QRgb TilesetManager::tileAverageColor(int wellIndex, int tileIndex) const {
    if (wellIndex < 0 || wellIndex >= wells_.size()) return qRgb(0, 0, 0);
    const Well& w = wells_[wellIndex];
    if (tileIndex < 0 || tileIndex >= w.tiles) return qRgb(0, 0, 0);

    const quint32 key = (static_cast<quint32>(wellIndex) << 16) |
                        (static_cast<quint32>(tileIndex) & 0xFFFFu);
    const auto it = avgCache_.constFind(key);
    if (it != avgCache_.constEnd()) return it.value();

    quint64 r = 0, g = 0, b = 0;
    const int y0 = tileIndex * kTileSize;
    for (int y = 0; y < kTileSize; ++y) {
        for (int x = 0; x < kTileSize; ++x) {
            const QRgb px = w.image.pixel(x, y0 + y);
            r += qRed(px);
            g += qGreen(px);
            b += qBlue(px);
        }
    }
    const int n = kTileSize * kTileSize;
    const QRgb avg = qRgb(static_cast<int>(r / n), static_cast<int>(g / n),
                          static_cast<int>(b / n));
    avgCache_.insert(key, avg);
    return avg;
}
