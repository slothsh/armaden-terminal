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
                if (result)
                    m_cache.store(tile, result.value());
                else
                    m_cache.markFailed(tile, result.status().message());
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

    [[nodiscard]] auto layers() const -> std::vector<MapLayer const*> {
        std::lock_guard const lock(m_mutex);
        std::vector<MapLayer const*> snapshot;
        snapshot.reserve(m_layers.size());
        for (auto const& layer : m_layers)
            snapshot.push_back(layer.get());
        return snapshot;
    }

    [[nodiscard]] auto layer(std::string const& id) const -> MapLayer const* {
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
