#include "conversation_projector.hpp"

#include <QList>
#include <QMetaObject>
#include <QString>

#include "conversation_model.hpp"
#include "session_list_model.hpp"

import std;
import events;
import orchestration;

namespace {

auto toBlockKind(mts::orchestration::BlockKind kind) -> int {
    switch (kind) {
        case mts::orchestration::BlockKind::UserText:
            return ConversationModel::UserText;

        case mts::orchestration::BlockKind::AssistantText:
            return ConversationModel::AssistantText;

        case mts::orchestration::BlockKind::Reasoning:
            return ConversationModel::Reasoning;

        case mts::orchestration::BlockKind::ToolCall:
            return ConversationModel::ToolCall;

        case mts::orchestration::BlockKind::ToolResult:
            return ConversationModel::ToolResult;

        case mts::orchestration::BlockKind::Error:
            return ConversationModel::Error;
    }

    return ConversationModel::AssistantText;
}

auto toBlockKind(mts::events::MessageKind kind) -> int {
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

auto toQtBlock(mts::orchestration::Block const& block) -> ConversationBlock {
    return ConversationBlock{
        toBlockKind(block.kind),
        QString::fromStdString(block.text),
        QString::fromStdString(block.payload),
    };
}

} // END OF NAMESPACE anonymous

ConversationProjector::ConversationProjector(
    mts::events::EventBus& bus,
    mts::orchestration::ConversationService& service,
    SessionListModel* sessions,
    ConversationModel* conversation,
    QObject* parent
)
    : QObject(parent)
    , m_bus(&bus)
    , m_service(&service)
    , m_sessions(sessions)
    , m_conversation(conversation) {
    m_runStartedId = m_bus->subscribe<mts::events::RunStarted>([this](mts::events::RunStarted const& event) {
        QString const id = QString::fromStdString(event.session);
        QMetaObject::invokeMethod(this, [this, id] {
            m_service->runStarted(id.toStdString());
            m_sessions->setRunning(id, true);
            syncState();
        }, Qt::QueuedConnection);
    });

    m_runFinishedId = m_bus->subscribe<mts::events::RunFinished>([this](mts::events::RunFinished const& event) {
        QString const id = QString::fromStdString(event.session);
        QMetaObject::invokeMethod(this, [this, id] {
            m_service->runFinished(id.toStdString());
            m_sessions->setRunning(id, false);
            if (id == QString::fromStdString(m_service->activeSession())) {
                reloadConversation();
            }

            syncState();
            refreshSessions();
        }, Qt::QueuedConnection);
    });

    m_sessionUpdatedId = m_bus->subscribe<mts::events::SessionUpdated>([this](mts::events::SessionUpdated const&) {
        QMetaObject::invokeMethod(this, [this] {
            refreshSessions();
            syncState();
        }, Qt::QueuedConnection);
    });

    m_runFailedId = m_bus->subscribe<mts::events::RunFailed>([this](mts::events::RunFailed const& event) {
        QString const id = QString::fromStdString(event.session);
        QString const message = QString::fromStdString(event.message);
        QMetaObject::invokeMethod(this, [this, id, message] {
            m_service->runFailed(id.toStdString());
            m_sessions->setRunning(id, false);
            if (id == QString::fromStdString(m_service->activeSession())) {
                m_streamingRow = -1;
                if (!message.isEmpty()) {
                    m_conversation->append(ConversationBlock{ConversationModel::Error, message, {}});
                }
            }

            syncState();
            refreshSessions();
        }, Qt::QueuedConnection);
    });

    m_tokenDeltaId = m_bus->subscribe<mts::events::TokenDelta>([this](mts::events::TokenDelta const& event) {
        QString const id = QString::fromStdString(event.session);
        QString const delta = QString::fromStdString(event.delta);
        QMetaObject::invokeMethod(this, [this, id, delta] {
            m_service->tokenDelta(id.toStdString(), delta.toStdString());
            if (id != QString::fromStdString(m_service->activeSession())) {
                return;
            }

            if (m_streamingRow < 0) {
                m_conversation->append(ConversationBlock{ConversationModel::AssistantText, {}, {}});
                m_streamingRow = m_conversation->lastRow();
            }
            m_conversation->appendText(m_streamingRow, delta);
        }, Qt::QueuedConnection);
    });

    m_messageAddedId = m_bus->subscribe<mts::events::MessageAdded>([this](mts::events::MessageAdded const& event) {
        QString const id = QString::fromStdString(event.session);
        int const kind = toBlockKind(event.kind);
        QString const text = QString::fromStdString(event.text);
        QMetaObject::invokeMethod(this, [this, id, kind, text] {
            if (id != QString::fromStdString(m_service->activeSession())) {
                return;
            }

            if (kind == ConversationModel::AssistantText && m_streamingRow >= 0) {
                m_conversation->setText(m_streamingRow, text);
                m_streamingRow = -1;
            } else {
                m_conversation->append(ConversationBlock{kind, text, {}});
            }
        }, Qt::QueuedConnection);
    });

    m_toolCallStartedId = m_bus->subscribe<mts::events::ToolCallStarted>([this](mts::events::ToolCallStarted const& event) {
        QString const id = QString::fromStdString(event.session);
        QString const name = QString::fromStdString(event.name);
        QString const args = QString::fromStdString(event.arguments);
        QMetaObject::invokeMethod(this, [this, id, name, args] {
            if (id != QString::fromStdString(m_service->activeSession())) {
                return;
            }

            m_streamingRow = -1;
            m_conversation->append(ConversationBlock{ConversationModel::ToolCall, name, args});
        }, Qt::QueuedConnection);
    });

    m_toolCallFinishedId = m_bus->subscribe<mts::events::ToolCallFinished>([this](mts::events::ToolCallFinished const& event) {
        QString const id = QString::fromStdString(event.session);
        QString const name = QString::fromStdString(event.name);
        QString const result = QString::fromStdString(event.result);
        QMetaObject::invokeMethod(this, [this, id, name, result] {
            if (id != QString::fromStdString(m_service->activeSession())) {
                return;
            }

            m_conversation->append(ConversationBlock{ConversationModel::ToolResult, name, result});
        }, Qt::QueuedConnection);
    });
}

ConversationProjector::~ConversationProjector() {
    m_bus->unsubscribe<mts::events::RunStarted>(m_runStartedId);
    m_bus->unsubscribe<mts::events::RunFinished>(m_runFinishedId);
    m_bus->unsubscribe<mts::events::RunFailed>(m_runFailedId);
    m_bus->unsubscribe<mts::events::TokenDelta>(m_tokenDeltaId);
    m_bus->unsubscribe<mts::events::MessageAdded>(m_messageAddedId);
    m_bus->unsubscribe<mts::events::ToolCallStarted>(m_toolCallStartedId);
    m_bus->unsubscribe<mts::events::ToolCallFinished>(m_toolCallFinishedId);
    m_bus->unsubscribe<mts::events::SessionUpdated>(m_sessionUpdatedId);
}

auto ConversationProjector::refreshSessions() -> void {
    auto const sessions = m_service->listSessions();
    QList<SessionEntry> entries;
    entries.reserve(static_cast<qsizetype>(sessions.size()));
    for (auto const& session : sessions) {
        entries.append(SessionEntry{
            QString::fromStdString(session.id),
            QString::fromStdString(session.title),
            session.updatedAt,
            session.running,
        });
    }

    m_sessions->replace(entries);
}

auto ConversationProjector::reloadConversation() -> void {
    m_conversation->clear();
    m_streamingRow = -1;

    std::string const& active = m_service->activeSession();
    auto const stored = m_service->storedBlocks(active);
    for (auto const& block : stored) {
        m_conversation->append(toQtBlock(block));
    }

    if (auto const* pending = m_service->pendingRun(active); pending != nullptr) {
        for (auto const& block : pending->blocks) {
            m_conversation->append(toQtBlock(block));
        }

        if (pending->streamingIndex >= 0) {
            m_streamingRow = static_cast<int>(stored.size()) + pending->streamingIndex;
        }
    }
}

auto ConversationProjector::updateDraft() -> void {
    m_draft = QString::fromStdString(m_service->draft());
}

auto ConversationProjector::syncState() -> void {
    bool const running = m_service->running();
    if (running != m_running) {
        m_running = running;
        emit runningChanged();
    }

    QString const status = QString::fromStdString(m_service->status());
    if (status != m_status) {
        m_status = status;
        emit statusChanged();
    }

    QString const active = QString::fromStdString(m_service->activeSession());
    if (active != m_activeSession) {
        m_activeSession = active;
        emit activeSessionChanged();
    }

    QString const draft = QString::fromStdString(m_service->draft());
    if (draft != m_draft) {
        m_draft = draft;
        emit draftChanged();
    }
}

auto ConversationProjector::running() const -> bool {
    return m_running;
}

auto ConversationProjector::status() const -> QString {
    return m_status;
}

auto ConversationProjector::activeSession() const -> QString {
    return m_activeSession;
}

auto ConversationProjector::draft() const -> QString {
    return m_draft;
}
