#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>

export module map:transform;

import std;
import core;

export namespace mts::map {

enum class MapErrorKind : core::u8 {
    Unknown,
    InvalidArgument,
    NotFound,
    Unavailable,
    Internal,
    Cancelled,
    ResourceExhausted,
    OutOfRange,
    DataLoss,
    PermissionDenied,
};

static_assert(core::ErrorKind<MapErrorKind>);

enum class MapBlend : std::uint8_t {
    Normal,
    Multiply,
    Screen,
    Overlay,
};

[[nodiscard]] auto blendModeFromString(std::string const& text) -> mts::core::Result<MapBlend> {
    if (text == "normal") {
        return MapBlend::Normal;
    }

    if (text == "multiply") {
        return MapBlend::Multiply;
    }

    if (text == "screen") {
        return MapBlend::Screen;
    }

    if (text == "overlay") {
        return MapBlend::Overlay;
    }

    return mts::core::Result<MapBlend>::err(mts::core::Error(MapErrorKind::InvalidArgument, "unknown blend mode: " + text));
}

[[nodiscard]] auto blendModeToString(MapBlend blend) -> std::string {
    switch (blend) {
        case MapBlend::Normal:
            return "normal";

        case MapBlend::Multiply:
            return "multiply";

        case MapBlend::Screen:
            return "screen";

        case MapBlend::Overlay:
            return "overlay";
    }

    return "normal";
}

struct MapTransform {
    double metersPerPixel = 1.0;
    double originX = 0.0;
    double originY = 0.0;
    bool flipY = true;
    std::string unitLabel = "m";

    [[nodiscard]] auto pixelToMetric(QPointF const& pixel) const -> QPointF {
        double const x = this->originX + pixel.x() * this->metersPerPixel;
        double const y = this->flipY
            ? this->originY - pixel.y() * this->metersPerPixel
            : this->originY + pixel.y() * this->metersPerPixel;

        return QPointF(x, y);
    }

    [[nodiscard]] auto metricToPixel(QPointF const& metric) const -> QPointF {
        double const x = (metric.x() - this->originX) / this->metersPerPixel;
        double const y = this->flipY
            ? (this->originY - metric.y()) / this->metersPerPixel
            : (metric.y() - this->originY) / this->metersPerPixel;

        return QPointF(x, y);
    }

    [[nodiscard]] auto metricRectToPixelRect(QRectF const& metricRect) const -> QRectF {
        QPointF const topLeft = this->metricToPixel(metricRect.topLeft());
        QPointF const bottomRight = this->metricToPixel(metricRect.bottomRight());

        return QRectF(topLeft, bottomRight).normalized();
    }

    [[nodiscard]] auto pixelRectToMetricRect(QRectF const& pixelRect) const -> QRectF {
        QPointF const topLeft = this->pixelToMetric(pixelRect.topLeft());
        QPointF const bottomRight = this->pixelToMetric(pixelRect.bottomRight());

        return QRectF(topLeft, bottomRight).normalized();
    }

    [[nodiscard]] auto worldRect(QSize const& baseSize) const -> QRectF {
        return this->pixelRectToMetricRect(
            QRectF(QPointF(0.0, 0.0), QSizeF(baseSize.width(), baseSize.height()))
        );
    }
};

struct MapInfo {
    std::string id;
    std::string name;
    int tileSize = 256;
    int minLevel = 0;
    int maxLevel = 0;
    QSize baseSize;
    MapTransform transform;
    QRectF worldBounds;
};
} // END OF NAMESPACE `mts::map`
