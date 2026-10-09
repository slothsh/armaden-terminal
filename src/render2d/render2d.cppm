#include <QCanvasPainter>
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QtMath>
#include <cmath>

export module render2d;

import ecs;

export namespace mts::render2d {

struct Camera {
    QPointF center;
    float scale = 1.0f;
    float rotation = 0.0f;
};

struct MainCamera {};

struct GridStyle {
    float spacing = 20.0f;
    QColor lineColor = QColor(28, 28, 28);
    QColor axisColor = QColor(82, 82, 82);
    bool drawAxes = true;
};

struct RenderContext {
    QCanvasPainter* painter = nullptr;
    QRectF canvasRect;
    float devicePixelRatio = 1.0f;
    float lineWidth = 1.0f;
};

auto mapWorldToScreen(Camera const& camera, QSizeF const& canvasSize, QPointF const& world) -> QPointF {
    QPointF const relative = world - camera.center;
    float const cosine = qCos(camera.rotation);
    float const sine = qSin(camera.rotation);
    QPointF const rotated(
        relative.x() * cosine - relative.y() * sine,
        relative.x() * sine + relative.y() * cosine
    );
    QPointF const canvasCenter(canvasSize.width() * 0.5f, canvasSize.height() * 0.5f);

    return canvasCenter + QPointF(rotated.x() * camera.scale, -rotated.y() * camera.scale);
}

auto mapScreenToWorld(Camera const& camera, QSizeF const& canvasSize, QPointF const& screen) -> QPointF {
    QPointF const canvasCenter(canvasSize.width() * 0.5f, canvasSize.height() * 0.5f);
    float const scaledX = (screen.x() - canvasCenter.x()) / camera.scale;
    float const scaledY = -(screen.y() - canvasCenter.y()) / camera.scale;
    float const cosine = qCos(camera.rotation);
    float const sine = qSin(camera.rotation);
    QPointF const relative(
        scaledX * cosine + scaledY * sine,
        -scaledX * sine + scaledY * cosine
    );

    return camera.center + relative;
}

auto screenOrigin(Camera const& camera, QSizeF const& canvasSize) -> QPointF {
    return mapWorldToScreen(camera, canvasSize, QPointF());
}

auto screenRectForWorldRect(Camera const& camera, QSizeF const& canvasSize, QRectF const& worldRect) -> QRectF {
    QPointF const topLeft = mapWorldToScreen(camera, canvasSize, worldRect.topLeft());
    QPointF const bottomRight = mapWorldToScreen(camera, canvasSize, worldRect.bottomRight());

    return QRectF(topLeft, bottomRight).normalized();
}

auto worldBounds(Camera const& camera, QRectF const& canvasRect) -> QRectF {
    QSizeF const canvasSize = canvasRect.size();
    QPointF const topLeft = mapScreenToWorld(camera, canvasSize, canvasRect.topLeft());
    QPointF const topRight = mapScreenToWorld(camera, canvasSize, canvasRect.topRight());
    QPointF const bottomLeft = mapScreenToWorld(camera, canvasSize, canvasRect.bottomLeft());
    QPointF const bottomRight = mapScreenToWorld(camera, canvasSize, canvasRect.bottomRight());
    float const left = qMin(qMin(topLeft.x(), topRight.x()), qMin(bottomLeft.x(), bottomRight.x()));
    float const right = qMax(qMax(topLeft.x(), topRight.x()), qMax(bottomLeft.x(), bottomRight.x()));
    float const bottom = qMin(qMin(topLeft.y(), topRight.y()), qMin(bottomLeft.y(), bottomRight.y()));
    float const top = qMax(qMax(topLeft.y(), topRight.y()), qMax(bottomLeft.y(), bottomRight.y()));

    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

auto drawGridSystem(RenderContext const& context, mts::ecs::registry& world, Camera const& camera) -> void {
    auto const gridView = world.view<GridStyle const>();
    QSizeF const canvasSize = context.canvasRect.size();

    for (auto const entity : gridView) {
        auto const& style = gridView.get<GridStyle const>(entity);
        QRectF const bounds = worldBounds(camera, context.canvasRect);
        float const firstVerticalGridLine = std::ceil(bounds.left() / style.spacing);
        float const lastVerticalGridLine = std::floor(bounds.right() / style.spacing);
        float const firstHorizontalGridLine = std::ceil(bounds.bottom() / style.spacing);
        float const lastHorizontalGridLine = std::floor(bounds.top() / style.spacing);

        context.painter->setFillStyle(style.lineColor);
        for (float x = firstVerticalGridLine; x <= lastVerticalGridLine; x += 1.0f) {
            float const screenX = mapWorldToScreen(camera, canvasSize, QPointF(x * style.spacing, 0.0f)).x();
            context.painter->fillRect(screenX, context.canvasRect.top(), context.lineWidth, context.canvasRect.height());
        }

        for (float y = firstHorizontalGridLine; y <= lastHorizontalGridLine; y += 1.0f) {
            float const screenY = mapWorldToScreen(camera, canvasSize, QPointF(0.0f, y * style.spacing)).y();
            context.painter->fillRect(context.canvasRect.left(), screenY, context.canvasRect.width(), context.lineWidth);
        }

        if (!style.drawAxes) {
            continue;
        }

        QPointF const axisOrigin = screenOrigin(camera, canvasSize);
        context.painter->setFillStyle(style.axisColor);

        if (axisOrigin.x() >= context.canvasRect.left() && axisOrigin.x() <= context.canvasRect.right()) {
            context.painter->fillRect(axisOrigin.x(), context.canvasRect.top(), context.lineWidth, context.canvasRect.height());
        }

        if (axisOrigin.y() >= context.canvasRect.top() && axisOrigin.y() <= context.canvasRect.bottom()) {
            context.painter->fillRect(context.canvasRect.left(), axisOrigin.y(), context.canvasRect.width(), context.lineWidth);
        }
    }
}

} // END OF NAMESPACE `mts::render2d`
