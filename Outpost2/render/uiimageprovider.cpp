#include "uiimageprovider.h"

#include <QDir>

QImage UiImageProvider::loadKeyed(const QString& name) const {
    // Resolve "<name>" (default extension .bmp if none given) against the skin dir first,
    // then the shared common dir — so a skin can override a shared asset, and shared assets
    // (e.g. custom buttons) resolve when the skin doesn't provide them.
    QString file = name;
    if (!file.contains(QLatin1Char('.'))) file += QStringLiteral(".bmp");

    QImage img;
    for (const QString& dir : { skinDir_, commonDir_ }) {
        if (dir.isEmpty()) continue;
        img = QImage(QDir(dir).filePath(file));
        if (!img.isNull()) break;
    }
    if (img.isNull()) return QImage();

    // Colour-key the background (#141414) to transparent so the masked-out areas
    // let whatever is behind the HUD piece (the game view / a panel) show through.
    // Both sides of the comparison are OR'd with an opaque alpha so only the RGB
    // channels are compared — source BMPs are alpha-less, but convertToFormat may
    // produce any alpha value, and a key miss on alpha alone would leave the
    // background opaque.
    img = img.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width(), h = img.height();
    const QRgb key = keyColor_ | 0xff000000u;
    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if ((line[x] | 0xff000000u) == key) line[x] = 0u; // fully transparent
        }
    }
    return img;
}

QImage UiImageProvider::requestImage(const QString& id, QSize* size,
                                     const QSize& requestedSize) {
    auto it = cache_.constFind(id);
    if (it == cache_.constEnd()) it = cache_.insert(id, loadKeyed(id));
    QImage img = it.value();

    if (size) *size = img.size();
    if (!requestedSize.isEmpty() && !img.isNull())
        img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return img;
}
