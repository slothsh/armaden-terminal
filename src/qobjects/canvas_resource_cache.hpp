#pragma once

#include <QCanvasImage>
#include <QHash>
#include <QImage>
#include <QSet>
#include <QString>

#include <chrono>

class QCanvasPainter;

struct CachedMapImage {
    QCanvasImage image;
    double opacity = -1.0;
};

class CanvasResourceCache {
public:
    auto beginFrame() -> void;
    auto markSeen(QString const& key) -> void;
    auto endFrame() -> void;

    [[nodiscard]] auto find(QString const& key) const -> CachedMapImage const*;
    auto insert(
        QCanvasPainter* painter,
        QString const& key,
        QImage const& source,
        double opacity
    ) -> CachedMapImage const&;

    auto maybeRelease(QCanvasPainter* painter, std::chrono::steady_clock::time_point lastCameraChange) -> void;
    auto reset() -> void;

private:
    QHash<QString, CachedMapImage> m_images;
    QSet<QString> m_seen;
};
