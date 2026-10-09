#include "chat_controller.hpp"

#include "conversation_model.hpp"
#include "map_controller.hpp"
#include "session_list_model.hpp"

#include <QBuffer>
#include <QHash>
#include <QImage>
#include <QMetaObject>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QSizeF>
#include <QString>

import std;
import agent;
import runtime;
import events;
import config;
import persistence;
import provider;
import harness;
import imagery;
import map;

namespace {

std::string const kDefaultSessionTitle = "New session";

auto blockKindFor(mts::persistence::MessageKind kind) -> int {
    switch (kind) {
        case mts::persistence::MessageKind::UserText:
            return ConversationModel::UserText;

        case mts::persistence::MessageKind::AssistantText:
            return ConversationModel::AssistantText;

        case mts::persistence::MessageKind::Reasoning:
            return ConversationModel::Reasoning;

        case mts::persistence::MessageKind::ToolCall:
            return ConversationModel::ToolCall;

        case mts::persistence::MessageKind::ToolResult:
            return ConversationModel::ToolResult;

        case mts::persistence::MessageKind::Error:
            return ConversationModel::Error;
    }

    return ConversationModel::AssistantText;
}

auto blockKindForEvent(mts::events::MessageKind kind) -> int {
    switch (kind) {
        case mts::events::MessageKind::UserText:
            return ConversationModel::UserText;

        case mts::events::MessageKind::AssistantText:
            return ConversationModel::AssistantText;

        case mts::events::MessageKind::Reasoning:
            return ConversationModel::Reasoning;

        case mts::events::MessageKind::Error:
            return ConversationModel::Error;
    }

    return ConversationModel::AssistantText;
}

auto blockFor(mts::persistence::Message const& message) -> ConversationBlock {
    ConversationBlock block;
    block.kind = blockKindFor(message.kind);
    block.text = QString::fromStdString(message.text);
    block.payload = QString::fromStdString(message.payload);

    return block;
}

} // END OF NAMESPACE anonymous

struct PendingRun {
    QList<ConversationBlock> blocks;
    int streamingIndex = -1;
};

struct ChatController::Impl {
    mts::events::EventBus bus;
    std::unique_ptr<mts::persistence::SessionStoreHandle> store;
    std::unique_ptr<mts::model::ProviderHandle> provider;
    std::unique_ptr<MapController> mapController;
    std::unique_ptr<mts::model::ProviderHandle> namingProviderOwned;
    mts::model::ProviderHandle* namingProvider = nullptr;
    std::unique_ptr<mts::model::ProviderHandle> visionProviderOwned;
    mts::model::ProviderHandle* visionProvider = nullptr;
    std::unique_ptr<mts::harness::AgentHarness> harness;
    QHash<QString, std::shared_ptr<mts::runtime::Cancellation>> cancellations;
    QHash<QString, std::shared_ptr<mts::runtime::Cancellation>> namingCancellations;
    QHash<QString, PendingRun> pending;
    QSet<QString> inFlight;
    QString activeSession;
    QString draft;
    QString status;
    int streamingRow = -1;
    bool running = false;

    // Declared last so it is destroyed first: the pool's destructor joins its
    // workers, which must happen before the harness/provider/store/bus above
    // (which the workers reference) are freed.
    std::unique_ptr<mts::runtime::ThreadPool> pool;
};

ChatController::ChatController(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
    , m_sessions(new SessionListModel(this))
    , m_conversation(new ConversationModel(this)) {
    std::string configError;
    auto const config = mts::config::loadConfigOrDefaults(mts::config::configPath(), configError);
    if (!configError.empty()) {
        setStatus(QString::fromStdString(configError));
    } else if (config.provider.model.empty() || config.provider.apiKey.empty()) {
        setStatus(QStringLiteral("No model configured; using the offline provider. Create config/mindtosis.json."));
    }

    auto const workers = config.runtime.workers == 0 ? 4u : config.runtime.workers;
    m_impl->pool = std::make_unique<mts::runtime::ThreadPool>(workers);

    auto store = mts::persistence::makeSessionStore(mts::persistence::databasePath());
    if (store) {
        m_impl->store = std::make_unique<mts::persistence::SessionStoreHandle>(std::move(store.value()));
    } else {
        setStatus(QString::fromStdString(store.status().message()));
    }

    auto provider = mts::model::makeProvider(config);
    if (provider) {
        m_impl->provider = std::make_unique<mts::model::ProviderHandle>(std::move(provider.value()));
    } else {
        setStatus(QString::fromStdString(provider.status().message()));
    }

    if (!config.provider.apiKey.empty()) {
        auto namingConfig = config;
        if (!config.naming.model.empty()) {
            namingConfig.provider.model = config.naming.model;
        }

        if (!namingConfig.provider.model.empty()) {
            if (auto naming = mts::model::makeProvider(namingConfig); naming) {
                m_impl->namingProviderOwned = std::make_unique<mts::model::ProviderHandle>(std::move(naming.value()));
                m_impl->namingProvider = m_impl->namingProviderOwned.get();
            }
        }
    }

    if (!config.vision.model.empty()) {
        auto vision = mts::model::makeVisionProvider(config);
        if (vision) {
            m_impl->visionProviderOwned = std::make_unique<mts::model::ProviderHandle>(std::move(vision.value()));
            m_impl->visionProvider = m_impl->visionProviderOwned.get();
        } else {
            setStatus(QString::fromStdString(vision.status().message()));
        }
    }

    m_impl->mapController = std::make_unique<MapController>(
        *m_impl->pool,
        mts::imagery::expandHomePath(config.map.configPath),
        mts::imagery::expandHomePath(config.cache.tilesDir),
        this
    );
    if (!m_impl->mapController->status().isEmpty()) {
        setStatus(m_impl->mapController->status());
    }

    if (m_impl->store != nullptr && m_impl->provider != nullptr) {
        mts::harness::MapTools mapTools;
        mapTools.visionTimeoutMs = int(config.vision.timeoutMs);
        mapTools.info = [this]() -> mts::agent::Result<mts::agent::json> {
            auto const* controller = m_impl->mapController.get();
            auto const* loader = controller != nullptr ? controller->loader() : nullptr;
            if (loader == nullptr) {
                return mts::agent::Result<mts::agent::json>::fromStatus(mts::agent::Status::unavailable(
                    controller != nullptr && !controller->status().isEmpty()
                        ? controller->status().toStdString()
                        : "map is not loaded"));
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

        mapTools.snapshot = [this, maxPixels = int(config.vision.maxPixels)](
            std::optional<mts::harness::MapToolRegion> const& region,
            std::optional<int> level,
            std::vector<std::string> const& layers
        ) -> mts::agent::Result<mts::harness::MapToolSnapshot> {
            auto const* controller = m_impl->mapController.get();
            if (controller == nullptr || controller->loader() == nullptr || controller->snapshotService() == nullptr) {
                return mts::agent::Result<mts::harness::MapToolSnapshot>::fromStatus(
                    mts::agent::Status::unavailable("map imagery is not available"));
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
                return snapshot.status();
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

        m_impl->harness = std::make_unique<mts::harness::AgentHarness>(
            *m_impl->provider, m_impl->namingProvider, m_impl->visionProvider, *m_impl->store,
            *m_impl->pool, m_impl->bus, config.server, std::move(mapTools)
        );

        static_cast<void>(m_impl->bus.subscribe<mts::events::RunStarted>([this](mts::events::RunStarted const& event) {
            QString const id = QString::fromStdString(event.session);
            QMetaObject::invokeMethod(this, [this, id] {
                m_impl->inFlight.insert(id);
                m_sessions->setRunning(id, true);
                refreshRunning();
            }, Qt::QueuedConnection);
        }));

        static_cast<void>(m_impl->bus.subscribe<mts::events::RunFinished>([this](mts::events::RunFinished const& event) {
            QString const id = QString::fromStdString(event.session);
            QMetaObject::invokeMethod(this, [this, id] {
                m_impl->inFlight.remove(id);
                m_impl->cancellations.remove(id);
                m_impl->pending.remove(id);
                m_sessions->setRunning(id, false);
                if (id == m_impl->activeSession) {
                    reloadConversation(id);
                }

                refreshRunning();
                refreshSessions();
                maybeNameSession(id);
            }, Qt::QueuedConnection);
        }));

        static_cast<void>(m_impl->bus.subscribe<mts::events::RunFailed>([this](mts::events::RunFailed const& event) {
            QString const id = QString::fromStdString(event.session);
            QString const message = QString::fromStdString(event.message);
            QMetaObject::invokeMethod(this, [this, id, message] {
                m_impl->inFlight.remove(id);
                m_impl->cancellations.remove(id);
                m_impl->pending.remove(id);
                m_sessions->setRunning(id, false);
                if (id == m_impl->activeSession) {
                    m_impl->streamingRow = -1;
                    appendError(message);
                }

                refreshRunning();
                refreshSessions();
            }, Qt::QueuedConnection);
        }));

        static_cast<void>(m_impl->bus.subscribe<mts::events::TokenDelta>([this](mts::events::TokenDelta const& event) {
            QString const id = QString::fromStdString(event.session);
            QString const delta = QString::fromStdString(event.delta);
            QMetaObject::invokeMethod(this, [this, id, delta] {
                PendingRun& pending = m_impl->pending[id];
                if (pending.streamingIndex < 0) {
                    pending.blocks.append(ConversationBlock{ConversationModel::AssistantText, {}, {}});
                    pending.streamingIndex = pending.blocks.size() - 1;
                }
                pending.blocks[pending.streamingIndex].text += delta;

                if (id != m_impl->activeSession) {
                    return;
                }

                if (m_impl->streamingRow < 0) {
                    m_conversation->append(ConversationBlock{ConversationModel::AssistantText, {}, {}});
                    m_impl->streamingRow = m_conversation->lastRow();
                }
                m_conversation->appendText(m_impl->streamingRow, delta);
            }, Qt::QueuedConnection);
        }));

        static_cast<void>(m_impl->bus.subscribe<mts::events::MessageAdded>([this](mts::events::MessageAdded const& event) {
            QString const id = QString::fromStdString(event.session);
            int const kind = blockKindForEvent(event.kind);
            QString const text = QString::fromStdString(event.text);
            QMetaObject::invokeMethod(this, [this, id, kind, text] {
                if (id != m_impl->activeSession) {
                    return;
                }

                if (kind == ConversationModel::AssistantText && m_impl->streamingRow >= 0) {
                    m_conversation->setText(m_impl->streamingRow, text);
                    m_impl->streamingRow = -1;
                } else {
                    m_conversation->append(ConversationBlock{kind, text, {}});
                }
            }, Qt::QueuedConnection);
        }));

        static_cast<void>(m_impl->bus.subscribe<mts::events::ToolCallStarted>([this](mts::events::ToolCallStarted const& event) {
            QString const id = QString::fromStdString(event.session);
            QString const name = QString::fromStdString(event.name);
            QString const args = QString::fromStdString(event.arguments);
            QMetaObject::invokeMethod(this, [this, id, name, args] {
                if (id != m_impl->activeSession) {
                    return;
                }

                m_impl->streamingRow = -1;
                m_conversation->append(ConversationBlock{ConversationModel::ToolCall, name, args});
            }, Qt::QueuedConnection);
        }));

        static_cast<void>(m_impl->bus.subscribe<mts::events::ToolCallFinished>([this](mts::events::ToolCallFinished const& event) {
            QString const id = QString::fromStdString(event.session);
            QString const name = QString::fromStdString(event.name);
            QString const result = QString::fromStdString(event.result);
            QMetaObject::invokeMethod(this, [this, id, name, result] {
                if (id != m_impl->activeSession) {
                    return;
                }

                m_conversation->append(ConversationBlock{ConversationModel::ToolResult, name, result});
            }, Qt::QueuedConnection);
        }));
    }

    refreshSessions();
    if (m_sessions->rowCount() > 0) {
        auto const first = m_sessions->index(0);
        if (first.isValid()) {
            selectSession(m_sessions->data(first, SessionListModel::SessionIdRole).toString());
        }
    }
}

ChatController::~ChatController() {
    if (m_impl == nullptr) {
        return;
    }

    for (auto const& cancellation : std::as_const(m_impl->cancellations)) {
        if (cancellation != nullptr) {
            cancellation->cancel();
        }
    }

    for (auto const& cancellation : std::as_const(m_impl->namingCancellations)) {
        if (cancellation != nullptr) {
            cancellation->cancel();
        }
    }

    // Join the pool here, while the harness/provider/store/bus it references are
    // still alive. The pool is declared last in Impl for the same reason.
    m_impl->pool.reset();
}

auto ChatController::sessions() const -> SessionListModel* {
    return m_sessions;
}

auto ChatController::mapController() const -> MapController* {
    return m_impl->mapController.get();
}

auto ChatController::conversation() const -> ConversationModel* {
    return m_conversation;
}

auto ChatController::running() const -> bool {
    return m_impl->running;
}

auto ChatController::status() const -> QString {
    return m_impl->status;
}

auto ChatController::activeSession() const -> QString {
    return m_impl->activeSession;
}

auto ChatController::draft() const -> QString {
    return m_impl->draft;
}

auto ChatController::refreshSessions() -> void {
    if (m_impl->store == nullptr) {
        return;
    }

    auto const sessions = m_impl->store->listSessions();
    if (!sessions) {
        return;
    }

    QList<SessionEntry> entries;
    entries.reserve(static_cast<qsizetype>(sessions.value().size()));
    for (auto const& session : sessions.value()) {
        SessionEntry entry;
        entry.id = QString::fromStdString(session.id);
        entry.title = QString::fromStdString(session.title);
        entry.updatedAt = session.updatedAt;
        entry.running = m_impl->inFlight.contains(entry.id);
        entries.append(entry);
    }

    m_sessions->replace(entries);
}

auto ChatController::refreshRunning() -> void {
    bool const running = !m_impl->activeSession.isEmpty() && m_impl->inFlight.contains(m_impl->activeSession);
    if (m_impl->running == running) {
        return;
    }

    m_impl->running = running;

    emit runningChanged();
}

auto ChatController::reloadConversation(QString const& id) -> void {
    m_conversation->clear();
    m_impl->streamingRow = -1;

    if (m_impl->store == nullptr) {
        return;
    }

    auto const messages = m_impl->store->loadMessages(id.toStdString());
    if (!messages) {
        return;
    }

    for (auto const& message : messages.value()) {
        m_conversation->append(blockFor(message));
    }
}

auto ChatController::setStatus(QString const& status) -> void {
    if (m_impl->status == status) {
        return;
    }

    m_impl->status = status;

    emit statusChanged();
}

auto ChatController::appendError(QString const& message) -> void {
    if (message.isEmpty()) {
        return;
    }

    m_conversation->append(ConversationBlock{ConversationModel::Error, message, {}});
}

auto ChatController::createSession() -> void {
    if (m_impl->store == nullptr) {
        return;
    }

    auto const session = m_impl->store->createSession(kDefaultSessionTitle);
    if (!session) {
        setStatus(QString::fromStdString(session.status().message()));
        return;
    }

    refreshSessions();
    selectSession(QString::fromStdString(session.value().id));
}

auto ChatController::deleteSession(QString const& id) -> void {
    if (m_impl->store == nullptr) {
        return;
    }

    if (auto const status = m_impl->store->deleteSession(id.toStdString()); !status) {
        setStatus(QString::fromStdString(status.message()));
        return;
    }

    if (auto it = m_impl->cancellations.find(id); it != m_impl->cancellations.end()) {
        if (it.value() != nullptr) {
            it.value()->cancel();
        }
        m_impl->cancellations.erase(it);
    }
    m_impl->inFlight.remove(id);
    m_impl->pending.remove(id);

    refreshSessions();

    if (m_impl->activeSession == id) {
        m_impl->activeSession.clear();
        m_impl->streamingRow = -1;
        m_conversation->clear();

        emit activeSessionChanged();

        if (m_sessions->rowCount() > 0) {
            selectSession(m_sessions->data(m_sessions->index(0), SessionListModel::SessionIdRole).toString());
        } else {
            m_impl->draft.clear();
            emit draftChanged();
        }
    }
}

auto ChatController::renameSession(QString const& id, QString const& title) -> void {
    if (m_impl->store == nullptr) {
        return;
    }

    if (auto const status = m_impl->store->renameSession(id.toStdString(), title.toStdString()); !status) {
        setStatus(QString::fromStdString(status.message()));
        return;
    }

    refreshSessions();
}

auto ChatController::maybeNameSession(QString const& id) -> void {
    if (m_impl->store == nullptr || m_impl->harness == nullptr) {
        return;
    }

    if (m_impl->namingCancellations.contains(id)) {
        return;
    }

    auto const session = m_impl->store->getSession(id.toStdString());
    if (!session || session.value().title != kDefaultSessionTitle) {
        return;
    }

    auto cancellation = std::make_shared<mts::runtime::Cancellation>();
    m_impl->namingCancellations.insert(id, cancellation);

    auto* harness = m_impl->harness.get();
    m_impl->pool->schedule([this, id, cancellation, harness] {
        auto const title = harness->suggestSessionTitle(id.toStdString(), *cancellation);
        QMetaObject::invokeMethod(this, [this, id, title] {
            m_impl->namingCancellations.remove(id);
            if (!title || m_impl->store == nullptr) {
                return;
            }

            auto const current = m_impl->store->getSession(id.toStdString());
            if (!current || current.value().title != kDefaultSessionTitle) {
                return;
            }

            renameSession(id, QString::fromStdString(title.value()));
        }, Qt::QueuedConnection);
    });
}

auto ChatController::clearHistory() -> void {
    if (m_impl->store == nullptr) {
        return;
    }

    auto const sessions = m_impl->store->listSessions();
    if (sessions) {
        for (auto const& session : sessions.value()) {
            static_cast<void>(m_impl->store->deleteSession(session.id));
        }
    }

    for (auto const& cancellation : std::as_const(m_impl->cancellations)) {
        if (cancellation != nullptr) {
            cancellation->cancel();
        }
    }
    m_impl->cancellations.clear();
    m_impl->pending.clear();
    m_impl->inFlight.clear();

    m_impl->activeSession.clear();
    m_impl->streamingRow = -1;
    m_conversation->clear();
    m_impl->draft.clear();

    emit activeSessionChanged();
    emit draftChanged();
    refreshSessions();
}

auto ChatController::selectSession(QString const& id) -> void {
    m_impl->activeSession = id;
    m_impl->streamingRow = -1;
    m_conversation->clear();

    emit activeSessionChanged();

    int persisted = 0;
    QString draft;
    if (m_impl->store != nullptr) {
        auto const messages = m_impl->store->loadMessages(id.toStdString());
        if (messages) {
            for (auto const& message : messages.value()) {
                m_conversation->append(blockFor(message));
                ++persisted;
            }
        }

        if (auto const session = m_impl->store->getSession(id.toStdString()); session) {
            draft = QString::fromStdString(session.value().draft);
        }
    }

    if (auto const it = m_impl->pending.constFind(id); it != m_impl->pending.constEnd()) {
        for (auto const& block : it->blocks) {
            m_conversation->append(block);
        }

        if (it->streamingIndex >= 0) {
            m_impl->streamingRow = persisted + it->streamingIndex;
        }
    }

    m_impl->draft = draft;

    refreshRunning();
    emit draftChanged();
}

auto ChatController::saveDraft(QString const& text) -> void {
    m_impl->draft = text;
    if (m_impl->store != nullptr && !m_impl->activeSession.isEmpty()) {
        static_cast<void>(m_impl->store->saveDraft(m_impl->activeSession.toStdString(), text.toStdString()));
    }
}

auto ChatController::sendMessage(QString const& text) -> void {
    if (m_impl->harness == nullptr || m_impl->inFlight.contains(m_impl->activeSession)) {
        return;
    }

    if (text.trimmed().isEmpty()) {
        return;
    }

    if (m_impl->activeSession.isEmpty()) {
        createSession();
    }

    if (m_impl->activeSession.isEmpty()) {
        return;
    }

    if (auto const status = m_impl->harness->submitUserMessage(
            m_impl->activeSession.toStdString(), text.toStdString());
        !status) {
        setStatus(QString::fromStdString(status.message()));
        return;
    }

    QString const id = m_impl->activeSession;
    auto cancellation = std::make_shared<mts::runtime::Cancellation>();
    m_impl->cancellations.insert(id, cancellation);
    m_impl->inFlight.insert(id);
    m_sessions->setRunning(id, true);
    saveDraft(QString());

    emit draftChanged();
    refreshRunning();
    m_impl->harness->run(id.toStdString(), cancellation);
}

auto ChatController::cancel() -> void {
    if (auto it = m_impl->cancellations.find(m_impl->activeSession); it != m_impl->cancellations.end() && it.value() != nullptr) {
        it.value()->cancel();
    }
}

auto ChatController::cancelAll() -> void {
    for (auto const& cancellation : std::as_const(m_impl->cancellations)) {
        if (cancellation != nullptr) {
            cancellation->cancel();
        }
    }
}

auto ChatController::hasInFlight() const -> bool {
    return !m_impl->inFlight.isEmpty();
}
