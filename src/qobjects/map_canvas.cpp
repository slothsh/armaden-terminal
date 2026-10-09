#include "map_canvas.hpp"

#include "map_controller.hpp"
#include "map_renderer.hpp"
#include "map_view.hpp"

#include <QMetaObject>
#include <QMouseEvent>
#include <QPoint>
#include <QRectF>
#include <QSizeF>
#include <QTimer>
#include <QtGlobal>
#include <QWheelEvent>

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
    this->setAcceptedMouseButtons(Qt::MiddleButton);
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

auto MapCanvas::createItemRenderer() const -> QCanvasPainterItemRenderer* {
    return new MapRenderer;
}

auto MapCanvas::handleTileUpdate() -> void {
    this->update();
}

auto MapCanvas::mousePressEvent(QMouseEvent* event) -> void {
    if (event->button() != Qt::MiddleButton) {
        event->ignore();
        return;
    }

    m_lastDragPosition = event->position();
    m_dragging = true;
    event->accept();
}

auto MapCanvas::mouseMoveEvent(QMouseEvent* event) -> void {
    if (!m_dragging) {
        event->ignore();
        return;
    }

    QPointF const position = event->position();
    QPointF const delta = position - m_lastDragPosition;
    m_lastDragPosition = position;

    float const previousX = m_view.translationX();
    float const previousY = m_view.translationY();
    m_view.applyPan(delta);

    if (!qFuzzyCompare(previousX, m_view.translationX())) {
        emit this->translationXChanged();
    }

    if (!qFuzzyCompare(previousY, m_view.translationY())) {
        emit this->translationYChanged();
    }

    restartIdleTimer(m_idleTimer);
    this->update();
    event->accept();
}

auto MapCanvas::mouseReleaseEvent(QMouseEvent* event) -> void {
    if (event->button() != Qt::MiddleButton || !m_dragging) {
        event->ignore();
        return;
    }

    m_dragging = false;
    event->accept();
}

auto MapCanvas::wheelEvent(QWheelEvent* event) -> void {
    QPoint const angleDelta = event->angleDelta();
    QPoint const pixelDelta = event->pixelDelta();
    float const delta = angleDelta.y() != 0
        ? float(angleDelta.y()) / 120.0f
        : float(pixelDelta.y()) / 100.0f;

    if (qFuzzyIsNull(delta)) {
        event->ignore();
        return;
    }

    float const previousX = m_view.translationX();
    float const previousY = m_view.translationY();
    float const previousDepth = m_view.zDepth();
    m_view.applyZoom(delta, event->position(), QSizeF(this->width(), this->height()));

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
    event->accept();
}
