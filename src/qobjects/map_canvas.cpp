#include "map_canvas.hpp"

#include "map_controller.hpp"
#include "map_renderer.hpp"
#include "map_view.hpp"

#include <QMetaObject>
#include <QPoint>
#include <QRectF>
#include <QSizeF>
#include <QTimer>
#include <QtGlobal>

import map;

namespace {

constexpr int kIdleReleaseTimerMs = 1200;

auto restartIdleTimer(QTimer* timer) -> void {
    if (timer != nullptr) {
        timer->start();
    }
}

} // END OF NAMESPACE anonymous

MapCanvas::MapCanvas(QQuickItem* parent)
    : QCanvasPainterItem(parent)
    , m_idleTimer(new QTimer(this)) {
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(kIdleReleaseTimerMs);
    this->connect(m_idleTimer, &QTimer::timeout, this, [this] { this->update(); });
    this->connect(this, &QQuickItem::widthChanged, this, [this] { this->maybeApplyInitialView(); });
    this->connect(this, &QQuickItem::heightChanged, this, [this] { this->maybeApplyInitialView(); });
}

auto MapCanvas::componentComplete() -> void {
    QCanvasPainterItem::componentComplete();
    this->maybeApplyInitialView();
}

auto MapCanvas::maybeApplyInitialView() -> void {
    if (m_initialViewApplied) {
        return;
    }

    if (m_controller == nullptr || m_controller->loader() == nullptr) {
        return;
    }

    if (this->width() <= 0.0 || this->height() <= 0.0) {
        return;
    }

    this->fitToMap();
}

auto MapCanvas::fitToMap() -> void {
    if (m_controller == nullptr || m_controller->loader() == nullptr) {
        return;
    }

    if (this->width() <= 0.0 || this->height() <= 0.0) {
        return;
    }

    QRectF const bounds = m_controller->loader()->info().worldBounds;
    double const scale = m_view.fitScale(bounds, QSizeF(this->width(), this->height()));
    if (!(scale > 0.0)) {
        return;
    }

    this->setZDepth(MapView::depthForScale(scale));
    float const applied = m_view.scale();
    QPointF const center = bounds.center();
    this->setTranslationX(float(-center.x() * applied));
    this->setTranslationY(float(center.y() * applied));
    m_initialViewApplied = true;
}

auto MapCanvas::setController(MapController* controller) -> void {
    if (m_controller.data() == controller) {
        return;
    }

    m_controller = controller;
    QObject::disconnect(m_readyConnection);
    if (m_controller != nullptr) {
        m_readyConnection = this->connect(m_controller.data(), &MapController::readyChanged, this, [this] { this->maybeApplyInitialView(); });
    }

    m_initialViewApplied = false;

    emit this->controllerChanged();
    this->maybeApplyInitialView();
    this->update();
}

auto MapCanvas::controller() const -> MapController* {
    return m_controller.data();
}

auto MapCanvas::translationX() const -> float {
    return m_view.translationX();
}

auto MapCanvas::setTranslationX(float translation) -> void {
    if (!m_view.setTranslationX(translation)) {
        return;
    }

    emit this->translationXChanged();
    restartIdleTimer(m_idleTimer);
    this->update();
}

auto MapCanvas::translationY() const -> float {
    return m_view.translationY();
}

auto MapCanvas::setTranslationY(float translation) -> void {
    if (!m_view.setTranslationY(translation)) {
        return;
    }

    emit this->translationYChanged();
    restartIdleTimer(m_idleTimer);
    this->update();
}

auto MapCanvas::zDepth() const -> float {
    return m_view.zDepth();
}

auto MapCanvas::setZDepth(float depth) -> void {
    if (!m_view.setZDepth(depth)) {
        return;
    }

    emit this->zDepthChanged();
    restartIdleTimer(m_idleTimer);
    this->update();
}

auto MapCanvas::view() const -> MapView const& {
    return m_view;
}

auto MapCanvas::interactionPan(qreal dx, qreal dy) -> void {
    float const previousX = m_view.translationX();
    float const previousY = m_view.translationY();
    m_view.applyPan(QPointF(dx, dy));

    if (!qFuzzyCompare(previousX, m_view.translationX())) {
        emit this->translationXChanged();
    }

    if (!qFuzzyCompare(previousY, m_view.translationY())) {
        emit this->translationYChanged();
    }

    restartIdleTimer(m_idleTimer);
    this->update();
}

auto MapCanvas::interactionZoom(qreal delta, qreal x, qreal y) -> void {
    if (qFuzzyIsNull(delta)) {
        return;
    }

    float const previousX = m_view.translationX();
    float const previousY = m_view.translationY();
    float const previousDepth = m_view.zDepth();
    m_view.applyZoom(float(delta), QPointF(x, y), QSizeF(this->width(), this->height()));

    if (!qFuzzyCompare(previousX, m_view.translationX())) {
        emit this->translationXChanged();
    }

    if (!qFuzzyCompare(previousY, m_view.translationY())) {
        emit this->translationYChanged();
    }

    if (!qFuzzyCompare(previousDepth, m_view.zDepth())) {
        emit this->zDepthChanged();
    }

    restartIdleTimer(m_idleTimer);
    this->update();
}

auto MapCanvas::createItemRenderer() const -> QCanvasPainterItemRenderer* {
    return new MapRenderer;
}

auto MapCanvas::handleTileUpdate() -> void {
    this->update();
}
