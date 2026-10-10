#pragma once

#include <QEvent>
#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

#include <cstddef>
#include <memory>

namespace mts::interaction {
class TargetRegistry;
class ContextStore;
}

class InteractionScope;

class InteractionController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(InteractionController)
    QML_UNCREATABLE("InteractionController is owned by the Application")
    Q_PROPERTY(int targetCount READ targetCount NOTIFY registryChanged)
    Q_PROPERTY(quint64 focusedTarget READ focusedTarget NOTIFY contextChanged)
    Q_PROPERTY(quint64 currentTarget READ currentTarget NOTIFY contextChanged)
    Q_PROPERTY(quint64 hoveredTarget READ hoveredTarget NOTIFY contextChanged)

public:
    explicit InteractionController(QObject* parent = nullptr);
    ~InteractionController() override;

    [[nodiscard]] auto targetCount() const -> int;
    [[nodiscard]] auto focusedTarget() const -> quint64;
    [[nodiscard]] auto currentTarget() const -> quint64;
    [[nodiscard]] auto hoveredTarget() const -> quint64;

    [[nodiscard]] auto registry() -> mts::interaction::TargetRegistry&;
    [[nodiscard]] auto context() -> mts::interaction::ContextStore&;

signals:
    auto registryChanged() -> void;
    auto contextChanged() -> void;

protected:
    auto eventFilter(QObject* watched, QEvent* event) -> bool override;

private:
    auto onScopeCreated(InteractionScope* scope) -> void;
    auto onScopeChanged(InteractionScope* scope) -> void;
    auto onScopeDestroyed(InteractionScope* scope) -> void;
    auto attachWindow(QQuickWindow* window) -> void;
    auto syncWindowEvent(QQuickWindow* window, QEvent* event) -> void;
    auto resolveFocus(QQuickWindow* window) -> void;
    [[nodiscard]] auto scopeForItem(QQuickItem const* item) const -> InteractionScope*;
    [[nodiscard]] auto hitTestTarget(QQuickWindow* window, QPointF const& scenePosition) const -> quint64;

    std::unique_ptr<mts::interaction::TargetRegistry> m_registry;
    std::unique_ptr<mts::interaction::ContextStore> m_context;
    QHash<InteractionScope*, quint64> m_bindings;
    QHash<QObject*, InteractionScope*> m_ownerIndex;
    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_focusConnection;
    std::size_t m_contextSubscription = 0;
};
