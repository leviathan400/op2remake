#pragma once
#include <QQuickImageProvider>

#include "artfile.h"

// =============================================================================
// Op2ArtImageProvider  —  exposes op2_art.prt sprites to the QML UI layer
// =============================================================================
//
// Lets QML show any art sprite directly:
//     Image { source: "image://op2art/<group>"        }   // frame 0
//     Image { source: "image://op2art/<group>/<frame>" }   // a specific frame
// e.g. the OP2 button graphic is group 50 (frame 0 = normal, frame 1 = pressed).
//
// Holds its OWN ArtFile (the OP2_ART.BMP pixel blob is ~7 MB) and renders frames
// on demand — no pre-compositing, so it's cheap. Registered on the QML engine in
// main.cpp; the engine takes ownership.

class Op2ArtImageProvider : public QQuickImageProvider {
public:
    Op2ArtImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    bool load(const QString& prtPath, const QString& bmpPath) {
        return art_.load(prtPath, bmpPath, nullptr);
    }

    // id = "<group>" or "<group>/<frame>". Returns a null QImage on a bad id/group.
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    ArtFile art_;
};
