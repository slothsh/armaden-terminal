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
    void initializeResources(QCanvasPainter *painter) override {
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

        if (!m_world.valid(m_sprite) && !m_everonImage.isNull()) {
            m_sprite = m_world.create();
            m_world.emplace<mts::ecs::Sprite>(m_sprite, mts::ecs::Sprite{
                m_everonImage,
                float(m_everonImage.width()),
                float(m_everonImage.height()),
            });
        }
    }

    void synchronizeData(QCanvasPainterItem *item) override {
        const auto *canvas = static_cast<const PainterCanvas *>(item);
        if (!m_world.valid(m_camera))
            return;

        const float scale = scaleForDepth(canvas->zDepth());
        auto &camera = m_world.get<mts::ecs::Camera>(m_camera);
        camera.center = QPointF(-canvas->translationX() / scale, canvas->translationY() / scale);
        camera.scale = scale;
        camera.rotation = 0.0f;
    }

    void paint(QCanvasPainter *painter) override {
        const QRectF canvasRect(0, 0, width(), height());
        const float devicePixelRatio = qMax(1.0f, painter->devicePixelRatio());
        const mts::ecs::RenderContext context {
            painter,
            canvasRect,
            devicePixelRatio,
            1.0f / devicePixelRatio,
        };

        painter->setFillStyle(QColor(24, 24, 24));
        painter->fillRect(canvasRect);

        if (!m_world.valid(m_camera))
            return;

        const mts::ecs::Camera &camera = m_world.get<mts::ecs::Camera>(m_camera);
        mts::ecs::drawGridSystem(context, m_world, camera);
        mts::ecs::drawSpriteSystem(context, m_world, camera);
    }

private:
    mts::ecs::registry m_world;
    mts::ecs::entity m_camera = mts::ecs::null;
    mts::ecs::entity m_grid = mts::ecs::null;
    mts::ecs::entity m_sprite = mts::ecs::null;
    QCanvasImage m_everonImage;
};

}

PainterCanvas::PainterCanvas(QQuickItem *parent)
    : QCanvasPainterItem(parent)
{
    setAcceptedMouseButtons(Qt::MiddleButton);
}

float PainterCanvas::translationX() const {
    return m_translationX;
}

void PainterCanvas::setTranslationX(float translation) {
    if (qFuzzyCompare(m_translationX, translation))
        return;
    m_translationX = translation;
    emit translationXChanged();
    update();
}

float PainterCanvas::translationY() const {
    return m_translationY;
}

void PainterCanvas::setTranslationY(float translation) {
    if (qFuzzyCompare(m_translationY, translation))
        return;
    m_translationY = translation;
    emit translationYChanged();
    update();
}

float PainterCanvas::zDepth() const {
    return m_zDepth;
}

void PainterCanvas::setZDepth(float depth) {
    const float clampedDepth = qBound(minimumZDepth, depth, maximumZDepth);
    if (qFuzzyCompare(m_zDepth, clampedDepth))
        return;
    m_zDepth = clampedDepth;
    emit zDepthChanged();
    update();
}

QCanvasPainterItemRenderer *PainterCanvas::createItemRenderer() const {
    return new PainterCanvasRenderer;
}

void PainterCanvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::MiddleButton) {
        event->ignore();
        return;
    }

    m_lastDragPosition = event->position();
    m_dragging = true;
    event->accept();
}

void PainterCanvas::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragging) {
        event->ignore();
        return;
    }

    const QPointF position = event->position();
    const QPointF delta = position - m_lastDragPosition;
    m_lastDragPosition = position;
    setTranslationX(m_translationX + float(delta.x()));
    setTranslationY(m_translationY + float(delta.y()));
    event->accept();
}

void PainterCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::MiddleButton || !m_dragging) {
        event->ignore();
        return;
    }

    m_dragging = false;
    event->accept();
}

void PainterCanvas::wheelEvent(QWheelEvent *event) {
    const QPoint angleDelta = event->angleDelta();
    const QPoint pixelDelta = event->pixelDelta();
    const float delta = angleDelta.y() != 0
        ? float(angleDelta.y()) / 120.0f
        : float(pixelDelta.y()) / 100.0f;

    if (qFuzzyIsNull(delta)) {
        event->ignore();
        return;
    }

    const float previousScale = scaleForDepth(m_zDepth);
    const float nextDepth = qBound(
        minimumZDepth,
        m_zDepth + delta * wheelDepthStep,
        maximumZDepth
    );
    const float nextScale = scaleForDepth(nextDepth);
    const QPointF cursorOffset = event->position() - QPointF(width() * 0.5f, height() * 0.5f);
    const float scaleRatio = nextScale / previousScale;

    setTranslationX(float(cursorOffset.x() - (cursorOffset.x() - m_translationX) * scaleRatio));
    setTranslationY(float(cursorOffset.y() - (cursorOffset.y() - m_translationY) * scaleRatio));
    setZDepth(nextDepth);

    event->accept();
}
