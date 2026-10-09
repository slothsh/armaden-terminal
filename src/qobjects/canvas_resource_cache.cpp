#include "canvas_resource_cache.hpp"

#include <QCanvasPainter>

#include <cmath>

namespace {

constexpr auto kIdleReleaseDelay = std::chrono::milliseconds(1000);
constexpr qsizetype kMaximumCachedImages = 1200;
constexpr qsizetype kMaximumCachedImageBytes = 384LL * 1024LL * 1024LL;
constexpr double kOpacityEpsilon = 0.001;

} // END OF NAMESPACE anonymous

auto CanvasResourceCache::beginFrame() -> void {
    m_seen.clear();
}

auto CanvasResourceCache::markSeen(QString const& key) -> void {
    m_seen.insert(key);
}

auto CanvasResourceCache::endFrame() -> void {
    auto it = m_images.begin();
    while (it != m_images.end()) {
        if (m_seen.contains(it.key())) {
            ++it;
            continue;
        }

        it = m_images.erase(it);
    }
}

auto CanvasResourceCache::find(QString const& key) const -> CachedMapImage const* {
    auto const it = m_images.constFind(key);

    return it == m_images.constEnd() ? nullptr : &it.value();
}

auto CanvasResourceCache::insert(
    QCanvasPainter* painter,
    QString const& key,
    QImage const& source,
    double opacity
) -> CachedMapImage const& {
    auto it = m_images.find(key);
    if (it != m_images.end() && std::abs(it->opacity - opacity) <= kOpacityEpsilon) {
        return it.value();
    }

    if (it != m_images.end()) {
        it = m_images.erase(it);
    }

    auto const inserted = m_images.insert(key, CachedMapImage{painter->addImage(source), opacity});

    return inserted.value();
}

auto CanvasResourceCache::maybeRelease(
    QCanvasPainter* painter,
    std::chrono::steady_clock::time_point lastCameraChange
) -> void {
    auto const now = std::chrono::steady_clock::now();
    if ((now - lastCameraChange) < kIdleReleaseDelay) {
        return;
    }

    if (painter->activeImageCount() <= kMaximumCachedImages
        && painter->activeImageMemoryUsage() <= kMaximumCachedImageBytes) {
        return;
    }

    painter->cleanupResources();
    m_images.clear();
    m_seen.clear();
}

auto CanvasResourceCache::reset() -> void {
    m_images.clear();
    m_seen.clear();
}
