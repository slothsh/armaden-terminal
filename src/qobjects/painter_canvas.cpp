#include "painter_canvas.hpp"

#include <QCanvasImage>
#include <QCanvasPainter>
#include <QCanvasPainterItemRenderer>
#include <QImage>
#include <QMouseEvent>
#include <QRectF>
#include <QWheelEvent>

#include <cmath>

namespace {

constexpr float baseGridSpacing = 20.0f;
constexpr float minimumZDepth = -2.0f;
constexpr float maximumZDepth = 2.0f;
constexpr float wheelDepthStep = 0.1f;

float scaleForDepth(float zDepth) {
    return std::exp(zDepth);
}

struct Viewport {
    QPointF origin;
    float scale;

    QPointF mapWorldToScreen(const QPointF &point) const {
        return origin + QPointF(point.x() * scale, -point.y() * scale);
    }
};

class PainterCanvasRenderer final : public QCanvasPainterItemRenderer {
public:
    void initializeResources(QCanvasPainter *painter) override {
        if (!m_everonImage.isNull())
            return;

        m_everonImage = painter->addImage(QImage(":/assets/everon.png"));
    }

    void synchronizeData(QCanvasPainterItem *item) override {
        const auto *canvas = static_cast<const PainterCanvas *>(item);
        m_translationX = canvas->translationX();
        m_translationY = canvas->translationY();
        m_zDepth = canvas->zDepth();
    }

    void paint(QCanvasPainter *painter) override {
        const QRectF canvasRect(0, 0, width(), height());
        const float devicePixelRatio = qMax(1.0f, painter->devicePixelRatio());
        const float lineWidth = 1.0f / devicePixelRatio;
        const float scale = scaleForDepth(m_zDepth);
        const Viewport viewport {
            QPointF(width() * 0.5f + m_translationX, height() * 0.5f + m_translationY),
            scale,
        };

        painter->setFillStyle(QColor(24, 24, 24));
        painter->fillRect(canvasRect);

        const float leftWorld = -viewport.origin.x() / scale;
        const float rightWorld = (width() - viewport.origin.x()) / scale;
        const float bottomWorld = (viewport.origin.y() - height()) / scale;
        const float topWorld = viewport.origin.y() / scale;
        const float firstVerticalGridLine = std::ceil(leftWorld / baseGridSpacing);
        const float lastVerticalGridLine = std::floor(rightWorld / baseGridSpacing);
        const float firstHorizontalGridLine = std::ceil(bottomWorld / baseGridSpacing);
        const float lastHorizontalGridLine = std::floor(topWorld / baseGridSpacing);

        painter->setFillStyle(QColor(28, 28, 28));
        for (float x = firstVerticalGridLine; x <= lastVerticalGridLine; x += 1.0f) {
            const float screenX = viewport.mapWorldToScreen(QPointF(x * baseGridSpacing, 0.0f)).x();
            painter->fillRect(screenX, canvasRect.top(), lineWidth, canvasRect.height());
        }
        for (float y = firstHorizontalGridLine; y <= lastHorizontalGridLine; y += 1.0f) {
            const float screenY = viewport.mapWorldToScreen(QPointF(0.0f, y * baseGridSpacing)).y();
            painter->fillRect(canvasRect.left(), screenY, canvasRect.width(), lineWidth);
        }

        const QPointF axisOrigin = viewport.mapWorldToScreen(QPointF());
        painter->setFillStyle(QColor(82, 82, 82));
        if (axisOrigin.x() >= canvasRect.left() && axisOrigin.x() <= canvasRect.right())
            painter->fillRect(axisOrigin.x(), canvasRect.top(), lineWidth, canvasRect.height());
        if (axisOrigin.y() >= canvasRect.top() && axisOrigin.y() <= canvasRect.bottom())
            painter->fillRect(canvasRect.left(), axisOrigin.y(), canvasRect.width(), lineWidth);

        // if (!m_everonImage.isNull() && m_everonImage.width() > 0 && m_everonImage.height() > 0) {
        //     const QPointF topLeft = viewport.mapWorldToScreen(QPointF(
        //         -float(m_everonImage.width()) * 0.5f,
        //         float(m_everonImage.height()) * 0.5f
        //     ));
        //     painter->drawImage(
        //         m_everonImage,
        //         QRectF(
        //             topLeft,
        //             QSizeF(float(m_everonImage.width()) * scale, float(m_everonImage.height()) * scale)
        //         )
        //     );
        // }
    }

private:
    QCanvasImage m_everonImage;
    float m_translationX = 0.0f;
    float m_translationY = 0.0f;
    float m_zDepth = 0.0f;
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
