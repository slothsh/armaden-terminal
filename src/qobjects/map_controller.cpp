#include "map_controller.hpp"

import std;
import runtime;
import imagery;
import map;

MapController::MapController(mts::runtime::ThreadPool& pool,
                             std::filesystem::path mapConfigPath,
                             std::filesystem::path cacheDir,
                             QObject* parent)
    : QObject(parent)
    , m_pool(pool) {
    auto loader = mts::map::MapLoader::create(mapConfigPath, {cacheDir}, m_pool);
    if (!loader) {
        m_status = QString::fromStdString(loader.status().message());
        emit this->statusChanged();
        return;
    }

    m_loader = std::move(loader.value());
    m_snapshots = std::make_unique<mts::map::MapSnapshotService>(m_loader.get(), cacheDir);
    m_loader->startBuild();
    m_name = QString::fromStdString(m_loader->info().name);
    m_ready = true;
    emit this->nameChanged();
    emit this->readyChanged();
}

MapController::~MapController() = default;

auto MapController::ready() const -> bool {
    return m_ready;
}

auto MapController::name() const -> QString {
    return m_name;
}

auto MapController::status() const -> QString {
    return m_status;
}

auto MapController::loader() const -> mts::map::MapLoader const* {
    return m_loader.get();
}

auto MapController::snapshotService() const -> mts::map::MapSnapshotService const* {
    return m_snapshots.get();
}
