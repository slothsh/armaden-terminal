#include <entt/entt.hpp>
#include <cmath>
#include <QCanvasImage>
#include <QCanvasPainter>
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QtMath>

export module ecs;

export namespace mts::ecs {
    using ::entt::entity;
    using ::entt::registry;
    using ::entt::handle;
    using ::entt::exclude_t;
    using ::entt::null;
    using ::entt::to_integral;

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

    struct Sprite {
        QCanvasImage image;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct RenderLayer {
        int value = 0;
    };

    struct RenderContext {
        QCanvasPainter *painter = nullptr;
        QRectF canvasRect;
        float devicePixelRatio = 1.0f;
        float lineWidth = 1.0f;
    };

    QPointF mapWorldToScreen(const Camera &camera, const QSizeF &canvasSize, const QPointF &world) {
        const QPointF relative = world - camera.center;
        const float cosine = qCos(camera.rotation);
        const float sine = qSin(camera.rotation);
        const QPointF rotated(
            relative.x() * cosine - relative.y() * sine,
            relative.x() * sine + relative.y() * cosine
        );
        const QPointF canvasCenter(canvasSize.width() * 0.5f, canvasSize.height() * 0.5f);
        return canvasCenter + QPointF(rotated.x() * camera.scale, -rotated.y() * camera.scale);
    }

    QPointF mapScreenToWorld(const Camera &camera, const QSizeF &canvasSize, const QPointF &screen) {
        const QPointF canvasCenter(canvasSize.width() * 0.5f, canvasSize.height() * 0.5f);
        const float scaledX = (screen.x() - canvasCenter.x()) / camera.scale;
        const float scaledY = -(screen.y() - canvasCenter.y()) / camera.scale;
        const float cosine = qCos(camera.rotation);
        const float sine = qSin(camera.rotation);
        const QPointF relative(
            scaledX * cosine + scaledY * sine,
            -scaledX * sine + scaledY * cosine
        );
        return camera.center + relative;
    }

    QPointF screenOrigin(const Camera &camera, const QSizeF &canvasSize) {
        return mapWorldToScreen(camera, canvasSize, QPointF());
    }

    QRectF worldBounds(const Camera &camera, const QRectF &canvasRect) {
        const QSizeF canvasSize = canvasRect.size();
        const QPointF topLeft = mapScreenToWorld(camera, canvasSize, canvasRect.topLeft());
        const QPointF topRight = mapScreenToWorld(camera, canvasSize, canvasRect.topRight());
        const QPointF bottomLeft = mapScreenToWorld(camera, canvasSize, canvasRect.bottomLeft());
        const QPointF bottomRight = mapScreenToWorld(camera, canvasSize, canvasRect.bottomRight());
        const float left = qMin(qMin(topLeft.x(), topRight.x()), qMin(bottomLeft.x(), bottomRight.x()));
        const float right = qMax(qMax(topLeft.x(), topRight.x()), qMax(bottomLeft.x(), bottomRight.x()));
        const float bottom = qMin(qMin(topLeft.y(), topRight.y()), qMin(bottomLeft.y(), bottomRight.y()));
        const float top = qMax(qMax(topLeft.y(), topRight.y()), qMax(bottomLeft.y(), bottomRight.y()));
        return QRectF(QPointF(left, top), QPointF(right, bottom));
    }

    void drawGridSystem(const RenderContext &context, registry &world, const Camera &camera) {
        const auto gridView = world.view<const GridStyle>();
        const QSizeF canvasSize = context.canvasRect.size();

        for (const auto entity : gridView) {
            const auto &style = gridView.get<const GridStyle>(entity);
            const QRectF bounds = worldBounds(camera, context.canvasRect);
            const float firstVerticalGridLine = std::ceil(bounds.left() / style.spacing);
            const float lastVerticalGridLine = std::floor(bounds.right() / style.spacing);
            const float firstHorizontalGridLine = std::ceil(bounds.bottom() / style.spacing);
            const float lastHorizontalGridLine = std::floor(bounds.top() / style.spacing);

            context.painter->setFillStyle(style.lineColor);
            for (float x = firstVerticalGridLine; x <= lastVerticalGridLine; x += 1.0f) {
                const float screenX = mapWorldToScreen(camera, canvasSize, QPointF(x * style.spacing, 0.0f)).x();
                context.painter->fillRect(screenX, context.canvasRect.top(), context.lineWidth, context.canvasRect.height());
            }
            for (float y = firstHorizontalGridLine; y <= lastHorizontalGridLine; y += 1.0f) {
                const float screenY = mapWorldToScreen(camera, canvasSize, QPointF(0.0f, y * style.spacing)).y();
                context.painter->fillRect(context.canvasRect.left(), screenY, context.canvasRect.width(), context.lineWidth);
            }

            if (!style.drawAxes)
                continue;

            const QPointF axisOrigin = screenOrigin(camera, canvasSize);
            context.painter->setFillStyle(style.axisColor);
            if (axisOrigin.x() >= context.canvasRect.left() && axisOrigin.x() <= context.canvasRect.right())
                context.painter->fillRect(axisOrigin.x(), context.canvasRect.top(), context.lineWidth, context.canvasRect.height());
            if (axisOrigin.y() >= context.canvasRect.top() && axisOrigin.y() <= context.canvasRect.bottom())
                context.painter->fillRect(context.canvasRect.left(), axisOrigin.y(), context.canvasRect.width(), context.lineWidth);
        }
    }
}
