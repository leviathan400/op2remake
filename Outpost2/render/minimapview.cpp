#include "minimapview.h"

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QtGlobal>

#include "tileset.h" // TilesetManager::kTileSize

MinimapView::MinimapView(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAcceptedMouseButtons(Qt::LeftButton);
}

void MinimapView::setSource(TilesetView* s) {
    if (source_ == s) return;
    if (source_) disconnect(source_, nullptr, this, nullptr);
    source_ = s;
    if (source_) {
        connect(source_, &TilesetView::cameraChanged, this, [this] { update(); });
    }
    emit sourceChanged();
    update();
}

// The minimap→panel transform: scale s fits the minimap image (1 px = 1 tile)
// inside the panel preserving aspect ratio (qMin of the two axis scales), and
// (dx,dy) centres the leftover space. panel = minimap*s + (dx,dy); every other
// method derives its mapping from these three values, forward or inverted.
bool MinimapView::computeFit(qreal& s, qreal& dx, qreal& dy) const {
    if (!source_) return false;
    const QImage& mm = source_->minimapImage();
    if (mm.isNull() || mm.width() <= 0 || mm.height() <= 0) return false;
    s = qMin(width() / mm.width(), height() / mm.height());
    dx = (width() - mm.width() * s) / 2.0;
    dy = (height() - mm.height() * s) / 2.0;
    return true;
}

void MinimapView::paint(QPainter* painter) {
    qreal s = 0, dx = 0, dy = 0;
    if (!computeFit(s, dx, dy)) return;

    const QImage& mm = source_->minimapImage();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(QRectF(dx, dy, mm.width() * s, mm.height() * s), mm,
                       QRectF(0, 0, mm.width(), mm.height()));

    // View box: map pixels -> minimap. 1 minimap pixel = 1 tile = T map pixels.
    const qreal k = s / TilesetManager::kTileSize;
    const QRectF box(dx + source_->cameraX() * k,
                     dy + source_->cameraY() * k,
                     source_->viewWidthMapPx() * k,
                     source_->viewHeightMapPx() * k);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(QPen(Qt::white, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(box);
}

void MinimapView::jumpTo(qreal panelX, qreal panelY) {
    qreal s = 0, dx = 0, dy = 0;
    if (!computeFit(s, dx, dy)) return;
    // Invert the fit transform (panel = tile*s + d  ⇒  tile = (panel − d)/s) to
    // get the clicked tile, then centre the camera on that tile's map pixel.
    const qreal tileX = (panelX - dx) / s;            // minimap px == tiles
    const qreal tileY = (panelY - dy) / s;
    source_->centerOnMap(tileX * TilesetManager::kTileSize,
                         tileY * TilesetManager::kTileSize);
}

void MinimapView::mousePressEvent(QMouseEvent* event) {
    jumpTo(event->position().x(), event->position().y());
    event->accept();
}

void MinimapView::mouseMoveEvent(QMouseEvent* event) {
    jumpTo(event->position().x(), event->position().y());
    event->accept();
}
