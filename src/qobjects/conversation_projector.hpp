#pragma once

#include <QObject>
#include <QString>

#include <cstddef>

#include "conversation_model.hpp"
#include "session_list_model.hpp"

namespace mts::events {
class EventBus;
}

namespace mts::orchestration {
class ConversationService;
}

class ConversationProjector : public QObject {
    Q_OBJECT

public:
    ConversationProjector(
        mts::events::EventBus& bus,
        mts::orchestration::ConversationService& service,
        SessionListModel* sessions,
        ConversationModel* conversation,
        QObject* parent = nullptr
    );
    ~ConversationProjector() override;

    ConversationProjector(ConversationProjector const&) = delete;
    auto operator=(ConversationProjector const&) -> ConversationProjector& = delete;

    auto refreshSessions() -> void;
    auto reloadConversation() -> void;
    auto syncState() -> void;
    auto updateDraft() -> void;

    [[nodiscard]] auto running() const -> bool;
    [[nodiscard]] auto status() const -> QString;
    [[nodiscard]] auto activeSession() const -> QString;
    [[nodiscard]] auto draft() const -> QString;

signals:
    auto runningChanged() -> void;
    auto statusChanged() -> void;
    auto activeSessionChanged() -> void;
    auto draftChanged() -> void;

private:
    mts::events::EventBus* m_bus;
    mts::orchestration::ConversationService* m_service;
    SessionListModel* m_sessions;
    ConversationModel* m_conversation;
    int m_streamingRow = -1;
    bool m_running = false;
    QString m_status;
    QString m_activeSession;
    QString m_draft;
    std::size_t m_runStartedId = 0;
    std::size_t m_runFinishedId = 0;
    std::size_t m_runFailedId = 0;
    std::size_t m_tokenDeltaId = 0;
    std::size_t m_messageAddedId = 0;
    std::size_t m_toolCallStartedId = 0;
    std::size_t m_toolCallFinishedId = 0;
    std::size_t m_sessionUpdatedId = 0;
};
