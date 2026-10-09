#include "tile_presenter.hpp"

#include "map_canvas.hpp"

#include <QCanvasPainter>
#include <QImage>
#include <QMetaObject>
#include <QPointF>
#include <QRectF>
#include <QtGlobal>

#include <cmath>

import std;
import core;
import render2d;
import imagery;
import map;

namespace {

constexpr int kMaximumBackfillUp = 16;
constexpr double kOpacityEpsilon = 0.001;

auto tileKey(QString const& layerId, mts::imagery::TileId const& tile) -> QString {
    return layerId + "/" + QString::number(tile.level) + "/" + QString::number(tile.x) + "/"
        + QString::number(tile.y);
}

auto prepareTileForLayer(QImage const& image, double opacity) -> QImage {
    if (opacity >= 0.999) {
        return image;
    }

    QImage prepared = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    double const alphaScale = qBound(0.0, opacity, 1.0);

    for (int y = 0; y < prepared.height(); ++y) {
        auto* line = reinterpret_cast<quint32*>(prepared.scanLine(y));

        for (int x = 0; x < prepared.width(); ++x) {
            quint32 const value = line[x];
            int const alpha = int(std::lround(double(qAlpha(value)) * alphaScale));
            int const red = int(std::lround(double(qRed(value)) * alphaScale));
            int const green = int(std::lround(double(qGreen(value)) * alphaScale));
            int const blue = int(std::lround(double(qBlue(value)) * alphaScale));
            line[x] = qPremultiply(qRgba(red, green, blue, alpha));
        }
    }

    return prepared;
}

} // END OF NAMESPACE anonymous

TilePresenter::TilePresenter(CanvasResourceCache* cache)
    : m_cache(cache) {}

auto TilePresenter::setCanvas(MapCanvas* canvas) -> void {
    m_canvas = canvas;
}

auto TilePresenter::reset() -> void {
    m_requested.clear();
    m_loader = nullptr;
}

auto TilePresenter::present(
    QCanvasPainter* painter,
    mts::render2d::RenderContext const& context,
    mts::render2d::Camera const& camera,
    mts::map::MapLoader const* loader
) -> void {
    m_loader = loader;
    if (m_loader == nullptr) {
        m_requested.clear();

        return;
    }

    m_cache->beginFrame();
    this->presentMapLayers(painter, context, camera);
    m_cache->endFrame();
    m_requested.clear();
}

auto TilePresenter::presentMapLayers(
    QCanvasPainter* painter,
    mts::render2d::RenderContext const& context,
    mts::render2d::Camera const& camera
) -> void {
    mts::map::MapLoader const* loader = m_loader;

    auto const layerTiles = loader->visibleTiles(context.canvasRect, camera, double(context.devicePixelRatio));
    for (auto const& entry : layerTiles) {
        if (!entry.visible) {
            continue;
        }

        mts::map::MapLayer* const layer = loader->layer(entry.layerId);
        if (layer == nullptr) {
            continue;
        }

        mts::imagery::LevelInfo const& grid = entry.viewport.levelInfo;
        double const opacity = layer->opacity();

        for (auto const& tile : entry.viewport.tiles) {
            QString const key = tileKey(QString::fromStdString(entry.layerId), tile);
            auto const state = layer->cache().state(tile);
            if (state == mts::imagery::TileState::Ready) {
                this->drawReadyTile(painter, context, camera, grid, layer, opacity, tile, key);
            } else {
                if (state == mts::imagery::TileState::Missing && layer->levelReady(tile.level)) {
                    this->requestTile(tile, QString::fromStdString(entry.layerId));
                }

                this->paintBackfill(painter, context, camera, layer, opacity, tile);
            }
        }
    }
}

auto TilePresenter::drawReadyTile(
    QCanvasPainter* painter,
    mts::render2d::RenderContext const& context,
    mts::render2d::Camera const& camera,
    mts::imagery::LevelInfo const& grid,
    mts::map::MapLayer* layer,
    double opacity,
    mts::imagery::TileId const& tile,
    QString const& key
) -> void {
    auto const* slot = m_cache->find(key);
    if (slot == nullptr || std::abs(slot->opacity - opacity) > kOpacityEpsilon) {
        auto const entry = layer->cache().find(tile);
        if (!entry || entry->state != mts::imagery::TileState::Ready) {
            return;
        }

        slot = &m_cache->insert(painter, key, prepareTileForLayer(entry->image, opacity), opacity);
    }

    m_cache->markSeen(key);

    mts::map::MapInfo const& info = m_loader->info();
    QRectF const tileBase = mts::imagery::levelRectToBaseRect(mts::imagery::tileRect(grid, tile), tile.level);
    QRectF const tileWorld = info.transform.pixelRectToMetricRect(tileBase);
    QRectF const destination = mts::render2d::screenRectForWorldRect(camera, context.canvasRect.size(), tileWorld);
    painter->drawImage(slot->image, destination);
}

auto TilePresenter::paintBackfill(
    QCanvasPainter* painter,
    mts::render2d::RenderContext const& context,
    mts::render2d::Camera const& camera,
    mts::map::MapLayer* layer,
    double opacity,
    mts::imagery::TileId const& tile
) -> void {
    mts::map::MapInfo const& info = m_loader->info();
    int const maxLevel = layer->source().levelCount() - 1;
    int const maxUp = std::min(kMaximumBackfillUp, maxLevel - tile.level);

    for (int up = 1; up <= maxUp; ++up) {
        int const factor = 1 << up;
        mts::imagery::TileId const ancestor{tile.level + up, tile.x / factor, tile.y / factor};
        if (!layer->levelReady(ancestor.level)) {
            continue;
        }

        auto const cached = layer->cache().find(ancestor);
        if (!cached || cached->state != mts::imagery::TileState::Ready) {
            if (layer->cache().state(ancestor) == mts::imagery::TileState::Missing) {
                this->requestTile(ancestor, QString::fromStdString(layer->id()));
            }

            continue;
        }

        mts::imagery::LevelInfo const ancestorGrid =
            mts::imagery::levelInfoForBase(info.baseSize, info.tileSize, ancestor.level);
        QRectF const childBase = mts::imagery::levelRectToBaseRect(
            mts::imagery::tileRect(
                mts::imagery::levelInfoForBase(info.baseSize, info.tileSize, tile.level),
                tile
            ),
            tile.level
        );
        QRectF const ancestorBase =
            mts::imagery::levelRectToBaseRect(mts::imagery::tileRect(ancestorGrid, ancestor), ancestor.level);

        QString const key = tileKey(QString::fromStdString(layer->id()), ancestor) + "#backfill";
        auto const* slot = m_cache->find(key);
        if (slot == nullptr || std::abs(slot->opacity - opacity) > kOpacityEpsilon) {
            slot = &m_cache->insert(painter, key, prepareTileForLayer(cached->image, opacity), opacity);
        }

        m_cache->markSeen(key);

        QRectF const ancestorWorld = info.transform.pixelRectToMetricRect(ancestorBase);
        QRectF const childWorld = info.transform.pixelRectToMetricRect(childBase);
        QRectF const ancestorScreen =
            mts::render2d::screenRectForWorldRect(camera, context.canvasRect.size(), ancestorWorld);
        QRectF const childScreen =
            mts::render2d::screenRectForWorldRect(camera, context.canvasRect.size(), childWorld);

        qreal const relativeLeft = (childBase.left() - ancestorBase.left()) / qMax(ancestorBase.width(), 1.0);
        qreal const relativeTop = (childBase.top() - ancestorBase.top()) / qMax(ancestorBase.height(), 1.0);
        qreal const relativeWidth = childBase.width() / qMax(ancestorBase.width(), 1.0);
        qreal const relativeHeight = childBase.height() / qMax(ancestorBase.height(), 1.0);
        QRectF const sourceRect(
            relativeLeft * cached->image.width(),
            relativeTop * cached->image.height(),
            relativeWidth * cached->image.width(),
            relativeHeight * cached->image.height()
        );

        painter->drawImage(slot->image, sourceRect, childScreen);

        return;
    }
}

auto TilePresenter::requestTile(mts::imagery::TileId const& tile, QString const& layerId) -> void {
    QString const key = tileKey(layerId, tile);
    if (m_requested.contains(key)) {
        return;
    }

    m_requested.insert(key);

    QPointer<MapCanvas> const canvas = m_canvas;
    m_loader->requestTile(
        layerId.toStdString(),
        tile,
        [canvas](mts::core::Result<mts::imagery::SharedImage> const&) {
            if (canvas != nullptr) {
                QMetaObject::invokeMethod(canvas, &MapCanvas::handleTileUpdate, Qt::QueuedConnection);
            }
        }
    );
}
