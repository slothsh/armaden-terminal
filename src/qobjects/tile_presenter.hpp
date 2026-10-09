#pragma once

#include <QPointer>
#include <QSet>
#include <QString>

#include "canvas_resource_cache.hpp"

class MapCanvas;
class QCanvasPainter;

namespace mts::imagery {
struct LevelInfo;
struct TileId;
}

namespace mts::map {
class MapLayer;
class MapLoader;
}

namespace mts::render2d {
struct Camera;
struct RenderContext;
}

class TilePresenter {
public:
    explicit TilePresenter(CanvasResourceCache* cache);

    auto setCanvas(MapCanvas* canvas) -> void;
    auto reset() -> void;

    auto present(
        QCanvasPainter* painter,
        mts::render2d::RenderContext const& context,
        mts::render2d::Camera const& camera,
        mts::map::MapLoader const* loader
    ) -> void;

private:
    auto presentMapLayers(
        QCanvasPainter* painter,
        mts::render2d::RenderContext const& context,
        mts::render2d::Camera const& camera
    ) -> void;

    auto drawReadyTile(
        QCanvasPainter* painter,
        mts::render2d::RenderContext const& context,
        mts::render2d::Camera const& camera,
        mts::imagery::LevelInfo const& grid,
        mts::map::MapLayer* layer,
        double opacity,
        mts::imagery::TileId const& tile,
        QString const& key
    ) -> void;

    auto paintBackfill(
        QCanvasPainter* painter,
        mts::render2d::RenderContext const& context,
        mts::render2d::Camera const& camera,
        mts::map::MapLayer* layer,
        double opacity,
        mts::imagery::TileId const& tile
    ) -> void;

    auto requestTile(mts::imagery::TileId const& tile, QString const& layerId) -> void;

    CanvasResourceCache* m_cache;
    QPointer<MapCanvas> m_canvas;
    QSet<QString> m_requested;
    mts::map::MapLoader const* m_loader = nullptr;
};
