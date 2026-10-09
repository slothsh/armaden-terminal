#include "map_view.hpp"

#include <QPoint>
#include <QtGlobal>

#include <cmath>

namespace {

constexpr float kMinimumZDepth = -4.0f;
constexpr float kMaximumZDepth = 2.0f;
constexpr float kWheelDepthStep = 0.1f;
constexpr double kFitMargin = 0.92;

} // END OF NAMESPACE anonymous

auto MapView::scaleForDepth(float zDepth) -> float {
    return std::exp(zDepth);
}

auto MapView::depthForScale(double scale) -> float {
    return float(std::log(scale));
}

auto MapView::translationX() const -> float {
    return m_translationX;
}

auto MapView::setTranslationX(float translation) -> bool {
    if (qFuzzyCompare(m_translationX, translation)) {
        return false;
    }

    m_translationX = translation;

    return true;
}

auto MapView::translationY() const -> float {
    return m_translationY;
}

auto MapView::setTranslationY(float translation) -> bool {
    if (qFuzzyCompare(m_translationY, translation)) {
        return false;
    }

    m_translationY = translation;

    return true;
}

auto MapView::zDepth() const -> float {
    return m_zDepth;
}

auto MapView::setZDepth(float depth) -> bool {
    float const clampedDepth = qBound(kMinimumZDepth, depth, kMaximumZDepth);
    if (qFuzzyCompare(m_zDepth, clampedDepth)) {
        return false;
    }

    m_zDepth = clampedDepth;

    return true;
}

auto MapView::scale() const -> float {
    return MapView::scaleForDepth(m_zDepth);
}

auto MapView::cameraCenter() const -> QPointF {
    float const applied = this->scale();

    return QPointF(-m_translationX / applied, m_translationY / applied);
}

auto MapView::applyPan(QPointF const& delta) -> void {
    static_cast<void>(this->setTranslationX(m_translationX + float(delta.x())));
    static_cast<void>(this->setTranslationY(m_translationY + float(delta.y())));
}

auto MapView::applyZoom(float depthDelta, QPointF const& cursor, QSizeF const& canvasSize) -> void {
    float const previousScale = this->scale();
    float const nextDepth = qBound(kMinimumZDepth, m_zDepth + depthDelta * kWheelDepthStep, kMaximumZDepth);
    float const nextScale = MapView::scaleForDepth(nextDepth);
    QPointF const cursorOffset = cursor - QPointF(canvasSize.width() * 0.5f, canvasSize.height() * 0.5f);
    float const scaleRatio = nextScale / previousScale;

    static_cast<void>(this->setTranslationX(float(cursorOffset.x() - (cursorOffset.x() - m_translationX) * scaleRatio)));
    static_cast<void>(this->setTranslationY(float(cursorOffset.y() - (cursorOffset.y() - m_translationY) * scaleRatio)));
    static_cast<void>(this->setZDepth(nextDepth));
}

auto MapView::fitScale(QRectF const& worldBounds, QSizeF const& canvasSize) const -> double {
    if (worldBounds.isEmpty() || !(worldBounds.width() > 0.0) || !(worldBounds.height() > 0.0)) {
        return 0.0;
    }

    double const scale = qMin(
        canvasSize.width() * kFitMargin / worldBounds.width(),
        canvasSize.height() * kFitMargin / worldBounds.height()
    );

    return scale > 0.0 ? scale : 0.0;
}
