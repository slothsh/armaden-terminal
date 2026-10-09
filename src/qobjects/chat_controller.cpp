#include "chat_controller.hpp"

#include "app_context.hpp"
#include "conversation_projector.hpp"

#include <QString>

import std;
import orchestration;

ChatController::ChatController(mts::app::AppContext const& context, QObject* parent)
    : QObject(parent)
    , m_service(&context.conversation)
    , m_sessions(new SessionListModel(this))
    , m_conversation(new ConversationModel(this))
    , m_mapController(&context.mapController) {
    m_projector = std::make_unique<ConversationProjector>(
        context.bus,
        *m_service,
        m_sessions,
        m_conversation,
        this
    );

    QObject::connect(m_projector.get(), &ConversationProjector::runningChanged, this, &ChatController::runningChanged);
    QObject::connect(m_projector.get(), &ConversationProjector::statusChanged, this, &ChatController::statusChanged);
    QObject::connect(m_projector.get(), &ConversationProjector::activeSessionChanged, this, &ChatController::activeSessionChanged);
    QObject::connect(m_projector.get(), &ConversationProjector::draftChanged, this, &ChatController::draftChanged);

    m_projector->syncState();
    m_projector->refreshSessions();
    if (m_sessions->rowCount() > 0) {
        auto const first = m_sessions->index(0);
        if (first.isValid()) {
            this->selectSession(m_sessions->data(first, SessionListModel::SessionIdRole).toString());
        }
    }
}

ChatController::~ChatController() {
    if (m_service != nullptr) {
        m_service->shutdown();
    }
}

auto ChatController::sessions() const -> SessionListModel* {
    return this->m_sessions;
}

auto ChatController::conversation() const -> ConversationModel* {
    return this->m_conversation;
}

auto ChatController::mapController() const -> MapController* {
    return this->m_mapController;
}

auto ChatController::running() const -> bool {
    return this->m_projector->running();
}

auto ChatController::status() const -> QString {
    return this->m_projector->status();
}

auto ChatController::activeSession() const -> QString {
    return this->m_projector->activeSession();
}

auto ChatController::draft() const -> QString {
    return this->m_projector->draft();
}

auto ChatController::createSession() -> void {
    static_cast<void>(m_service->createSession());
    m_projector->refreshSessions();
    m_projector->reloadConversation();
    m_projector->syncState();
}

auto ChatController::selectSession(QString const& id) -> void {
    m_service->selectSession(id.toStdString());
    m_projector->reloadConversation();
    m_projector->syncState();
}

auto ChatController::deleteSession(QString const& id) -> void {
    static_cast<void>(m_service->deleteSession(id.toStdString()));
    m_projector->refreshSessions();
    m_projector->reloadConversation();
    m_projector->syncState();
}

auto ChatController::renameSession(QString const& id, QString const& title) -> void {
    static_cast<void>(m_service->renameSession(id.toStdString(), title.toStdString()));
    m_projector->refreshSessions();
    m_projector->syncState();
}

auto ChatController::clearHistory() -> void {
    m_service->clearHistory();
    m_projector->refreshSessions();
    m_projector->reloadConversation();
    m_projector->syncState();
}

auto ChatController::sendMessage(QString const& text) -> void {
    static_cast<void>(m_service->submit(text.toStdString()));
    m_projector->refreshSessions();
    m_projector->syncState();
}

auto ChatController::saveDraft(QString const& text) -> void {
    m_service->saveDraft(text.toStdString());
    m_projector->updateDraft();
}

auto ChatController::cancel() -> void {
    m_service->cancel();
}

auto ChatController::cancelAll() -> void {
    m_service->cancelAll();
}

auto ChatController::hasInFlight() const -> bool {
    return m_service->hasInFlight();
}
