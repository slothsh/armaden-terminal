#include "painter_canvas.hpp"

#include <QCanvasImage>
#include <QCanvasPainter>
#include <QCanvasPainterItemRenderer>
#include <QImage>
#include <QMouseEvent>
#include <QRectF>
#include <QWheelEvent>

import std;
import ecs;

namespace {

constexpr float minimumZDepth = -2.0f;
constexpr float maximumZDepth = 2.0f;
constexpr float wheelDepthStep = 0.1f;

float scaleForDepth(float zDepth) {
    return std::exp(zDepth);
}

class PainterCanvasRenderer final : public QCanvasPainterItemRenderer {
public:
    void initializeResources(QCanvasPainter* painter) override {
        if (m_everonImage.isNull())
            m_everonImage = painter->addImage(QImage(":/assets/everon.png"));

        if (!m_world.valid(m_camera)) {
            m_camera = m_world.create();
            m_world.emplace<mts::ecs::Camera>(m_camera, mts::ecs::Camera{});
            m_world.emplace<mts::ecs::MainCamera>(m_camera);
        }

        if (!m_world.valid(m_grid)) {
            m_grid = m_world.create();
            m_world.emplace<mts::ecs::GridStyle>(m_grid, mts::ecs::GridStyle{});
        }
    }

    void synchronizeData(QCanvasPainterItem* item) override {
        auto const* canvas = static_cast<PainterCanvas const*>(item);
        if (!m_world.valid(m_camera))
            return;

        float const scale = scaleForDepth(canvas->zDepth());
        auto& camera = m_world.get<mts::ecs::Camera>(m_camera);
        camera.center = QPointF(-canvas->translationX() / scale, canvas->translationY() / scale);
        camera.scale = scale;
        camera.rotation = 0.0f;
    }

    void paint(QCanvasPainter* painter) override {
        QRectF const canvasRect(0, 0, this->width(), this->height());
        float const devicePixelRatio = qMax(1.0f, painter->devicePixelRatio());
        mts::ecs::RenderContext const context {
            painter,
            canvasRect,
            devicePixelRatio,
            1.0f / devicePixelRatio,
        };

        painter->setFillStyle(QColor(24, 24, 24));
        painter->fillRect(canvasRect);

        if (!m_world.valid(m_camera))
            return;

        mts::ecs::Camera const& camera = m_world.get<mts::ecs::Camera>(m_camera);
        mts::ecs::drawGridSystem(context, m_world, camera);
    }

private:
    mts::ecs::registry m_world;
    mts::ecs::entity m_camera = mts::ecs::null;
    mts::ecs::entity m_grid = mts::ecs::null;
    QCanvasImage m_everonImage;
};

}

PainterCanvas::PainterCanvas(QQuickItem* parent)
    : QCanvasPainterItem(parent)
{
    this->setAcceptedMouseButtons(Qt::MiddleButton);
}

float PainterCanvas::translationX() const {
    return m_translationX;
}

void PainterCanvas::setTranslationX(float translation) {
    if (qFuzzyCompare(m_translationX, translation))
        return;
    m_translationX = translation;
    emit this->translationXChanged();
    this->update();
}

float PainterCanvas::translationY() const {
    return m_translationY;
}

void PainterCanvas::setTranslationY(float translation) {
    if (qFuzzyCompare(m_translationY, translation))
        return;
    m_translationY = translation;
    emit this->translationYChanged();
    this->update();
}

float PainterCanvas::zDepth() const {
    return m_zDepth;
}

void PainterCanvas::setZDepth(float depth) {
    float const clampedDepth = qBound(minimumZDepth, depth, maximumZDepth);
    if (qFuzzyCompare(m_zDepth, clampedDepth))
        return;
    m_zDepth = clampedDepth;
    emit this->zDepthChanged();
    this->update();
}

QCanvasPainterItemRenderer* PainterCanvas::createItemRenderer() const {
    return new PainterCanvasRenderer;
}

void PainterCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::MiddleButton) {
        event->ignore();
        return;
    }

    m_lastDragPosition = event->position();
    m_dragging = true;
    event->accept();
}

void PainterCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (!m_dragging) {
        event->ignore();
        return;
    }

    QPointF const position = event->position();
    QPointF const delta = position - m_lastDragPosition;
    m_lastDragPosition = position;
    this->setTranslationX(m_translationX + float(delta.x()));
    this->setTranslationY(m_translationY + float(delta.y()));
    event->accept();
}

void PainterCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::MiddleButton || !m_dragging) {
        event->ignore();
        return;
    }

    m_dragging = false;
    event->accept();
}

void PainterCanvas::wheelEvent(QWheelEvent* event) {
    QPoint const angleDelta = event->angleDelta();
    QPoint const pixelDelta = event->pixelDelta();
    float const delta = angleDelta.y() != 0
        ? float(angleDelta.y()) / 120.0f
        : float(pixelDelta.y()) / 100.0f;

    if (qFuzzyIsNull(delta)) {
        event->ignore();
        return;
    }

    float const previousScale = scaleForDepth(m_zDepth);
    float const nextDepth = qBound(
        minimumZDepth,
        m_zDepth + delta * wheelDepthStep,
        maximumZDepth
    );
    float const nextScale = scaleForDepth(nextDepth);
    QPointF const cursorOffset = event->position() - QPointF(this->width() * 0.5f, this->height() * 0.5f);
    float const scaleRatio = nextScale / previousScale;

    this->setTranslationX(float(cursorOffset.x() - (cursorOffset.x() - m_translationX) * scaleRatio));
    this->setTranslationY(float(cursorOffset.y() - (cursorOffset.y() - m_translationY) * scaleRatio));
    this->setZDepth(nextDepth);

    event->accept();
}
