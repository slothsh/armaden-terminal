#include <QByteArray>
#include <QColor>
#include <QCryptographicHash>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QtGlobal>

export module map;

import std;
import agent;
import runtime;
import imagery;
import ecs;

export namespace mts::map {

using mts::agent::json;

enum class MapBlend : std::uint8_t {
    Normal,
    Multiply,
    Screen,
    Overlay,
};

[[nodiscard]] auto blendModeFromString(std::string const& text) -> mts::agent::Result<MapBlend> {
    if (text == "normal")
        return MapBlend::Normal;
    if (text == "multiply")
        return MapBlend::Multiply;
    if (text == "screen")
        return MapBlend::Screen;
    if (text == "overlay")
        return MapBlend::Overlay;
    return mts::agent::Result<MapBlend>::fromStatus(
        mts::agent::Status::invalidArgument("unknown blend mode: " + text));
}

[[nodiscard]] auto blendModeToString(MapBlend blend) -> std::string {
    switch (blend) {
        case MapBlend::Normal: return "normal";
        case MapBlend::Multiply: return "multiply";
        case MapBlend::Screen: return "screen";
        case MapBlend::Overlay: return "overlay";
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
            QRectF(QPointF(0.0, 0.0), QSizeF(baseSize.width(), baseSize.height())));
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

class MapLayer {
public:
    MapLayer(std::string id,
             std::string name,
             imagery::SingleFileTileSource source,
             MapBlend blend,
             double opacity,
             bool visible,
             int levelBias,
             mts::runtime::ThreadPool& pool)
        : m_id(std::move(id))
        , m_name(std::move(name))
        , m_source(std::move(source))
        , m_blend(blend)
        , m_opacity(qBound(0.0, opacity, 1.0))
        , m_visible(visible)
        , m_levelBias(levelBias)
        , m_scheduler(pool, [this](imagery::TileId const& tile) {
              return m_source.readTile(tile);
          }) {}

    MapLayer(MapLayer const&) = delete;
    MapLayer& operator=(MapLayer const&) = delete;

    [[nodiscard]] auto id() const -> std::string const& {
        return m_id;
    }

    [[nodiscard]] auto name() const -> std::string const& {
        return m_name;
    }

    [[nodiscard]] auto blend() const -> MapBlend {
        return m_blend;
    }

    [[nodiscard]] auto opacity() const -> double {
        std::lock_guard const lock(m_mutex);
        return m_opacity;
    }

    [[nodiscard]] auto visible() const -> bool {
        std::lock_guard const lock(m_mutex);
        return m_visible;
    }

    [[nodiscard]] auto levelBias() const -> int {
        return m_levelBias;
    }

    void setVisible(bool visible) {
        {
            std::lock_guard const lock(m_mutex);
            m_visible = visible;
        }
    }

    void setOpacity(double opacity) {
        std::lock_guard const lock(m_mutex);
        m_opacity = qBound(0.0, opacity, 1.0);
    }

    [[nodiscard]] auto source() const -> imagery::SingleFileTileSource const& {
        return m_source;
    }

    [[nodiscard]] auto cache() const -> imagery::TileCache const& {
        return m_cache;
    }

    void startBuild(mts::runtime::ThreadPool& pool) {
        m_source.startBuild(pool);
    }

    [[nodiscard]] auto buildProgress() const -> imagery::TileBuildProgress {
        return m_source.buildProgress();
    }

    [[nodiscard]] auto levelReady(int level) const -> bool {
        return m_source.levelReady(level);
    }

    void requestTile(imagery::TileId const& tile,
                     imagery::TileScheduler::Generation generation,
                     imagery::TileScheduler::Completion completion) {
        if (auto const entry = m_cache.find(tile); entry && entry->state == imagery::TileState::Ready) {
            mts::agent::Result<imagery::SharedImage> const result = entry->image;
            if (completion)
                completion(result);
            return;
        }

        if (m_cache.state(tile) != imagery::TileState::Queued)
            m_cache.markQueued(tile);

        m_scheduler.request(tile, generation,
            [this, tile, completion = std::move(completion)](mts::agent::Result<imagery::SharedImage> const& result) {
                if (result) {
                    m_cache.store(tile, result.value());
                } else if (result.status().code() == mts::agent::StatusCode::NotFound) {
                    m_cache.evict(tile);
                } else {
                    m_cache.markFailed(tile, result.status().message());
                }
                if (completion)
                    completion(result);
            });
    }

    void setGeneration(imagery::TileScheduler::Generation generation) {
        m_scheduler.setGeneration(generation);
    }

    void cancelPending() {
        m_scheduler.cancelPending();
    }

    [[nodiscard]] auto fetchTile(imagery::TileId const& tile) -> mts::agent::Result<imagery::SharedImage> {
        if (auto const entry = m_cache.find(tile); entry && entry->state == imagery::TileState::Ready)
            return entry->image;

        auto const decoded = m_source.readTile(tile);
        if (decoded)
            m_cache.store(tile, decoded.value());
        return decoded;
    }

private:
    std::string m_id;
    std::string m_name;
    imagery::SingleFileTileSource m_source;
    imagery::TileCache m_cache;
    imagery::TileScheduler m_scheduler;
    MapBlend m_blend;
    mutable std::mutex m_mutex;
    double m_opacity;
    bool m_visible;
    int m_levelBias;
};

struct LayerTiles {
    std::string layerId;
    bool visible = false;
    double opacity = 1.0;
    MapBlend blend = MapBlend::Normal;
    imagery::ViewportTiles viewport;
};

class MapLoader {
public:
    struct Options {
        std::filesystem::path cacheDir;
    };

    [[nodiscard]] static auto create(std::filesystem::path const& configPath,
                                     Options options,
                                     mts::runtime::ThreadPool& pool)
        -> mts::agent::Result<std::unique_ptr<MapLoader>>;

    [[nodiscard]] auto info() const -> MapInfo const& {
        return m_info;
    }

    [[nodiscard]] auto transform() const -> MapTransform const& {
        return m_info.transform;
    }

    [[nodiscard]] auto layers() const -> std::vector<MapLayer*> {
        std::lock_guard const lock(m_mutex);
        std::vector<MapLayer*> snapshot;
        snapshot.reserve(m_layers.size());
        for (auto const& layer : m_layers)
            snapshot.push_back(layer.get());
        return snapshot;
    }

    [[nodiscard]] auto layer(std::string const& id) const -> MapLayer* {
        return this->findLayer(id);
    }

    void setLayerVisible(std::string const& id, bool visible) const {
        std::lock_guard const lock(m_mutex);
        auto const it = std::find_if(m_layers.begin(), m_layers.end(),
            [&id](std::unique_ptr<MapLayer> const& layer) { return layer->id() == id; });
        if (it != m_layers.end())
            (*it)->setVisible(visible);
    }

    void setLayerOpacity(std::string const& id, double opacity) const {
        std::lock_guard const lock(m_mutex);
        auto const it = std::find_if(m_layers.begin(), m_layers.end(),
            [&id](std::unique_ptr<MapLayer> const& layer) { return layer->id() == id; });
        if (it != m_layers.end())
            (*it)->setOpacity(opacity);
    }

    void startBuild() const {
        std::lock_guard const lock(m_mutex);
        for (auto const& layer : m_layers)
            layer->startBuild(*m_pool);
    }

    [[nodiscard]] auto building() const -> bool {
        std::lock_guard const lock(m_mutex);
        return std::any_of(m_layers.begin(), m_layers.end(),
            [](std::unique_ptr<MapLayer> const& layer) {
                auto const progress = layer->buildProgress();
                return progress.building && !progress.ready && !progress.failed;
            });
    }

    [[nodiscard]] auto allReady() const -> bool {
        std::lock_guard const lock(m_mutex);
        return std::all_of(m_layers.begin(), m_layers.end(),
            [](std::unique_ptr<MapLayer> const& layer) {
                auto const progress = layer->buildProgress();
                return progress.ready || progress.failed;
            });
    }

    [[nodiscard]] auto visibleTiles(QRectF const& canvasRect,
                                    mts::ecs::Camera const& camera,
                                    double devicePixelRatio) const -> std::vector<LayerTiles> {
        if (canvasRect.isEmpty())
            return {};

        QRectF const worldRect = mts::ecs::worldBounds(camera, canvasRect).normalized();
        if (worldRect.isEmpty())
            return {};

        QRectF const baseRect = m_info.transform.metricRectToPixelRect(worldRect);
        double const worldPerPixel = std::max(
            worldRect.width() / canvasRect.width(),
            worldRect.height() / canvasRect.height()
        );
        double const zoom = worldPerPixel / m_info.transform.metersPerPixel;

        std::lock_guard const lock(m_mutex);
        std::vector<LayerTiles> result;
        result.reserve(m_layers.size());
        for (auto const& layer : m_layers) {
            imagery::ViewportRequest const request{
                baseRect,
                layer->source().baseSize(),
                canvasRect.size(),
                devicePixelRatio,
                zoom,
                m_info.tileSize,
                layer->source().levelCount() - 1,
                imagery::LodPolicy{double(layer->levelBias()), 0},
            };

            LayerTiles entry;
            entry.layerId = layer->id();
            entry.visible = layer->visible();
            entry.opacity = layer->opacity();
            entry.blend = layer->blend();
            entry.viewport = imagery::resolveViewport(request);
            result.push_back(std::move(entry));
        }
        return result;
    }

    auto requestTile(std::string const& layerId,
                     imagery::TileId const& tile,
                     imagery::TileScheduler::Generation generation,
                     imagery::TileScheduler::Completion completion) const -> mts::agent::Status {
        MapLayer* const target = this->findLayer(layerId);
        if (target == nullptr)
            return mts::agent::Status::notFound("unknown map layer: " + layerId);
        target->requestTile(tile, generation, std::move(completion));
        return mts::agent::Status::ok();
    }

    void setGeneration(imagery::TileScheduler::Generation generation) const {
        std::lock_guard const lock(m_mutex);
        for (auto const& layer : m_layers)
            layer->setGeneration(generation);
    }

    void cancelPending() const {
        std::lock_guard const lock(m_mutex);
        for (auto const& layer : m_layers)
            layer->cancelPending();
    }

private:
    explicit MapLoader(MapInfo info, std::vector<std::unique_ptr<MapLayer>> layers, mts::runtime::ThreadPool& pool)
        : m_info(std::move(info))
        , m_layers(std::move(layers))
        , m_pool(&pool) {}

    [[nodiscard]] auto findLayer(std::string const& id) const -> MapLayer* {
        std::lock_guard const lock(m_mutex);
        auto const it = std::find_if(m_layers.begin(), m_layers.end(),
            [&id](std::unique_ptr<MapLayer> const& layer) { return layer->id() == id; });
        return it == m_layers.end() ? nullptr : it->get();
    }

    MapInfo m_info;
    std::vector<std::unique_ptr<MapLayer>> m_layers;
    mts::runtime::ThreadPool* m_pool;
    mutable std::mutex m_mutex;
};

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

    explicit MapSnapshotService(MapLoader const* loader,
                                std::filesystem::path cacheDir,
                                std::size_t cacheCapacity = 128)
        : m_loader(loader)
        , m_cacheDir(std::move(cacheDir))
        , m_cacheCapacity(std::max<std::size_t>(cacheCapacity, 1)) {
        m_mapDir = m_cacheDir / "snapshots" / this->mapHash();
    }

    MapSnapshotService(MapSnapshotService const&) = delete;
    MapSnapshotService& operator=(MapSnapshotService const&) = delete;

    [[nodiscard]] auto renderRegion(QRectF const& worldRect, SnapshotOptions const& options) const
        -> mts::agent::Result<MapSnapshot> {
        if (worldRect.isEmpty() || !(worldRect.width() > 0.0) || !(worldRect.height() > 0.0))
            return mts::agent::Result<MapSnapshot>::fromStatus(
                mts::agent::Status::invalidArgument("snapshot region must have positive extent"));
        if (options.maxPixels <= 0)
            return mts::agent::Result<MapSnapshot>::fromStatus(
                mts::agent::Status::invalidArgument("snapshot maxPixels must be positive"));

        MapInfo const& info = m_loader->info();
        std::vector<MapLayer*> const layers = this->resolveLayers(options);
        if (layers.empty())
            return mts::agent::Result<MapSnapshot>::fromStatus(
                mts::agent::Status::invalidArgument("snapshot layer selection is empty"));

        QRectF const baseRect = info.transform.metricRectToPixelRect(worldRect);
        int const maxLevel = std::max(0, m_loaderMaxLevel(layers));
        int const level = options.level.value_or(autoLevel(baseRect, options.maxPixels, maxLevel));
        if (level < 0 || level > maxLevel)
            return mts::agent::Result<MapSnapshot>::fromStatus(
                mts::agent::Status::invalidArgument(
                    "snapshot level out of range: " + std::to_string(level)));

        QRectF const levelRect = imagery::baseRectToLevelRect(baseRect, level);
        QSize const levelSize(
            int(std::ceil(levelRect.width())),
            int(std::ceil(levelRect.height()))
        );
        if (std::max(levelSize.width(), levelSize.height()) > kMaximumIntermediateEdge)
            return mts::agent::Result<MapSnapshot>::fromStatus(
                mts::agent::Status::invalidArgument(
                    "snapshot region too large at level " + std::to_string(level)
                    + "; request a coarser level or a smaller region"));

        std::string const key = this->cacheKey(worldRect, level, layers, options.maxPixels);
        auto const cached = this->loadCached(key);
        if (cached)
            return MapSnapshot{key, std::move(cached.value()), worldRect, level, this->layerIds(layers), cached->size()};

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
                    if (!decoded)
                        return mts::agent::Result<MapSnapshot>::fromStatus(decoded.status());
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

    [[nodiscard]] auto snapshotDirectory() const -> std::filesystem::path const& {
        return m_mapDir;
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
                if (layer->visible())
                    visible.push_back(layer);
            }
            return visible;
        }

        std::vector<MapLayer*> selected;
        for (auto const& requested : options.layers) {
            bool const known = std::any_of(all.begin(), all.end(),
                [&requested](MapLayer* const layer) { return layer->id() == requested; });
            if (!known)
                return {};
        }
        for (MapLayer* const layer : all) {
            if (std::find(options.layers.begin(), options.layers.end(), layer->id()) != options.layers.end())
                selected.push_back(layer);
        }
        return selected;
    }

    [[nodiscard]] static auto layerIds(std::vector<MapLayer*> const& layers) -> std::vector<std::string> {
        std::vector<std::string> ids;
        ids.reserve(layers.size());
        for (MapLayer* const layer : layers)
            ids.push_back(layer->id());
        return ids;
    }

    [[nodiscard]] static auto autoLevel(QRectF const& baseRect, int maxPixels, int maxLevel) -> int {
        double const baseLongEdge = std::max(baseRect.width(), baseRect.height());
        if (!(baseLongEdge > 0.0) || maxPixels <= 0)
            return 0;
        double const zoom = baseLongEdge / double(maxPixels);
        return imagery::selectLevel(zoom, 1.0, maxLevel, imagery::LodPolicy{});
    }

    [[nodiscard]] static auto m_loaderMaxLevel(std::vector<MapLayer*> const& layers) -> int {
        int maxLevel = 0;
        for (MapLayer* const layer : layers)
            maxLevel = std::max(maxLevel, layer->source().levelCount() - 1);
        return maxLevel;
    }

    [[nodiscard]] auto cacheKey(QRectF const& worldRect, int level,
                                std::vector<MapLayer*> const& layers, int maxPixels) const -> std::string {
        auto const quantize = [](double value) {
            return std::round(value * 10000.0) / 10000.0;
        };
        std::string identity = std::to_string(quantize(worldRect.left()))
            + "|" + std::to_string(quantize(worldRect.top()))
            + "|" + std::to_string(quantize(worldRect.width()))
            + "|" + std::to_string(quantize(worldRect.height()))
            + "|" + std::to_string(level)
            + "|" + std::to_string(maxPixels);
        for (auto const& id : layerIds(layers))
            identity += "|" + id;
        return this->sha1Hex(identity);
    }

    [[nodiscard]] auto cachedPath(std::string const& key) const -> std::filesystem::path {
        return m_mapDir / (key + ".png");
    }

    [[nodiscard]] auto loadCached(std::string const& key) const -> std::optional<imagery::SharedImage> {
        std::lock_guard const lock(m_cacheMutex);
        std::error_code error;
        std::filesystem::path const path = this->cachedPath(key);
        if (!std::filesystem::exists(path, error) || error)
            return std::nullopt;
        imagery::SharedImage image(QString::fromStdString(path.string()));
        if (image.isNull())
            return std::nullopt;
        return image;
    }

    void storeCached(std::string const& key, imagery::SharedImage const& image) const {
        std::lock_guard const lock(m_cacheMutex);
        std::error_code error;
        std::filesystem::create_directories(m_mapDir, error);
        if (error)
            return;
        if (!image.save(QString::fromStdString(this->cachedPath(key).string()), "png"))
            return;
        this->evictOverCapacity();
    }

    void evictOverCapacity() const {
        std::error_code error;
        std::vector<std::filesystem::path> files;
        for (auto const& entry : std::filesystem::directory_iterator(m_mapDir, error)) {
            if (!error && entry.path().extension() == ".png")
                files.push_back(entry.path());
        }
        if (files.size() <= m_cacheCapacity)
            return;

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
        if (longEdge <= maxPixels)
            return image;
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
            case MapBlend::Multiply: return QPainter::CompositionMode_Multiply;
            case MapBlend::Screen: return QPainter::CompositionMode_Screen;
            case MapBlend::Overlay: return QPainter::CompositionMode_Overlay;
            case MapBlend::Normal: return QPainter::CompositionMode_SourceOver;
        }
        return QPainter::CompositionMode_SourceOver;
    }

    MapLoader const* m_loader;
    std::filesystem::path m_cacheDir;
    std::filesystem::path m_mapDir;
    std::size_t m_cacheCapacity;
    mutable std::mutex m_cacheMutex;
};

}

namespace mts::map::detail {

using mts::agent::json;
using mts::agent::Status;

[[nodiscard]] auto jsonDouble(json const& object, std::string_view key, double fallback, double& target) -> Status {
    if (!object.contains(key)) {
        target = fallback;
        return Status::ok();
    }
    auto const& value = object.at(key);
    if (!value.is_number())
        return Status::invalidArgument(std::string(key) + " must be a number");
    target = value.get<double>();
    return Status::ok();
}

[[nodiscard]] auto jsonString(json const& object, std::string_view key, std::string fallback, std::string& target) -> Status {
    if (!object.contains(key)) {
        target = std::move(fallback);
        return Status::ok();
    }
    auto const& value = object.at(key);
    if (!value.is_string())
        return Status::invalidArgument(std::string(key) + " must be a string");
    target = value.get<std::string>();
    return Status::ok();
}

[[nodiscard]] auto jsonBool(json const& object, std::string_view key, bool fallback, bool& target) -> Status {
    if (!object.contains(key)) {
        target = fallback;
        return Status::ok();
    }
    auto const& value = object.at(key);
    if (!value.is_boolean())
        return Status::invalidArgument(std::string(key) + " must be a boolean");
    target = value.get<bool>();
    return Status::ok();
}

[[nodiscard]] auto jsonInt(json const& object, std::string_view key, int fallback, int& target) -> Status {
    if (!object.contains(key)) {
        target = fallback;
        return Status::ok();
    }
    auto const& value = object.at(key);
    if (!value.is_number_integer())
        return Status::invalidArgument(std::string(key) + " must be an integer");
    target = value.get<int>();
    return Status::ok();
}

[[nodiscard]] auto resolveLayerPath(std::string const& path, std::filesystem::path const& configDir)
    -> std::filesystem::path {
    std::filesystem::path const resolved(path);
    if (resolved.is_absolute())
        return resolved;
    return configDir / resolved;
}

}

export namespace mts::map {

auto MapLoader::create(std::filesystem::path const& configPath,
                       Options options,
                       mts::runtime::ThreadPool& pool)
    -> mts::agent::Result<std::unique_ptr<MapLoader>> {
    std::ifstream stream(configPath);
    if (!stream.is_open())
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
            mts::agent::Status::invalidArgument("could not read map config: " + configPath.string()));

    std::string const text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    json const config = json::parse(text, nullptr, false);
    if (config.is_discarded() || !config.is_object())
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
            mts::agent::Status::invalidArgument("map config is not valid JSON: " + configPath.string()));

    std::filesystem::path const configDir = configPath.parent_path();
    std::string const stem = configPath.stem().string();

    MapInfo info;
    if (auto const status = detail::jsonString(config, "id", stem, info.id); !status)
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
    if (auto const status = detail::jsonString(config, "name", info.id, info.name); !status)
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
    if (auto const status = detail::jsonInt(config, "tileSize", 256, info.tileSize); !status)
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
    if (info.tileSize <= 0)
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
            mts::agent::Status::invalidArgument("map config tileSize must be positive"));

    MapTransform transform;
    if (auto const status = detail::jsonDouble(config, "metersPerPixel", 1.0, transform.metersPerPixel); !status)
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
    if (transform.metersPerPixel <= 0.0)
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
            mts::agent::Status::invalidArgument("map config metersPerPixel must be positive"));
    if (auto const status = detail::jsonDouble(config, "originX", 0.0, transform.originX); !status)
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
    if (auto const status = detail::jsonDouble(config, "originY", 0.0, transform.originY); !status)
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
    if (auto const status = detail::jsonBool(config, "flipY", true, transform.flipY); !status)
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
    if (auto const status = detail::jsonString(config, "unitLabel", "m", transform.unitLabel); !status)
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
    info.transform = transform;

    struct LayerSpec {
        std::string id;
        std::string name;
        std::string path;
        MapBlend blend = MapBlend::Normal;
        double opacity = 1.0;
        bool visible = true;
        int levelBias = 0;
    };

    std::vector<LayerSpec> specs;
    if (config.contains("layers")) {
        auto const& layers = config.at("layers");
        if (!layers.is_array())
            return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
                mts::agent::Status::invalidArgument("map config layers must be an array"));

        for (auto const& entry : layers) {
            if (!entry.is_object())
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
                    mts::agent::Status::invalidArgument("map config layer entries must be objects"));

            LayerSpec spec;
            if (auto const status = detail::jsonString(entry, "id", {}, spec.id); !status)
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
            if (spec.id.empty())
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
                    mts::agent::Status::invalidArgument("map config layer id is required"));
            if (auto const status = detail::jsonString(entry, "name", spec.id, spec.name); !status)
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
            if (auto const status = detail::jsonString(entry, "path", {}, spec.path); !status)
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
            if (spec.path.empty())
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
                    mts::agent::Status::invalidArgument("map config layer path is required: " + spec.id));

            std::string blendText;
            if (auto const status = detail::jsonString(entry, "blendMode", "normal", blendText); !status)
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
            auto const blend = blendModeFromString(blendText);
            if (!blend)
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(blend.status());
            spec.blend = blend.value();

            if (auto const status = detail::jsonDouble(entry, "opacity", 1.0, spec.opacity); !status)
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
            if (auto const status = detail::jsonBool(entry, "defaultVisible", true, spec.visible); !status)
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
            if (auto const status = detail::jsonInt(entry, "levelBias", 0, spec.levelBias); !status)
                return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);

            specs.push_back(std::move(spec));
        }
    } else {
        LayerSpec spec;
        spec.id = "base";
        spec.name = info.name;
        if (auto const status = detail::jsonString(config, "path", {}, spec.path); !status)
            return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(status);
        if (spec.path.empty())
            return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
                mts::agent::Status::invalidArgument("map config requires a path or a layers array"));
        specs.push_back(std::move(spec));
    }

    std::vector<std::unique_ptr<MapLayer>> layers;
    layers.reserve(specs.size());
    for (auto const& spec : specs) {
        auto const resolved = detail::resolveLayerPath(spec.path, configDir);
        auto const source = imagery::SingleFileTileSource::create(
            resolved, imagery::SingleFileTileSource::Options{info.tileSize, options.cacheDir});
        if (!source)
            return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
                mts::agent::Status::invalidArgument(
                    "layer '" + spec.id + "': " + source.status().message()));

        layers.push_back(std::make_unique<MapLayer>(
            spec.id, spec.name, std::move(source.value()), spec.blend,
            spec.opacity, spec.visible, spec.levelBias, pool));
    }
    if (layers.empty())
        return mts::agent::Result<std::unique_ptr<MapLoader>>::fromStatus(
            mts::agent::Status::invalidArgument("map config defines no layers"));

    info.baseSize = layers.front()->source().baseSize();
    info.worldBounds = info.transform.worldRect(info.baseSize);

    int maxLevel = 0;
    for (auto const& layer : layers)
        maxLevel = std::max(maxLevel, layer->source().levelCount() - 1);
    info.maxLevel = maxLevel;

    return std::unique_ptr<MapLoader>(new MapLoader(std::move(info), std::move(layers), pool));
}

}
