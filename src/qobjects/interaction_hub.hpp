#pragma once

#include <QObject>

class InteractionScope;

class InteractionHub : public QObject {
    Q_OBJECT

public:
    explicit InteractionHub(QObject* parent = nullptr);

    [[nodiscard]] static auto instance() -> InteractionHub*;

    auto notifyScopeCreated(InteractionScope* scope) -> void;
    auto notifyScopeChanged(InteractionScope* scope) -> void;
    auto notifyScopeDestroyed(InteractionScope* scope) -> void;

signals:
    auto scopeCreated(InteractionScope* scope) -> void;
    auto scopeChanged(InteractionScope* scope) -> void;
    auto scopeDestroyed(InteractionScope* scope) -> void;
};
