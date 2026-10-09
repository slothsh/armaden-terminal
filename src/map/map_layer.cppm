#include <QtGlobal>

export module map:layer;

import std;
import foundation;
import runtime;
import imagery;

import :transform;

export namespace mts::map {

class MapLayer {
public:
    MapLayer(
        std::string id,
        std::string name,
        imagery::AnyTileSource source,
        MapBlend blend,
        double opacity,
        bool visible,
        int levelBias,
        mts::runtime::ThreadPool& pool
    )
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
    auto operator=(MapLayer const&) -> MapLayer& = delete;

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

    [[nodiscard]] auto source() const -> imagery::AnyTileSource const& {
        return m_source;
    }

    [[nodiscard]] auto cache() const -> imagery::TileCache const& {
        return m_cache;
    }

    auto startBuild(mts::runtime::ThreadPool& pool) -> void {
        m_source.startBuild(pool);
    }

    [[nodiscard]] auto levelReady(int level) const -> bool {
        return m_source.levelReady(level);
    }

    auto requestTile(
        imagery::TileId const& tile,
        imagery::TileScheduler::Completion completion
    ) -> void {
        if (auto const entry = m_cache.find(tile); entry && entry->state == imagery::TileState::Ready) {
            mts::foundation::Result<imagery::SharedImage> const result = entry->image;
            if (completion) {
                completion(result);
            }

            return;
        }

        if (m_cache.state(tile) != imagery::TileState::Queued) {
            m_cache.markQueued(tile);
        }

        m_scheduler.request(tile,
            [this, tile, completion = std::move(completion)](mts::foundation::Result<imagery::SharedImage> const& result) {
                if (result) {
                    m_cache.store(tile, result.value());
                } else if (result.error().kind<MapErrorKind>() == MapErrorKind::NotFound) {
                    m_cache.evict(tile);
                } else {
                    m_cache.markFailed(tile, std::string(result.error().message()));
                }

                if (completion) {
                    completion(result);
                }
            });
    }

    [[nodiscard]] auto fetchTile(imagery::TileId const& tile) -> mts::foundation::Result<imagery::SharedImage> {
        if (auto const entry = m_cache.find(tile); entry && entry->state == imagery::TileState::Ready) {
            return entry->image;
        }

        auto const decoded = m_source.readTile(tile);
        if (decoded) {
            m_cache.store(tile, decoded.value());
        }

        return decoded;
    }

private:
    std::string m_id;
    std::string m_name;
    imagery::AnyTileSource m_source;
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

} // END OF NAMESPACE `mts::map`
