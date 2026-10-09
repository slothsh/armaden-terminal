#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

class MapView {
public:
    [[nodiscard]] static auto scaleForDepth(float zDepth) -> float;
    [[nodiscard]] static auto depthForScale(double scale) -> float;

    [[nodiscard]] auto translationX() const -> float;
    auto setTranslationX(float translation) -> bool;

    [[nodiscard]] auto translationY() const -> float;
    auto setTranslationY(float translation) -> bool;

    [[nodiscard]] auto zDepth() const -> float;
    auto setZDepth(float depth) -> bool;

    [[nodiscard]] auto scale() const -> float;
    [[nodiscard]] auto cameraCenter() const -> QPointF;

    auto applyPan(QPointF const& delta) -> void;
    auto applyZoom(float depthDelta, QPointF const& cursor, QSizeF const& canvasSize) -> void;
    [[nodiscard]] auto fitScale(QRectF const& worldBounds, QSizeF const& canvasSize) const -> double;

private:
    float m_translationX = 0.0f;
    float m_translationY = 0.0f;
    float m_zDepth = 0.0f;
};
