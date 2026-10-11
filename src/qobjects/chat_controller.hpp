#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "conversation_model.hpp"
#include "map_controller.hpp"
#include "session_list_model.hpp"

namespace mts::app {
struct AppContext;
}

namespace mts::orchestration {
class ConversationService;
}

namespace mts::interaction {
class CommandFacade;
}

class ConversationProjector;

class ChatController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(ChatController)
    QML_UNCREATABLE("ChatController is created by the application")
    Q_PROPERTY(SessionListModel* sessions READ sessions CONSTANT)
    Q_PROPERTY(ConversationModel* conversation READ conversation CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString activeSession READ activeSession NOTIFY activeSessionChanged)
    Q_PROPERTY(QString draft READ draft NOTIFY draftChanged)
    Q_PROPERTY(MapController* mapController READ mapController CONSTANT)

public:
    explicit ChatController(mts::app::AppContext const& context, QObject* parent = nullptr);
    ~ChatController() override;

    auto sessions() const -> SessionListModel*;
    auto conversation() const -> ConversationModel*;
    auto mapController() const -> MapController*;
    auto interactionCommands() const -> mts::interaction::CommandFacade*;
    auto running() const -> bool;
    auto status() const -> QString;
    auto activeSession() const -> QString;
    auto draft() const -> QString;

    Q_INVOKABLE auto createSession() -> void;
    Q_INVOKABLE auto selectSession(QString const& id) -> void;
    Q_INVOKABLE auto deleteSession(QString const& id) -> void;
    Q_INVOKABLE auto renameSession(QString const& id, QString const& title) -> void;
    Q_INVOKABLE auto clearHistory() -> void;
    Q_INVOKABLE auto sendMessage(QString const& text) -> void;
    Q_INVOKABLE auto saveDraft(QString const& text) -> void;
    Q_INVOKABLE auto cancel() -> void;
    Q_INVOKABLE auto cancelAll() -> void;
    Q_INVOKABLE auto hasInFlight() const -> bool;

signals:
    auto runningChanged() -> void;
    auto statusChanged() -> void;
    auto activeSessionChanged() -> void;
    auto draftChanged() -> void;

private:
    mts::orchestration::ConversationService* m_service = nullptr;
    std::unique_ptr<ConversationProjector> m_projector;
    SessionListModel* m_sessions;
    ConversationModel* m_conversation;
    MapController* m_mapController = nullptr;
    mts::interaction::CommandFacade* m_interactionCommands = nullptr;
};
