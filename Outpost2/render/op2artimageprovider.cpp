#include "op2artimageprovider.h"

QImage Op2ArtImageProvider::requestImage(const QString& id, QSize* size,
                                         const QSize& requestedSize) {
    // id = "<group>" (frame 0) or "<group>/<frame>".
    // Edge cases: a non-numeric id or unknown group returns a null QImage (QML
    // shows nothing, no warning); an out-of-range frame falls back to frame 0
    // (a visible-but-wrong frame beats an invisible button).
    const QStringList parts = id.split(QLatin1Char('/'));
    bool okG = false, okF = true;
    const int group = parts.value(0).toInt(&okG);
    const int frame = parts.size() > 1 ? parts.value(1).toInt(&okF) : 0;
    if (!okG || !okF) return QImage();

    const int fs = art_.groupFrameStart(group);
    if (fs < 0) return QImage();
    const int fc = art_.groupFrameCount(group);
    const int f = (frame >= 0 && frame < fc) ? frame : 0;

    QImage img = art_.frameToQImage(fs + f, nullptr, nullptr, false);
    if (size) *size = img.size();
    if (!requestedSize.isEmpty() && !img.isNull())
        img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return img;
}
