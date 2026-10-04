#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "conversation_model.hpp"
#include "session_list_model.hpp"

class ChatController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(ChatController)
    Q_PROPERTY(SessionListModel* sessions READ sessions CONSTANT)
    Q_PROPERTY(ConversationModel* conversation READ conversation CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString activeSession READ activeSession NOTIFY activeSessionChanged)
    Q_PROPERTY(QString draft READ draft NOTIFY draftChanged)

public:
    explicit ChatController(QObject* parent = nullptr);
    ~ChatController() override;

    SessionListModel* sessions() const;
    ConversationModel* conversation() const;
    bool running() const;
    QString status() const;
    QString activeSession() const;
    QString draft() const;

    Q_INVOKABLE void createSession();
    Q_INVOKABLE void selectSession(QString const& id);
    Q_INVOKABLE void deleteSession(QString const& id);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void sendMessage(QString const& text);
    Q_INVOKABLE void saveDraft(QString const& text);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void cancelAll();
    Q_INVOKABLE bool hasInFlight() const;

signals:
    void runningChanged();
    void statusChanged();
    void activeSessionChanged();
    void draftChanged();

private:
    void refreshSessions();
    void refreshRunning();
    void reloadConversation(QString const& id);
    void setStatus(QString const& status);
    void appendError(QString const& message);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    SessionListModel* m_sessions;
    ConversationModel* m_conversation;
};
