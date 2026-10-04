#include "chat_controller.hpp"

#include "conversation_model.hpp"
#include "session_list_model.hpp"

#include <QMetaObject>
#include <QString>

import std;
import runtime;
import events;
import config;
import persistence;
import provider;
import harness;

namespace {

int blockKindFor(mts::persistence::MessageKind kind) {
    switch (kind) {
        case mts::persistence::MessageKind::UserText: return ConversationModel::UserText;
        case mts::persistence::MessageKind::AssistantText: return ConversationModel::AssistantText;
        case mts::persistence::MessageKind::Reasoning: return ConversationModel::Reasoning;
        case mts::persistence::MessageKind::ToolCall: return ConversationModel::ToolCall;
        case mts::persistence::MessageKind::ToolResult: return ConversationModel::ToolResult;
        case mts::persistence::MessageKind::Error: return ConversationModel::Error;
    }
    return ConversationModel::AssistantText;
}

int blockKindForEvent(mts::events::MessageKind kind) {
    switch (kind) {
        case mts::events::MessageKind::UserText: return ConversationModel::UserText;
        case mts::events::MessageKind::AssistantText: return ConversationModel::AssistantText;
        case mts::events::MessageKind::Reasoning: return ConversationModel::Reasoning;
        case mts::events::MessageKind::Error: return ConversationModel::Error;
    }
    return ConversationModel::AssistantText;
}

ConversationBlock blockFor(mts::persistence::Message const& message) {
    ConversationBlock block;
    block.kind = blockKindFor(message.kind);
    block.text = QString::fromStdString(message.text);
    block.payload = QString::fromStdString(message.payload);
    return block;
}

}

struct ChatController::Impl {
    std::unique_ptr<mts::runtime::ThreadPool> pool;
    mts::events::EventBus bus;
    std::unique_ptr<mts::persistence::SessionStoreHandle> store;
    std::unique_ptr<mts::model::ProviderHandle> provider;
    std::unique_ptr<mts::harness::AgentHarness> harness;
    std::shared_ptr<mts::runtime::Cancellation> cancellation;
    QString activeSession;
    QString draft;
    QString status;
    int streamingRow = -1;
    bool running = false;
};

ChatController::ChatController(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
    , m_sessions(new SessionListModel(this))
    , m_conversation(new ConversationModel(this)) {
    std::string configError;
    auto const config = mts::config::loadConfigOrDefaults(mts::config::configPath(), configError);
    if (!configError.empty())
        setStatus(QString::fromStdString(configError));
    else if (config.provider.model.empty() || config.provider.apiKey.empty())
        setStatus(QStringLiteral("No model configured; using the offline provider. Create config/mindtosis.json."));

    auto const workers = config.runtime.workers == 0 ? 4u : config.runtime.workers;
    m_impl->pool = std::make_unique<mts::runtime::ThreadPool>(workers);

    auto store = mts::persistence::makeSessionStore(mts::persistence::databasePath());
    if (store)
        m_impl->store = std::make_unique<mts::persistence::SessionStoreHandle>(std::move(store.value()));
    else
        setStatus(QString::fromStdString(store.status().message()));

    auto provider = mts::model::makeProvider(config);
    if (provider)
        m_impl->provider = std::make_unique<mts::model::ProviderHandle>(std::move(provider.value()));
    else
        setStatus(QString::fromStdString(provider.status().message()));

    if (m_impl->store != nullptr && m_impl->provider != nullptr) {
        m_impl->harness = std::make_unique<mts::harness::AgentHarness>(
            *m_impl->provider, *m_impl->store, *m_impl->pool, m_impl->bus
        );

        (void)m_impl->bus.subscribe<mts::events::RunStarted>([this](mts::events::RunStarted const&) {
            QMetaObject::invokeMethod(this, [this] { setRunning(true); }, Qt::QueuedConnection);
        });
        (void)m_impl->bus.subscribe<mts::events::RunFinished>([this](mts::events::RunFinished const&) {
            QMetaObject::invokeMethod(this, [this] {
                m_impl->streamingRow = -1;
                setRunning(false);
                refreshSessions();
            }, Qt::QueuedConnection);
        });
        (void)m_impl->bus.subscribe<mts::events::RunFailed>([this](mts::events::RunFailed const& event) {
            QString const message = QString::fromStdString(event.message);
            QMetaObject::invokeMethod(this, [this, message] {
                m_impl->streamingRow = -1;
                setRunning(false);
                appendError(message);
            }, Qt::QueuedConnection);
        });
        (void)m_impl->bus.subscribe<mts::events::TokenDelta>([this](mts::events::TokenDelta const& event) {
            QString const delta = QString::fromStdString(event.delta);
            QMetaObject::invokeMethod(this, [this, delta] {
                if (m_impl->streamingRow < 0) {
                    m_conversation->append(ConversationBlock{ConversationModel::AssistantText, {}, {}});
                    m_impl->streamingRow = m_conversation->lastRow();
                }
                m_conversation->appendText(m_impl->streamingRow, delta);
            }, Qt::QueuedConnection);
        });
        (void)m_impl->bus.subscribe<mts::events::MessageAdded>([this](mts::events::MessageAdded const& event) {
            int const kind = blockKindForEvent(event.kind);
            QString const text = QString::fromStdString(event.text);
            QMetaObject::invokeMethod(this, [this, kind, text] {
                if (kind == ConversationModel::AssistantText && m_impl->streamingRow >= 0) {
                    m_conversation->setText(m_impl->streamingRow, text);
                    m_impl->streamingRow = -1;
                } else {
                    m_conversation->append(ConversationBlock{kind, text, {}});
                }
            }, Qt::QueuedConnection);
        });
        (void)m_impl->bus.subscribe<mts::events::ToolCallStarted>([this](mts::events::ToolCallStarted const& event) {
            QString const name = QString::fromStdString(event.name);
            QString const args = QString::fromStdString(event.arguments);
            QMetaObject::invokeMethod(this, [this, name, args] {
                m_impl->streamingRow = -1;
                m_conversation->append(ConversationBlock{ConversationModel::ToolCall, name, args});
            }, Qt::QueuedConnection);
        });
        (void)m_impl->bus.subscribe<mts::events::ToolCallFinished>([this](mts::events::ToolCallFinished const& event) {
            QString const name = QString::fromStdString(event.name);
            QString const result = QString::fromStdString(event.result);
            QMetaObject::invokeMethod(this, [this, name, result] {
                m_conversation->append(ConversationBlock{ConversationModel::ToolResult, name, result});
            }, Qt::QueuedConnection);
        });
    }

    refreshSessions();
    if (m_sessions->rowCount() > 0) {
        auto const first = m_sessions->index(0);
        if (first.isValid())
            selectSession(m_sessions->data(first, SessionListModel::SessionIdRole).toString());
    }
}

ChatController::~ChatController() = default;

SessionListModel* ChatController::sessions() const {
    return m_sessions;
}

ConversationModel* ChatController::conversation() const {
    return m_conversation;
}

bool ChatController::running() const {
    return m_impl->running;
}

QString ChatController::status() const {
    return m_impl->status;
}

QString ChatController::activeSession() const {
    return m_impl->activeSession;
}

QString ChatController::draft() const {
    return m_impl->draft;
}

void ChatController::refreshSessions() {
    if (m_impl->store == nullptr)
        return;

    auto const sessions = m_impl->store->listSessions();
    if (!sessions)
        return;

    QList<SessionEntry> entries;
    entries.reserve(static_cast<qsizetype>(sessions.value().size()));
    for (auto const& session : sessions.value()) {
        SessionEntry entry;
        entry.id = QString::fromStdString(session.id);
        entry.title = QString::fromStdString(session.title);
        entry.updatedAt = session.updatedAt;
        entries.append(entry);
    }
    m_sessions->replace(entries);
}

void ChatController::setRunning(bool running) {
    if (m_impl->running == running)
        return;
    m_impl->running = running;
    emit runningChanged();
}

void ChatController::setStatus(QString const& status) {
    if (m_impl->status == status)
        return;
    m_impl->status = status;
    emit statusChanged();
}

void ChatController::appendError(QString const& message) {
    if (message.isEmpty())
        return;
    m_conversation->append(ConversationBlock{ConversationModel::Error, message, {}});
}

void ChatController::createSession() {
    if (m_impl->store == nullptr)
        return;

    auto const session = m_impl->store->createSession("New session");
    if (!session) {
        setStatus(QString::fromStdString(session.status().message()));
        return;
    }

    refreshSessions();
    selectSession(QString::fromStdString(session.value().id));
}

void ChatController::deleteSession(QString const& id) {
    if (m_impl->store == nullptr)
        return;

    if (auto const status = m_impl->store->deleteSession(id.toStdString()); !status) {
        setStatus(QString::fromStdString(status.message()));
        return;
    }

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

void ChatController::clearHistory() {
    if (m_impl->store == nullptr)
        return;

    auto const sessions = m_impl->store->listSessions();
    if (sessions) {
        for (auto const& session : sessions.value())
            (void)m_impl->store->deleteSession(session.id);
    }

    m_impl->activeSession.clear();
    m_impl->streamingRow = -1;
    m_conversation->clear();
    m_impl->draft.clear();
    emit activeSessionChanged();
    emit draftChanged();
    refreshSessions();
}

void ChatController::selectSession(QString const& id) {
    m_impl->activeSession = id;
    m_impl->streamingRow = -1;
    m_conversation->clear();
    emit activeSessionChanged();

    QString draft;
    if (m_impl->store != nullptr) {
        auto const messages = m_impl->store->loadMessages(id.toStdString());
        if (messages) {
            for (auto const& message : messages.value())
                m_conversation->append(blockFor(message));
        }

        if (auto const session = m_impl->store->getSession(id.toStdString()); session)
            draft = QString::fromStdString(session.value().draft);
    }

    m_impl->draft = draft;
    emit draftChanged();
}

void ChatController::saveDraft(QString const& text) {
    m_impl->draft = text;
    if (m_impl->store != nullptr && !m_impl->activeSession.isEmpty())
        (void)m_impl->store->saveDraft(m_impl->activeSession.toStdString(), text.toStdString());
}

void ChatController::sendMessage(QString const& text) {
    if (m_impl->harness == nullptr || m_impl->running)
        return;
    if (text.trimmed().isEmpty())
        return;
    if (m_impl->activeSession.isEmpty())
        createSession();
    if (m_impl->activeSession.isEmpty())
        return;

    if (auto const status = m_impl->harness->submitUserMessage(
            m_impl->activeSession.toStdString(), text.toStdString());
        !status) {
        setStatus(QString::fromStdString(status.message()));
        return;
    }

    m_impl->cancellation = std::make_shared<mts::runtime::Cancellation>();
    saveDraft(QString());
    emit draftChanged();
    setRunning(true);
    m_impl->harness->run(m_impl->activeSession.toStdString(), m_impl->cancellation);
}

void ChatController::cancel() {
    if (m_impl->cancellation != nullptr)
        m_impl->cancellation->cancel();
}
