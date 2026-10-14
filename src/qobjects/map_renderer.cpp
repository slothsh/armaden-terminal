#include "map_renderer.hpp"

#include "canvas_resource_cache.hpp"
#include "map_canvas.hpp"
#include "map_controller.hpp"
#include "map_view.hpp"
#include "tile_presenter.hpp"

#include <QCanvasPainter>
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QtGlobal>
#include <QtQuick/QQuickWindow>

#include <chrono>

import std;
import ecs;
import render2d;
import map;

struct MapRenderer::Impl {
    MapCanvas* canvas = nullptr;
    MapController* controller = nullptr;
    mts::map::MapLoader const* loader = nullptr;
    CanvasResourceCache cache;
    TilePresenter presenter{&cache};
    mts::ecs::registry world;
    mts::ecs::entity camera = mts::ecs::null;
    mts::ecs::entity grid = mts::ecs::null;
    float devicePixelRatio = 1.0f;
    std::chrono::steady_clock::time_point lastCameraChangeAt = std::chrono::steady_clock::now();
};

MapRenderer::MapRenderer()
    : m_impl(std::make_unique<Impl>()) {}

MapRenderer::~MapRenderer() = default;

auto MapRenderer::initializeResources(QCanvasPainter* painter) -> void {
    static_cast<void>(painter);

    if (!m_impl->world.valid(m_impl->camera)) {
        m_impl->camera = m_impl->world.create();
        m_impl->world.emplace<mts::render2d::Camera>(m_impl->camera, mts::render2d::Camera{});
        m_impl->world.emplace<mts::render2d::MainCamera>(m_impl->camera);
    }

    if (!m_impl->world.valid(m_impl->grid)) {
        m_impl->grid = m_impl->world.create();
        m_impl->world.emplace<mts::render2d::GridStyle>(m_impl->grid, mts::render2d::GridStyle{});
    }
}

auto MapRenderer::synchronizeData(QCanvasPainterItem* item) -> void {
    m_impl->devicePixelRatio = item->window() != nullptr
        ? static_cast<float>(item->window()->devicePixelRatio())
        : 1.0f;

    if (QCanvasPainter* const painter = this->painter(); painter != nullptr) {
        m_impl->cache.maybeRelease(painter, m_impl->lastCameraChangeAt);
    }

    auto const* canvas = static_cast<MapCanvas const*>(item);
    MapController* const controller = canvas->controller();
    if (controller != m_impl->controller) {
        m_impl->controller = controller;
        m_impl->cache.reset();
        m_impl->presenter.reset();
    }

    auto const* loader = m_impl->controller != nullptr ? m_impl->controller->loader() : nullptr;
    if (loader != m_impl->loader) {
        m_impl->loader = loader;
        m_impl->cache.reset();
        m_impl->presenter.reset();
    }

    if (m_impl->canvas != canvas) {
        m_impl->canvas = const_cast<MapCanvas*>(canvas);
        m_impl->presenter.setCanvas(m_impl->canvas);
    }

    if (!m_impl->world.valid(m_impl->camera)) {
        return;
    }

    MapView const& view = canvas->view();
    float const scale = view.scale();
    auto& camera = m_impl->world.get<mts::render2d::Camera>(m_impl->camera);
    QPointF const center = view.cameraCenter();
    if (center != camera.center || scale != camera.scale) {
        m_impl->lastCameraChangeAt = std::chrono::steady_clock::now();
    }

    camera.center = center;
    camera.scale = scale;
    camera.rotation = 0.0f;
}

auto MapRenderer::paint(QCanvasPainter* painter) -> void {
    QRectF const canvasRect(0, 0, this->width(), this->height());
    float const devicePixelRatio = qMax(1.0f, m_impl->devicePixelRatio);
    mts::render2d::RenderContext const context{
        painter,
        canvasRect,
        devicePixelRatio,
        1.0f / devicePixelRatio,
    };

    painter->setFillStyle(QColor(24, 24, 24));
    painter->fillRect(canvasRect);

    if (!m_impl->world.valid(m_impl->camera)) {
        return;
    }

    mts::render2d::Camera const& camera = m_impl->world.get<mts::render2d::Camera>(m_impl->camera);
    mts::render2d::drawGridSystem(context, m_impl->world, camera);

    if (m_impl->loader != nullptr) {
        m_impl->presenter.present(painter, context, camera, m_impl->loader);
    }
}
