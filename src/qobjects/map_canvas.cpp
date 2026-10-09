#include "map_canvas.hpp"

#include "map_controller.hpp"

#include <QCanvasImage>
#include <QCanvasPainter>
#include <QCanvasPainterItemRenderer>
#include <QImage>
#include <QMetaObject>
#include <QMouseEvent>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QWheelEvent>

import std;
import agent;
import ecs;
import imagery;
import map;

namespace {

constexpr float minimumZDepth = -4.0f;
constexpr float maximumZDepth = 2.0f;
constexpr float wheelDepthStep = 0.1f;
constexpr int kMaximumBackfillUp = 4;
constexpr double kFitMargin = 0.92;

float scaleForDepth(float zDepth) {
    return std::exp(zDepth);
}

QString tileKey(QString const& layerId, mts::imagery::TileId const& tile) {
    return layerId + "/" + QString::number(tile.level) + "/" + QString::number(tile.x) + "/"
        + QString::number(tile.y);
}

QImage prepareTileForLayer(QImage const& image, double opacity) {
    if (opacity >= 0.999)
        return image;

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

class MapCanvasRenderer final : public QCanvasPainterItemRenderer {
public:
    void initializeResources(QCanvasPainter* painter) override {
        Q_UNUSED(painter);
        if (!m_world.valid(m_camera)) {
            m_camera = m_world.create();
            m_world.emplace<mts::ecs::Camera>(m_camera, mts::ecs::Camera{});
            m_world.emplace<mts::ecs::MainCamera>(m_camera);
        }

        if (!m_world.valid(m_grid)) {
            m_grid = m_world.create();
            m_world.emplace<mts::ecs::GridStyle>(m_grid, mts::ecs::GridStyle{});
        }
    }

    void synchronizeData(QCanvasPainterItem* item) override {
        auto const* canvas = static_cast<MapCanvas const*>(item);
        MapController* const controller = canvas->controller();
        if (controller != m_controller) {
            m_controller = controller;
            this->resetMapState();
        }

        auto const* loader = m_controller != nullptr ? m_controller->loader() : nullptr;
        if (loader != m_loader) {
            m_loader = loader;
            this->resetMapState();
        }

        if (m_canvas != canvas)
            m_canvas = const_cast<MapCanvas*>(canvas);

        if (!m_world.valid(m_camera))
            return;

        float const scale = scaleForDepth(canvas->zDepth());
        auto& camera = m_world.get<mts::ecs::Camera>(m_camera);
        camera.center = QPointF(-canvas->translationX() / scale, canvas->translationY() / scale);
        camera.scale = scale;
        camera.rotation = 0.0f;
    }

    void paint(QCanvasPainter* painter) override {
        QRectF const canvasRect(0, 0, this->width(), this->height());
        float const devicePixelRatio = qMax(1.0f, painter->devicePixelRatio());
        mts::ecs::RenderContext const context{
            painter,
            canvasRect,
            devicePixelRatio,
            1.0f / devicePixelRatio,
        };

        painter->setFillStyle(QColor(24, 24, 24));
        painter->fillRect(canvasRect);

        if (!m_world.valid(m_camera))
            return;

        mts::ecs::Camera const& camera = m_world.get<mts::ecs::Camera>(m_camera);
        mts::ecs::drawGridSystem(context, m_world, camera);

        if (m_loader != nullptr)
            this->paintMapLayers(painter, context, camera);
    }

private:
    struct TileSlot {
        QCanvasImage image;
        double opacity = -1.0;
    };

    void resetMapState() {
        m_images.clear();
        m_requested.clear();
    }

    void paintMapLayers(QCanvasPainter* painter, mts::ecs::RenderContext const& context, mts::ecs::Camera const& camera) {
        mts::map::MapLoader const* loader = m_loader;
        mts::map::MapInfo const& info = loader->info();
        QSet<QString> seen;

        auto const layerTiles = loader->visibleTiles(context.canvasRect, camera, double(context.devicePixelRatio));
        for (auto const& entry : layerTiles) {
            if (!entry.visible)
                continue;

            mts::map::MapLayer* const layer = loader->layer(entry.layerId);
            if (layer == nullptr)
                continue;

            mts::imagery::LevelInfo const& grid = entry.viewport.levelInfo;
            double const opacity = layer->opacity();
            for (auto const& tile : entry.viewport.tiles) {
                QString const key = tileKey(QString::fromStdString(entry.layerId), tile);
                auto const state = layer->cache().state(tile);
                if (state == mts::imagery::TileState::Ready) {
                    this->drawReadyTile(painter, context, camera, info, grid, layer, opacity, tile, key, seen);
                } else if (state == mts::imagery::TileState::Missing) {
                    this->requestTile(loader, QString::fromStdString(entry.layerId), tile);
                    this->paintBackfill(painter, context, camera, info, layer, opacity, entry.viewport.levelRect, tile, seen);
                } else if (state == mts::imagery::TileState::Queued) {
                    this->paintBackfill(painter, context, camera, info, layer, opacity, entry.viewport.levelRect, tile, seen);
                }
            }
        }

        auto it = m_images.begin();
        while (it != m_images.end()) {
            if (seen.contains(it.key())) {
                ++it;
                continue;
            }
            painter->removeImage(it->image);
            it = m_images.erase(it);
        }
        m_requested.clear();
    }

    void drawReadyTile(QCanvasPainter* painter,
                       mts::ecs::RenderContext const& context,
                       mts::ecs::Camera const& camera,
                       mts::map::MapInfo const& info,
                       mts::imagery::LevelInfo const& grid,
                       mts::map::MapLayer* layer,
                       double opacity,
                       mts::imagery::TileId const& tile,
                       QString const& key,
                       QSet<QString>& seen) {
        auto it = m_images.find(key);
        if (it != m_images.end() && qAbs(it->opacity - opacity) > 0.001) {
            painter->removeImage(it->image);
            it = m_images.erase(it);
        }
        if (it == m_images.end()) {
            auto const entry = layer->cache().find(tile);
            if (!entry || entry->state != mts::imagery::TileState::Ready)
                return;
            it = m_images.insert(key, TileSlot{painter->addImage(prepareTileForLayer(entry->image, opacity)), opacity});
        }
        seen.insert(key);

        QRectF const tileBase = mts::imagery::levelRectToBaseRect(mts::imagery::tileRect(grid, tile), tile.level);
        QRectF const tileWorld = info.transform.pixelRectToMetricRect(tileBase);
        QRectF const destination = mts::ecs::screenRectForWorldRect(camera, context.canvasRect.size(), tileWorld);
        painter->drawImage(it->image, destination);
    }

    void paintBackfill(QCanvasPainter* painter,
                       mts::ecs::RenderContext const& context,
                       mts::ecs::Camera const& camera,
                       mts::map::MapInfo const& info,
                       mts::map::MapLayer* layer,
                       double opacity,
                       QRectF const& levelRect,
                       mts::imagery::TileId const& tile,
                       QSet<QString>& seen) {
        for (int up = 1; up <= kMaximumBackfillUp && tile.level - up >= 0; ++up) {
            int const factor = 1 << up;
            mts::imagery::TileId const ancestor{tile.level - up, tile.x / factor, tile.y / factor};

            auto const cached = layer->cache().find(ancestor);
            if (!cached || cached->state != mts::imagery::TileState::Ready)
                continue;

            mts::imagery::LevelInfo const ancestorGrid = mts::imagery::levelInfoForBase(info.baseSize, info.tileSize, ancestor.level);
            QRectF const childBase = mts::imagery::levelRectToBaseRect(mts::imagery::tileRect(mts::imagery::levelInfoForBase(info.baseSize, info.tileSize, tile.level), tile), tile.level);
            QRectF const ancestorBase = mts::imagery::levelRectToBaseRect(mts::imagery::tileRect(ancestorGrid, ancestor), ancestor.level);

            QString const key = tileKey(QString::fromStdString(layer->id()), ancestor) + "#backfill";
            auto it = m_images.find(key);
            if (it != m_images.end() && qAbs(it->opacity - opacity) > 0.001) {
                painter->removeImage(it->image);
                it = m_images.erase(it);
            }
            if (it == m_images.end())
                it = m_images.insert(key, TileSlot{painter->addImage(prepareTileForLayer(cached->image, opacity)), opacity});
            seen.insert(key);

            QRectF const ancestorWorld = info.transform.pixelRectToMetricRect(ancestorBase);
            QRectF const childWorld = info.transform.pixelRectToMetricRect(childBase);
            QRectF const ancestorScreen = mts::ecs::screenRectForWorldRect(camera, context.canvasRect.size(), ancestorWorld);
            QRectF const childScreen = mts::ecs::screenRectForWorldRect(camera, context.canvasRect.size(), childWorld);

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
            painter->drawImage(it->image, sourceRect, childScreen);
            return;
        }
    }

    void requestTile(mts::map::MapLoader const* loader, QString const& layerId, mts::imagery::TileId const& tile) {
        QString const key = tileKey(layerId, tile);
        if (m_requested.contains(key))
            return;
        m_requested.insert(key);

        QPointer<MapCanvas> const canvas = m_canvas;
        loader->requestTile(layerId.toStdString(), tile, 1,
            [canvas](mts::agent::Result<mts::imagery::SharedImage> const&) {
                if (canvas != nullptr)
                    QMetaObject::invokeMethod(canvas, &MapCanvas::handleTileUpdate, Qt::QueuedConnection);
            });
    }

    QPointer<MapCanvas> m_canvas;
    MapController* m_controller = nullptr;
    mts::map::MapLoader const* m_loader = nullptr;
    mts::ecs::registry m_world;
    mts::ecs::entity m_camera = mts::ecs::null;
    mts::ecs::entity m_grid = mts::ecs::null;
    QHash<QString, TileSlot> m_images;
    QSet<QString> m_requested;
};

}

MapCanvas::MapCanvas(QQuickItem* parent)
    : QCanvasPainterItem(parent) {
    this->setAcceptedMouseButtons(Qt::MiddleButton);
    this->connect(this, &QQuickItem::widthChanged, this, [this] { this->maybeApplyInitialView(); });
    this->connect(this, &QQuickItem::heightChanged, this, [this] { this->maybeApplyInitialView(); });
}

void MapCanvas::componentComplete() {
    QCanvasPainterItem::componentComplete();
    this->maybeApplyInitialView();
}

void MapCanvas::maybeApplyInitialView() {
    if (m_initialViewApplied)
        return;
    if (m_controller == nullptr || m_controller->loader() == nullptr)
        return;
    if (this->width() <= 0.0 || this->height() <= 0.0)
        return;
    this->fitToMap();
}

void MapCanvas::fitToMap() {
    if (m_controller == nullptr || m_controller->loader() == nullptr)
        return;
    if (this->width() <= 0.0 || this->height() <= 0.0)
        return;

    QRectF const bounds = m_controller->loader()->info().worldBounds;
    if (bounds.isEmpty() || !(bounds.width() > 0.0) || !(bounds.height() > 0.0))
        return;

    double const scale = qMin(
        double(this->width()) * kFitMargin / bounds.width(),
        double(this->height()) * kFitMargin / bounds.height()
    );
    if (!(scale > 0.0))
        return;

    this->setZDepth(float(std::log(scale)));
    float const applied = scaleForDepth(m_zDepth);
    QPointF const center = bounds.center();
    this->setTranslationX(float(-center.x() * applied));
    this->setTranslationY(float(center.y() * applied));
    m_initialViewApplied = true;
}

void MapCanvas::setController(MapController* controller) {
    if (m_controller.data() == controller)
        return;
    m_controller = controller;
    QObject::disconnect(m_readyConnection);
    if (m_controller != nullptr)
        m_readyConnection = this->connect(m_controller.data(), &MapController::readyChanged, this, [this] { this->maybeApplyInitialView(); });
    m_initialViewApplied = false;
    emit this->controllerChanged();
    this->maybeApplyInitialView();
    this->update();
}

auto MapCanvas::controller() const -> MapController* {
    return m_controller.data();
}

auto MapCanvas::translationX() const -> float {
    return m_translationX;
}

void MapCanvas::setTranslationX(float translation) {
    if (qFuzzyCompare(m_translationX, translation))
        return;
    m_translationX = translation;
    emit this->translationXChanged();
    this->update();
}

auto MapCanvas::translationY() const -> float {
    return m_translationY;
}

void MapCanvas::setTranslationY(float translation) {
    if (qFuzzyCompare(m_translationY, translation))
        return;
    m_translationY = translation;
    emit this->translationYChanged();
    this->update();
}

auto MapCanvas::zDepth() const -> float {
    return m_zDepth;
}

void MapCanvas::setZDepth(float depth) {
    float const clampedDepth = qBound(minimumZDepth, depth, maximumZDepth);
    if (qFuzzyCompare(m_zDepth, clampedDepth))
        return;
    m_zDepth = clampedDepth;
    emit this->zDepthChanged();
    this->update();
}

QCanvasPainterItemRenderer* MapCanvas::createItemRenderer() const {
    return new MapCanvasRenderer;
}

void MapCanvas::handleTileUpdate() {
    this->update();
}

void MapCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::MiddleButton) {
        event->ignore();
        return;
    }

    m_lastDragPosition = event->position();
    m_dragging = true;
    event->accept();
}

void MapCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (!m_dragging) {
        event->ignore();
        return;
    }

    QPointF const position = event->position();
    QPointF const delta = position - m_lastDragPosition;
    m_lastDragPosition = position;
    this->setTranslationX(m_translationX + float(delta.x()));
    this->setTranslationY(m_translationY + float(delta.y()));
    event->accept();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::MiddleButton || !m_dragging) {
        event->ignore();
        return;
    }

    m_dragging = false;
    event->accept();
}

void MapCanvas::wheelEvent(QWheelEvent* event) {
    QPoint const angleDelta = event->angleDelta();
    QPoint const pixelDelta = event->pixelDelta();
    float const delta = angleDelta.y() != 0
        ? float(angleDelta.y()) / 120.0f
        : float(pixelDelta.y()) / 100.0f;

    if (qFuzzyIsNull(delta)) {
        event->ignore();
        return;
    }

    float const previousScale = scaleForDepth(m_zDepth);
    float const nextDepth = qBound(
        minimumZDepth,
        m_zDepth + delta * wheelDepthStep,
        maximumZDepth
    );
    float const nextScale = scaleForDepth(nextDepth);
    QPointF const cursorOffset = event->position() - QPointF(this->width() * 0.5f, this->height() * 0.5f);
    float const scaleRatio = nextScale / previousScale;

    this->setTranslationX(float(cursorOffset.x() - (cursorOffset.x() - m_translationX) * scaleRatio));
    this->setTranslationY(float(cursorOffset.y() - (cursorOffset.y() - m_translationY) * scaleRatio));
    this->setZDepth(nextDepth);

    event->accept();
}
