#include <QRectF>
#include <QSize>
#include <fstream>

export module map:loader;

import std;
import foundation;
import json;
import runtime;
import imagery;
import ecs;

import :transform;
import :layer;

export namespace mts::map {

class MapLoader {
public:
    struct Options {
        std::filesystem::path cacheDir;
    };

    [[nodiscard]] static auto create(
        std::filesystem::path const& configPath,
        Options options,
        mts::runtime::ThreadPool& pool
    ) -> mts::foundation::Result<std::unique_ptr<MapLoader>>;

    [[nodiscard]] auto info() const -> MapInfo const& {
        return m_info;
    }

    [[nodiscard]] auto layers() const -> std::vector<MapLayer*> {
        std::lock_guard const lock(m_mutex);
        std::vector<MapLayer*> snapshot;
        snapshot.reserve(m_layers.size());
        for (auto const& layer : m_layers) {
            snapshot.push_back(layer.get());
        }

        return snapshot;
    }

    [[nodiscard]] auto layer(std::string const& id) const -> MapLayer* {
        return this->findLayer(id);
    }

    auto startBuild() const -> void {
        std::lock_guard const lock(m_mutex);
        for (auto const& layer : m_layers) {
            layer->startBuild(*m_pool);
        }
    }

    [[nodiscard]] auto visibleTiles(
        QRectF const& canvasRect,
        mts::ecs::Camera const& camera,
        double devicePixelRatio
    ) const -> std::vector<LayerTiles> {
        if (canvasRect.isEmpty()) {
            return {};
        }

        QRectF const worldRect = mts::ecs::worldBounds(camera, canvasRect).normalized();
        if (worldRect.isEmpty()) {
            return {};
        }

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

    auto requestTile(
        std::string const& layerId,
        imagery::TileId const& tile,
        imagery::TileScheduler::Completion completion
    ) const -> mts::foundation::Status {
        MapLayer* const target = this->findLayer(layerId);
        if (target == nullptr) {
            return mts::foundation::Status::err(MapErrorKind::NotFound, "unknown map layer: " + layerId);
        }

        target->requestTile(tile, std::move(completion));

        return mts::foundation::Status::ok();
    }

private:
    explicit MapLoader(MapInfo info, std::vector<std::unique_ptr<MapLayer>> layers, mts::runtime::ThreadPool& pool)
        : m_info(std::move(info))
        , m_layers(std::move(layers))
        , m_pool(&pool) {}

    [[nodiscard]] auto findLayer(std::string const& id) const -> MapLayer* {
        std::lock_guard const lock(m_mutex);
        auto const it = std::find_if(
            m_layers.begin(),
            m_layers.end(),
            [&id](std::unique_ptr<MapLayer> const& layer) { return layer->id() == id; }
        );

        return it == m_layers.end() ? nullptr : it->get();
    }

    MapInfo m_info;
    std::vector<std::unique_ptr<MapLayer>> m_layers;
    mts::runtime::ThreadPool* m_pool;
    mutable std::mutex m_mutex;
};

} // END OF NAMESPACE `mts::map`

namespace mts::map::detail {

using mts::json;
using mts::foundation::Status;

[[nodiscard]] auto jsonDouble(json const& object, std::string_view key, double fallback, double& target) -> Status {
    if (!object.contains(key)) {
        target = fallback;
        return Status::ok();
    }

    auto const& value = object.at(key);
    if (!value.is_number()) {
        return Status::err(MapErrorKind::InvalidArgument, std::string(key) + " must be a number");
    }

    target = value.get<double>();

    return Status::ok();
}

[[nodiscard]] auto jsonString(json const& object, std::string_view key, std::string fallback, std::string& target) -> Status {
    if (!object.contains(key)) {
        target = std::move(fallback);
        return Status::ok();
    }

    auto const& value = object.at(key);
    if (!value.is_string()) {
        return Status::err(MapErrorKind::InvalidArgument, std::string(key) + " must be a string");
    }

    target = value.get<std::string>();

    return Status::ok();
}

[[nodiscard]] auto jsonBool(json const& object, std::string_view key, bool fallback, bool& target) -> Status {
    if (!object.contains(key)) {
        target = fallback;
        return Status::ok();
    }

    auto const& value = object.at(key);
    if (!value.is_boolean()) {
        return Status::err(MapErrorKind::InvalidArgument, std::string(key) + " must be a boolean");
    }

    target = value.get<bool>();

    return Status::ok();
}

[[nodiscard]] auto jsonInt(json const& object, std::string_view key, int fallback, int& target) -> Status {
    if (!object.contains(key)) {
        target = fallback;
        return Status::ok();
    }

    auto const& value = object.at(key);
    if (!value.is_number_integer()) {
        return Status::err(MapErrorKind::InvalidArgument, std::string(key) + " must be an integer");
    }

    target = value.get<int>();

    return Status::ok();
}

[[nodiscard]] auto resolveLayerPath(
    std::string const& path,
    std::filesystem::path const& configDir
) -> std::filesystem::path {
    std::filesystem::path const resolved(path);
    if (resolved.is_absolute()) {
        return resolved;
    }

    return configDir / resolved;
}

} // END OF NAMESPACE `mts::map::detail`

export namespace mts::map {

auto MapLoader::create(
    std::filesystem::path const& configPath,
    Options options,
    mts::runtime::ThreadPool& pool
) -> mts::foundation::Result<std::unique_ptr<MapLoader>> {
    std::ifstream stream(configPath);
    if (!stream.is_open()) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "could not read map config: " + configPath.string()));
    }

    std::string const text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    json const config = json::parse(text, nullptr, false);
    if (config.is_discarded() || !config.is_object()) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "map config is not valid JSON: " + configPath.string()));
    }

    std::filesystem::path const configDir = configPath.parent_path();
    std::string const stem = configPath.stem().string();

    MapInfo info;
    if (auto const status = detail::jsonString(config, "id", stem, info.id); !status) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
    }

    if (auto const status = detail::jsonString(config, "name", info.id, info.name); !status) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
    }

    if (auto const status = detail::jsonInt(config, "tileSize", 256, info.tileSize); !status) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
    }

    if (info.tileSize <= 0) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "map config tileSize must be positive"));
    }

    MapTransform transform;
    if (auto const status = detail::jsonDouble(config, "metersPerPixel", 1.0, transform.metersPerPixel); !status) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
    }

    if (transform.metersPerPixel <= 0.0) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "map config metersPerPixel must be positive"));
    }

    if (auto const status = detail::jsonDouble(config, "originX", 0.0, transform.originX); !status) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
    }

    if (auto const status = detail::jsonDouble(config, "originY", 0.0, transform.originY); !status) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
    }

    if (auto const status = detail::jsonBool(config, "flipY", true, transform.flipY); !status) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
    }

    if (auto const status = detail::jsonString(config, "unitLabel", "m", transform.unitLabel); !status) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
    }

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
        if (!layers.is_array()) {
            return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "map config layers must be an array"));
        }

        for (auto const& entry : layers) {
            if (!entry.is_object()) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "map config layer entries must be objects"));
            }

            LayerSpec spec;
            if (auto const status = detail::jsonString(entry, "id", {}, spec.id); !status) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
            }

            if (spec.id.empty()) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "map config layer id is required"));
            }

            if (auto const status = detail::jsonString(entry, "name", spec.id, spec.name); !status) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
            }

            if (auto const status = detail::jsonString(entry, "path", {}, spec.path); !status) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
            }

            if (spec.path.empty()) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "map config layer path is required: " + spec.id));
            }

            std::string blendText;
            if (auto const status = detail::jsonString(entry, "blendMode", "normal", blendText); !status) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
            }

            auto const blend = blendModeFromString(blendText);
            if (!blend) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(blend.error());
            }

            spec.blend = blend.value();

            if (auto const status = detail::jsonDouble(entry, "opacity", 1.0, spec.opacity); !status) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
            }

            if (auto const status = detail::jsonBool(entry, "defaultVisible", true, spec.visible); !status) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
            }

            if (auto const status = detail::jsonInt(entry, "levelBias", 0, spec.levelBias); !status) {
                return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
            }

            specs.push_back(std::move(spec));
        }
    } else {
        LayerSpec spec;
        spec.id = "base";
        spec.name = info.name;
        if (auto const status = detail::jsonString(config, "path", {}, spec.path); !status) {
            return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(status.error());
        }

        if (spec.path.empty()) {
            return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "map config requires a path or a layers array"));
        }

        specs.push_back(std::move(spec));
    }

    std::vector<std::unique_ptr<MapLayer>> layers;
    layers.reserve(specs.size());
    for (auto const& spec : specs) {
        auto const resolved = detail::resolveLayerPath(spec.path, configDir);
        auto const source = imagery::SingleFileTileSource::create(
            resolved,
            imagery::SingleFileTileSource::Options{info.tileSize, options.cacheDir}
        );
        if (!source) {
            return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(
                mts::foundation::Error(
                    MapErrorKind::InvalidArgument,
                    "layer '" + spec.id + "': " + source.error().message()
                )
            );
        }

        layers.push_back(std::make_unique<MapLayer>(
            spec.id,
            spec.name,
            std::move(source.value()),
            spec.blend,
            spec.opacity,
            spec.visible,
            spec.levelBias,
            pool
        ));
    }

    if (layers.empty()) {
        return mts::foundation::Result<std::unique_ptr<MapLoader>>::err(mts::foundation::Error(MapErrorKind::InvalidArgument, "map config defines no layers"));
    }

    info.baseSize = layers.front()->source().baseSize();
    info.worldBounds = info.transform.worldRect(info.baseSize);

    int maxLevel = 0;
    for (auto const& layer : layers) {
        maxLevel = std::max(maxLevel, layer->source().levelCount() - 1);
    }
    info.maxLevel = maxLevel;

    return std::unique_ptr<MapLoader>(new MapLoader(std::move(info), std::move(layers), pool));
}

} // END OF NAMESPACE `mts::map`
