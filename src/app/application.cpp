#include "application.hpp"

#include "app_context.hpp"
#include "chat_controller.hpp"
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
import foundation;
import agent;
import runtime;
import config;
import events;
import persistence;
import provider;
import harness;
import orchestration;
import imagery;
import map;

namespace {

auto makeMapTools(MapController* controller, mts::config::AppConfig const& config) -> mts::harness::MapTools {
    mts::harness::MapTools tools;
    tools.visionTimeoutMs = int(config.vision.timeoutMs);
    int const maxPixels = int(config.vision.maxPixels);

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

    std::string configError;
    auto const config = mts::config::loadConfigOrDefaults(mts::config::configPath(), configError);
    if (!configError.empty()) {
        m_initialStatus = configError;
    } else if (config.provider.model.empty() || config.provider.apiKey.empty()) {
        m_initialStatus = "No model configured; using the offline provider. Create config/mindtosis.json.";
    }

    auto const workers = config.runtime.workers == 0 ? 4u : config.runtime.workers;
    m_pool = std::make_unique<mts::runtime::ThreadPool>(workers);
    m_bus = std::make_unique<mts::events::EventBus>(m_pool.get());

    auto store = mts::persistence::makeSessionStore(mts::persistence::databasePath());
    if (store) {
        m_store = std::make_unique<mts::persistence::SessionStoreHandle>(std::move(store.value()));
    } else {
        m_initialStatus = std::string(store.error().message());
    }

    auto provider = mts::model::makeProvider(config);
    if (provider) {
        m_provider = std::make_unique<mts::model::ProviderHandle>(std::move(provider.value()));
    } else {
        m_initialStatus = std::string(provider.error().message());
    }

    if (!config.provider.apiKey.empty()) {
        auto namingConfig = config;
        if (!config.naming.model.empty()) {
            namingConfig.provider.model = config.naming.model;
        }

        if (!namingConfig.provider.model.empty()) {
            if (auto naming = mts::model::makeProvider(namingConfig); naming) {
                m_namingProvider = std::make_unique<mts::model::ProviderHandle>(std::move(naming.value()));
            }
        }
    }

    if (!config.vision.model.empty()) {
        auto vision = mts::model::makeVisionProvider(config);
        if (vision) {
            m_visionProvider = std::make_unique<mts::model::ProviderHandle>(std::move(vision.value()));
        } else {
            m_initialStatus = std::string(vision.error().message());
        }
    }

    m_mapController = std::make_unique<MapController>(
        *m_pool,
        mts::imagery::expandHomePath(config.map.configPath),
        mts::imagery::expandHomePath(config.cache.tilesDir),
        m_application.get()
    );
    if (!m_mapController->status().isEmpty()) {
        m_initialStatus = m_mapController->status().toStdString();
    }

    if (m_store != nullptr && m_provider != nullptr) {
        auto mapTools = makeMapTools(m_mapController.get(), config);
        m_harness = std::make_unique<mts::harness::AgentHarness>(
            *m_provider, m_namingProvider.get(), m_visionProvider.get(), *m_store,
            *m_pool, *m_bus, config.server, std::move(mapTools)
        );
    }

    m_conversationService = std::make_unique<mts::orchestration::ConversationService>(
        m_store.get(),
        m_harness.get(),
        m_initialStatus
    );

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
        m_initialStatus,
    };

    m_chat = new ChatController(context, m_application.get());

    mts::log::info("Done! Connecting application to GUI engine...");

    m_engine->setInitialProperties({
        {"chat", QVariant::fromValue(static_cast<QObject*>(m_chat))},
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
