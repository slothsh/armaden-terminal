#pragma once

#include <QHash>
#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace mts::interaction {
class TargetRegistry;
}

class InteractionScope;

class InteractionController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(InteractionController)
    QML_UNCREATABLE("InteractionController is owned by the Application")
    Q_PROPERTY(int targetCount READ targetCount NOTIFY registryChanged)

public:
    explicit InteractionController(QObject* parent = nullptr);
    ~InteractionController() override;

    [[nodiscard]] auto targetCount() const -> int;
    [[nodiscard]] auto registry() -> mts::interaction::TargetRegistry&;

signals:
    auto registryChanged() -> void;

private:
    auto onScopeCreated(InteractionScope* scope) -> void;
    auto onScopeChanged(InteractionScope* scope) -> void;
    auto onScopeDestroyed(InteractionScope* scope) -> void;

    std::unique_ptr<mts::interaction::TargetRegistry> m_registry;
    QHash<InteractionScope*, quint64> m_bindings;
};
