#pragma once
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

#include "tilesetview.h"

// =============================================================================
// MinimapView  —  draws the downscaled map + the camera view box
// =============================================================================
//
// Reads its data from a TilesetView (set via the `source` property in QML):
// the prebuilt minimap image and the current camera rect. Click/drag jumps the
// main camera. No zoom/map-mode buttons yet.

class MinimapView : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TilesetView* source READ source WRITE setSource NOTIFY sourceChanged)
public:
    explicit MinimapView(QQuickItem* parent = nullptr);

    TilesetView* source() const { return source_; }
    void setSource(TilesetView* s);

    void paint(QPainter* painter) override;

signals:
    void sourceChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    // The fit transform: panel = minimap-image * s, offset (dx, dy). False if no map.
    bool computeFit(qreal& s, qreal& dx, qreal& dy) const;
    void jumpTo(qreal panelX, qreal panelY);

    TilesetView* source_ = nullptr;
};
