#include <QByteArray>
#include <QCryptographicHash>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QSize>

export module map:snapshot;

import std;
import foundation;
import runtime;
import imagery;

import :transform;
import :layer;
import :loader;

export namespace mts::map {

struct SnapshotOptions {
    std::optional<int> level;
    std::vector<std::string> layers;
    int maxPixels = 1024;
    QColor background = QColor(24, 24, 24);
};

struct MapSnapshot {
    std::string id;
    imagery::SharedImage image;
    QRectF worldRect;
    int level = 0;
    std::vector<std::string> layers;
    QSize pixelSize;
};

class MapSnapshotService {
public:
    static constexpr int kMaximumIntermediateEdge = 16384;

    explicit MapSnapshotService(
        MapLoader const* loader,
        std::filesystem::path cacheDir,
        std::size_t cacheCapacity = 128
    )
        : m_loader(loader)
        , m_cacheDir(std::move(cacheDir))
        , m_cacheCapacity(std::max<std::size_t>(cacheCapacity, 1)) {
        m_mapDir = m_cacheDir / "snapshots" / this->mapHash();
    }

    MapSnapshotService(MapSnapshotService const&) = delete;
    auto operator=(MapSnapshotService const&) -> MapSnapshotService& = delete;

    [[nodiscard]] auto renderRegion(
        QRectF const& worldRect,
        SnapshotOptions const& options
    ) const -> mts::foundation::Result<MapSnapshot> {
        if (worldRect.isEmpty() || !(worldRect.width() > 0.0) || !(worldRect.height() > 0.0)) {
            return mts::foundation::Result<MapSnapshot>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "snapshot region must have positive extent"));
        }

        if (options.maxPixels <= 0) {
            return mts::foundation::Result<MapSnapshot>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "snapshot maxPixels must be positive"));
        }

        MapInfo const& info = m_loader->info();
        std::vector<MapLayer*> const layers = this->resolveLayers(options);
        if (layers.empty()) {
            return mts::foundation::Result<MapSnapshot>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "snapshot layer selection is empty"));
        }

        QRectF const baseRect = info.transform.metricRectToPixelRect(worldRect);
        int const maxLevel = std::max(0, m_loaderMaxLevel(layers));
        int const level = options.level.value_or(autoLevel(baseRect, options.maxPixels, maxLevel));
        if (level < 0 || level > maxLevel) {
            return mts::foundation::Result<MapSnapshot>::err(
                mts::foundation::Error(
                    MapErrorKind::InvalidArgument,
                    "snapshot level out of range: " + std::to_string(level)
                )
            );
        }

        QRectF const levelRect = imagery::baseRectToLevelRect(baseRect, level);
        QSize const levelSize(
            int(std::ceil(levelRect.width())),
            int(std::ceil(levelRect.height()))
        );
        if (std::max(levelSize.width(), levelSize.height()) > kMaximumIntermediateEdge) {
            return mts::foundation::Result<MapSnapshot>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, 
                    "snapshot region too large at level " + std::to_string(level)
                    + "; request a coarser level or a smaller region"));
        }

        std::string const key = this->cacheKey(worldRect, level, layers, options.maxPixels);
        auto const cached = this->loadCached(key);
        if (cached) {
            return MapSnapshot{key, std::move(cached.value()), worldRect, level, this->layerIds(layers), cached->size()};
        }

        imagery::LevelInfo const grid = imagery::levelInfoForBase(info.baseSize, info.tileSize, level);
        auto const tileIds = imagery::tilesForRect(grid, levelRect);

        QImage canvas(levelSize, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(options.background);
        {
            QPainter painter(&canvas);
            for (MapLayer* const layer : layers) {
                painter.setOpacity(layer->opacity());
                painter.setCompositionMode(this->compositionMode(layer->blend()));

                for (auto const& tile : tileIds) {
                    auto const decoded = layer->fetchTile(tile);
                    if (!decoded) {
                        return mts::foundation::Result<MapSnapshot>::err(decoded.error());
                    }

                    QPointF const origin = imagery::tileOrigin(grid, tile) - levelRect.topLeft();
                    painter.drawImage(origin, decoded.value());
                }
            }
        }

        QImage const oriented = info.transform.flipY ? canvas : canvas.flipped(Qt::Orientation::Vertical);
        QImage const output = this->clampToMaxPixels(oriented, options.maxPixels);
        this->storeCached(key, output);

        MapSnapshot snapshot;
        snapshot.id = key;
        snapshot.image = output;
        snapshot.worldRect = worldRect;
        snapshot.level = level;
        snapshot.layers = this->layerIds(layers);
        snapshot.pixelSize = output.size();

        return snapshot;
    }

private:
    [[nodiscard]] auto mapHash() const -> std::string {
        MapInfo const& info = m_loader->info();
        std::string const identity = info.id + "|" + info.name + "|" + std::to_string(info.tileSize)
            + "|" + std::to_string(info.baseSize.width()) + "x" + std::to_string(info.baseSize.height())
            + "|" + std::to_string(info.transform.metersPerPixel)
            + "|" + std::to_string(info.transform.originX) + "|" + std::to_string(info.transform.originY)
            + "|" + (info.transform.flipY ? "1" : "0");

        return this->sha1Hex(identity);
    }

    [[nodiscard]] auto sha1Hex(std::string const& text) const -> std::string {
        return QCryptographicHash::hash(QByteArray::fromStdString(text), QCryptographicHash::Sha1).toHex().toStdString();
    }

    [[nodiscard]] auto resolveLayers(SnapshotOptions const& options) const -> std::vector<MapLayer*> {
        std::vector<MapLayer*> const all = m_loader->layers();
        if (options.layers.empty()) {
            std::vector<MapLayer*> visible;
            visible.reserve(all.size());
            for (MapLayer* const layer : all) {
                if (layer->visible()) {
                    visible.push_back(layer);
                }
            }

            return visible;
        }

        std::vector<MapLayer*> selected;
        for (auto const& requested : options.layers) {
            bool const known = std::any_of(
                all.begin(),
                all.end(),
                [&requested](MapLayer* const layer) { return layer->id() == requested; }
            );
            if (!known) {
                return {};
            }
        }

        for (MapLayer* const layer : all) {
            if (std::find(options.layers.begin(), options.layers.end(), layer->id()) != options.layers.end()) {
                selected.push_back(layer);
            }
        }

        return selected;
    }

    [[nodiscard]] static auto layerIds(std::vector<MapLayer*> const& layers) -> std::vector<std::string> {
        std::vector<std::string> ids;
        ids.reserve(layers.size());
        for (MapLayer* const layer : layers) {
            ids.push_back(layer->id());
        }

        return ids;
    }

    [[nodiscard]] static auto autoLevel(QRectF const& baseRect, int maxPixels, int maxLevel) -> int {
        double const baseLongEdge = std::max(baseRect.width(), baseRect.height());
        if (!(baseLongEdge > 0.0) || maxPixels <= 0) {
            return 0;
        }

        double const zoom = baseLongEdge / double(maxPixels);

        return imagery::selectLevel(zoom, 1.0, maxLevel, imagery::LodPolicy{});
    }

    [[nodiscard]] static auto m_loaderMaxLevel(std::vector<MapLayer*> const& layers) -> int {
        int maxLevel = 0;
        for (MapLayer* const layer : layers) {
            maxLevel = std::max(maxLevel, layer->source().levelCount() - 1);
        }

        return maxLevel;
    }

    [[nodiscard]] auto cacheKey(
        QRectF const& worldRect,
        int level,
        std::vector<MapLayer*> const& layers,
        int maxPixels
    ) const -> std::string {
        auto const quantize = [](double value) -> double {
            return std::round(value * 10000.0) / 10000.0;
        };

        std::string identity = std::to_string(quantize(worldRect.left()))
            + "|" + std::to_string(quantize(worldRect.top()))
            + "|" + std::to_string(quantize(worldRect.width()))
            + "|" + std::to_string(quantize(worldRect.height()))
            + "|" + std::to_string(level)
            + "|" + std::to_string(maxPixels);
        for (auto const& id : layerIds(layers)) {
            identity += "|" + id;
        }

        return this->sha1Hex(identity);
    }

    [[nodiscard]] auto cachedPath(std::string const& key) const -> std::filesystem::path {
        return m_mapDir / (key + ".png");
    }

    [[nodiscard]] auto loadCached(std::string const& key) const -> std::optional<imagery::SharedImage> {
        std::lock_guard const lock(m_cacheMutex);
        std::error_code error;
        std::filesystem::path const path = this->cachedPath(key);
        if (!std::filesystem::exists(path, error) || error) {
            return std::nullopt;
        }

        imagery::SharedImage image(QString::fromStdString(path.string()));
        if (image.isNull()) {
            return std::nullopt;
        }

        return image;
    }

    auto storeCached(std::string const& key, imagery::SharedImage const& image) const -> void {
        std::lock_guard const lock(m_cacheMutex);
        std::error_code error;
        std::filesystem::create_directories(m_mapDir, error);
        if (error) {
            return;
        }

        if (!image.save(QString::fromStdString(this->cachedPath(key).string()), "png")) {
            return;
        }

        this->evictOverCapacity();
    }

    auto evictOverCapacity() const -> void {
        std::error_code error;
        std::vector<std::filesystem::path> files;
        for (auto const& entry : std::filesystem::directory_iterator(m_mapDir, error)) {
            if (!error && entry.path().extension() == ".png") {
                files.push_back(entry.path());
            }
        }

        if (files.size() <= m_cacheCapacity) {
            return;
        }

        std::sort(files.begin(), files.end(), [](std::filesystem::path const& a, std::filesystem::path const& b) {
            std::error_code timeError;
            auto const timeA = std::filesystem::last_write_time(a, timeError);
            auto const timeB = std::filesystem::last_write_time(b, timeError);

            return timeA < timeB;
        });

        while (files.size() > m_cacheCapacity) {
            std::filesystem::remove(files.front(), error);
            files.erase(files.begin());
        }
    }

    [[nodiscard]] static auto clampToMaxPixels(imagery::SharedImage const& image, int maxPixels) -> imagery::SharedImage {
        int const longEdge = std::max(image.width(), image.height());
        if (longEdge <= maxPixels) {
            return image;
        }

        double const scale = double(maxPixels) / double(longEdge);

        return image.scaled(
            int(std::round(double(image.width()) * scale)),
            int(std::round(double(image.height()) * scale)),
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation
        );
    }

    [[nodiscard]] static auto compositionMode(MapBlend blend) -> QPainter::CompositionMode {
        switch (blend) {
            case MapBlend::Multiply:
                return QPainter::CompositionMode_Multiply;

            case MapBlend::Screen:
                return QPainter::CompositionMode_Screen;

            case MapBlend::Overlay:
                return QPainter::CompositionMode_Overlay;

            case MapBlend::Normal:
                return QPainter::CompositionMode_SourceOver;
        }

        return QPainter::CompositionMode_SourceOver;
    }

    MapLoader const* m_loader;
    std::filesystem::path m_cacheDir;
    std::filesystem::path m_mapDir;
    std::size_t m_cacheCapacity;
    mutable std::mutex m_cacheMutex;
};

} // END OF NAMESPACE `mts::map`
