#include "application.hpp"

#include "app_context.hpp"
#include "chat_controller.hpp"
#include "interaction_controller.hpp"
#include "map_controller.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QPointF>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QRectF>
#include <QSizeF>
#include <QUrl>
#include <QVariant>

import std;
import logging;
import core;
import agent;
import runtime;
import config;
import events;
import persistence;
import provider;
import harness;
import orchestration;
import logging;
import imagery;
import map;

namespace {

struct Paths {
    std::filesystem::path configFile;
    std::filesystem::path databaseFile;
    std::filesystem::path home;
};

auto environmentValue(char const* name) -> std::optional<std::string> {
    if (char const* value = std::getenv(name); value != nullptr && *value != '\0') {
        return std::string(value);
    }

    return std::nullopt;
}

auto expandHome(std::string const& path, std::filesystem::path const& home) -> std::filesystem::path {
    if (path.empty() || path.front() != '~' || home.empty()) {
        return std::filesystem::path(path);
    }

    if (path.size() == 1) {
        return home;
    }

    if (path.at(1) == '/' || path.at(1) == std::filesystem::path::preferred_separator) {
        return home / std::filesystem::path(path.substr(2));
    }

    return std::filesystem::path(path);
}

auto resolvePaths() -> Paths {
    auto const home = environmentValue("HOME");

    Paths paths;
    paths.configFile = std::filesystem::path("config/mindtosis.json");
    paths.databaseFile = std::filesystem::path("mindtosis.db");
    if (home.has_value()) {
        paths.home = std::filesystem::path(*home);
    }

    if (auto const value = environmentValue("MINDTOIS_CONFIG"); value.has_value()) {
        paths.configFile = std::filesystem::path(*value);
    }

    if (auto const value = environmentValue("MINDTOIS_DB"); value.has_value()) {
        paths.databaseFile = std::filesystem::path(*value);
    } else if (auto const value = environmentValue("XDG_DATA_HOME"); value.has_value()) {
        paths.databaseFile = std::filesystem::path(*value) / "mindtosis" / "mindtosis.db";
    } else if (home.has_value()) {
        paths.databaseFile = paths.home / ".local" / "share" / "mindtosis" / "mindtosis.db";
    }

    return paths;
}

auto makeMapTools(MapController* controller, mts::model::VisionConfig const& vision) -> mts::harness::MapTools {
    mts::harness::MapTools tools;
    tools.visionTimeoutMs = int(vision.timeoutMs);
    int const maxPixels = int(vision.maxPixels);

    tools.info = [controller]() -> mts::agent::Result<mts::agent::json> {
        auto const* loader = controller != nullptr ? controller->loader() : nullptr;
        if (loader == nullptr) {
            return mts::agent::Result<mts::agent::json>::fromStatus(
                mts::agent::Status::unavailable(
                    controller != nullptr && !controller->status().isEmpty()
                    ? controller->status().toStdString()
                    : "map is not loaded"
                )
            );
        }

        mts::map::MapInfo const& info = loader->info();
        mts::agent::json result = mts::agent::json::object();
        result["id"] = info.id;
        result["name"] = info.name;
        result["unitLabel"] = info.transform.unitLabel;
        result["metersPerPixel"] = info.transform.metersPerPixel;
        result["minLevel"] = info.minLevel;
        result["maxLevel"] = info.maxLevel;
        result["tileSize"] = info.tileSize;
        result["baseSize"] = mts::agent::json{{"width", info.baseSize.width()}, {"height", info.baseSize.height()}};
        result["worldBounds"] = mts::agent::json{
            {"x", info.worldBounds.left()},
            {"y", info.worldBounds.top()},
            {"width", info.worldBounds.width()},
            {"height", info.worldBounds.height()},
        };

        mts::agent::json layers = mts::agent::json::array();
        for (auto const* layer : loader->layers()) {
            layers.push_back(mts::agent::json{
                {"id", layer->id()},
                {"name", layer->name()},
                {"visible", layer->visible()},
                {"opacity", layer->opacity()},
                {"blend", mts::map::blendModeToString(layer->blend())},
            });
        }
        result["layers"] = std::move(layers);

        return result;
    };

    tools.snapshot = [controller, maxPixels](
        std::optional<mts::harness::MapToolRegion> const& region,
        std::optional<int> level,
        std::vector<std::string> const& layers
    ) -> mts::agent::Result<mts::harness::MapToolSnapshot> {
        if (controller == nullptr || controller->loader() == nullptr || controller->snapshotService() == nullptr) {
            return mts::agent::Result<mts::harness::MapToolSnapshot>::fromStatus(
                mts::agent::Status::unavailable("map imagery is not available")
            );
        }

        mts::map::MapInfo const& info = controller->loader()->info();
        QRectF worldRect = info.worldBounds;
        if (region.has_value()) {
            worldRect = QRectF(
                QPointF(region->x, region->y),
                QSizeF(region->width, region->height)
            ).normalized();
        }

        mts::map::SnapshotOptions options;
        options.level = level;
        options.layers = layers;
        options.maxPixels = maxPixels;
        auto const snapshot = controller->snapshotService()->renderRegion(worldRect, options);
        if (!snapshot) {
            return mts::agent::fromFoundation(snapshot.error());
        }

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        snapshot.value().image.save(&buffer, "png");
        buffer.close();

        mts::harness::MapToolSnapshot out;
        out.snapshotId = snapshot.value().id;
        out.width = snapshot.value().pixelSize.width();
        out.height = snapshot.value().pixelSize.height();
        out.level = snapshot.value().level;
        out.layers = snapshot.value().layers;
        out.region = mts::harness::MapToolRegion{
            worldRect.left(), worldRect.top(), worldRect.width(), worldRect.height()};
        out.unitLabel = info.transform.unitLabel;
        out.pngBase64 = png.toBase64().toStdString();

        return out;
    };

    return tools;
}

} // END OF NAMESPACE anonymous

mts::app::Application::Application(int argc, char** argv)
    : m_argc(argc)
    , m_argv(argv) {
    initialize_logging();
    mts::log::info("Initializing application...");

    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);

    m_application = std::make_unique<QGuiApplication>(argc, argv);
    m_engine = std::make_unique<QQmlApplicationEngine>();

    Paths const paths = resolvePaths();

    std::string configError;
    auto const config = mts::config::loadConfigOrDefaults(paths.configFile, configError);
    auto const view = config.view();

    mts::model::ProviderConfig providerConfig;
    mts::model::VisionConfig visionConfig;
    mts::harness::ServerConfig serverConfig;
    mts::harness::NamingConfig namingConfig;
    mts::config::RuntimeConfig runtimeConfig;
    mts::imagery::CacheConfig cacheConfig;
    mts::map::MapConfig mapConfig;

    auto const reportConfigError = [this](mts::core::Status const& status) {
        if (!status && m_initialStatus.empty()) {
            m_initialStatus = std::string(status.error().message());
        }
    };

    reportConfigError(mts::model::applyConfig(view, providerConfig));
    reportConfigError(mts::model::applyConfig(view, visionConfig));
    reportConfigError(mts::harness::applyConfig(view, serverConfig));
    reportConfigError(mts::harness::applyConfig(view, namingConfig));
    reportConfigError(mts::config::applyConfig(view, runtimeConfig));
    reportConfigError(mts::imagery::applyConfig(view, cacheConfig));
    reportConfigError(mts::map::applyConfig(view, mapConfig));

    if (!configError.empty()) {
        m_initialStatus = configError;
    } else if (m_initialStatus.empty() && (providerConfig.model.empty() || providerConfig.apiKey.empty())) {
        m_initialStatus = "No model configured; using the offline provider. Create config/mindtosis.json.";
    }

    auto const workers = runtimeConfig.workers == 0 ? 4u : runtimeConfig.workers;
    m_pool = std::make_unique<mts::runtime::ThreadPool>(workers);
    m_bus = std::make_unique<mts::events::EventBus>(m_pool.get());

    auto store = mts::persistence::makeSessionStore(paths.databaseFile);
    if (store) {
        m_store = std::make_unique<mts::persistence::SessionStoreHandle>(std::move(store.value()));
    } else {
        m_initialStatus = std::string(store.error().message());
    }

    auto provider = mts::model::makeProvider(providerConfig);
    if (provider) {
        m_provider = std::make_unique<mts::model::ProviderHandle>(std::move(provider.value()));
    } else {
        m_initialStatus = std::string(provider.error().message());
    }

    if (!providerConfig.apiKey.empty()) {
        auto namingProviderConfig = providerConfig;
        if (!namingConfig.model.empty()) {
            namingProviderConfig.model = namingConfig.model;
        }

        if (!namingProviderConfig.model.empty()) {
            if (auto naming = mts::model::makeProvider(namingProviderConfig); naming) {
                m_namingProvider = std::make_unique<mts::model::ProviderHandle>(std::move(naming.value()));
            }
        }
    }

    if (!visionConfig.model.empty()) {
        auto vision = mts::model::makeVisionProvider(providerConfig, visionConfig);
        if (vision) {
            m_visionProvider = std::make_unique<mts::model::ProviderHandle>(std::move(vision.value()));
        } else {
            m_initialStatus = std::string(vision.error().message());
        }
    }

    std::filesystem::path const mapConfigFile = expandHome(mapConfig.configPath, paths.home);
    std::filesystem::path const tilesCacheDir = expandHome(cacheConfig.tilesDir, paths.home);
    m_mapController = std::make_unique<MapController>(
        *m_pool,
        mapConfigFile,
        tilesCacheDir,
        m_application.get()
    );
    if (!m_mapController->status().isEmpty()) {
        m_initialStatus = m_mapController->status().toStdString();
    }

    if (m_store != nullptr && m_provider != nullptr) {
        auto mapTools = makeMapTools(m_mapController.get(), visionConfig);
        m_harness = std::make_unique<mts::harness::AgentHarness>(
            *m_provider, m_namingProvider.get(), m_visionProvider.get(), *m_store,
            *m_pool, *m_bus, serverConfig, std::move(mapTools)
        );
    }

    m_conversationService = std::make_unique<mts::orchestration::ConversationService>(
        m_store.get(),
        m_harness.get(),
        m_initialStatus
    );

    m_interaction = new InteractionController(m_application.get());
    m_interaction->setEventBus(m_bus.get());

    mts::app::AppContext const context{
        *m_pool,
        *m_bus,
        m_store.get(),
        m_provider.get(),
        m_namingProvider.get(),
        m_visionProvider.get(),
        m_harness.get(),
        *m_mapController,
        *m_conversationService,
        m_interaction,
        m_initialStatus,
    };

    m_chat = new ChatController(context, m_application.get());

    mts::log::info("Done! Connecting application to GUI engine...");

    m_engine->setInitialProperties({
        {"chat", QVariant::fromValue(static_cast<QObject*>(m_chat))},
        {"interaction", QVariant::fromValue(static_cast<QObject*>(m_interaction))},
    });

    QObject::connect(
        m_engine.get(),
        &QQmlApplicationEngine::objectCreationFailed,
        m_application.get(),
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection
    );

    m_engine->load(QUrl("qrc:/qt/qml/Application/qml/Main.qml"));

    mts::log::info("Done! Running application...");
}

mts::app::Application::~Application() {
    delete m_chat;
    m_chat = nullptr;

    delete m_interaction;
    m_interaction = nullptr;

    m_conversationService.reset();
    m_pool.reset();

    m_harness.reset();
    m_mapController.reset();
    m_visionProvider.reset();
    m_namingProvider.reset();
    m_provider.reset();
    m_store.reset();
    m_engine.reset();
    m_bus.reset();
    m_application.reset();
}

auto mts::app::Application::run() -> int {
    return m_application->exec();
}
