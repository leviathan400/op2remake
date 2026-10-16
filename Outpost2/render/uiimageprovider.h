#pragma once
#include <QQuickImageProvider>
#include <QHash>
#include <QImage>
#include <QString>

// =============================================================================
// UiImageProvider  —  exposes OP2 interface (HUD) bitmaps to the QML UI layer
// =============================================================================
//
// OP2's HUD frame is built from bitmap pieces (the demo ships them as loose
// Bitmap_NNN.bmp files; retail keeps them in the EXE resources / .vol). Each
// piece is an 8-bit indexed bitmap whose background uses a single "transparent
// key" colour — palette index 0, RGB(20,20,20) / #141414 — for the areas that
// should let the game view / panels show through. We load the bitmap, colour-key
// that background to fully transparent, and hand it to QML:
//
//     Image { source: "image://ui/Bitmap_170" }   // <name> = file stem, no ".bmp"
//
// Files are read from a base directory (an interface "skin" — e.g. demo vs
// retail), set at startup from outpost2.ini ([UI] SkinDir). Nothing is baked
// into the binary, so no copyrighted HUD art is bundled in shared builds — the
// path points at the user's own files, exactly like op2_art.
//
// Loaded images are cached by name (the HUD pieces are small and reused).

class UiImageProvider : public QQuickImageProvider {
public:
    // keyColor = the background colour masked to transparent (OP2 default #141414).
    explicit UiImageProvider(QRgb keyColor = qRgb(20, 20, 20))
        : QQuickImageProvider(QQuickImageProvider::Image), keyColor_(keyColor) {}

    // Directory holding the skin's Bitmap_*.bmp files. Trailing slash optional.
    void setSkinDir(const QString& dir) { skinDir_ = dir; cache_.clear(); }
    QString skinDir() const { return skinDir_; }

    // A second, skin-INDEPENDENT directory for shared UI assets (buttons, etc.). Searched
    // after the skin dir, so a skin can still override a shared asset by name. This is where
    // custom/replaceable button graphics live (e.g. group50-0.png).
    void setCommonDir(const QString& dir) { commonDir_ = dir; cache_.clear(); }
    QString commonDir() const { return commonDir_; }

    // id = the bitmap file stem (e.g. "Bitmap_170"). Returns a null QImage if the
    // file is missing/unreadable.
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    QImage loadKeyed(const QString& name) const; // load + colour-key (uncached)

    QString skinDir_;
    QString commonDir_;
    QRgb keyColor_;
    mutable QHash<QString, QImage> cache_;
};
