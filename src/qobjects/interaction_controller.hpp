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

namespace mts::events {
class EventBus;
}

namespace mts::interaction {
class TargetRegistry;
class ContextStore;
class SelectionModel;
class TraceRing;
class LayerStack;
struct ContextChange;
struct SelectionChange;
struct LayerChange;
struct TargetId;
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
    Q_PROPERTY(int layerDepth READ layerDepth NOTIFY layersChanged)
    Q_PROPERTY(quint64 topLayerId READ topLayerId NOTIFY layersChanged)

public:
    explicit InteractionController(QObject* parent = nullptr);
    ~InteractionController() override;

    auto setEventBus(mts::events::EventBus* bus) -> void;

    [[nodiscard]] auto targetCount() const -> int;
    [[nodiscard]] auto focusedTarget() const -> quint64;
    [[nodiscard]] auto currentTarget() const -> quint64;
    [[nodiscard]] auto hoveredTarget() const -> quint64;
    [[nodiscard]] auto layerDepth() const -> int;
    [[nodiscard]] auto topLayerId() const -> quint64;

    [[nodiscard]] auto registry() -> mts::interaction::TargetRegistry&;
    [[nodiscard]] auto context() -> mts::interaction::ContextStore&;
    [[nodiscard]] auto selection() -> mts::interaction::SelectionModel&;
    [[nodiscard]] auto trace() -> mts::interaction::TraceRing&;
    [[nodiscard]] auto layers() -> mts::interaction::LayerStack&;

    Q_INVOKABLE auto pushLayer(
        QObject* object,
        QString const& kind,
        QString const& domain,
        quint64 ownerTarget,
        bool dismissOnEscape,
        bool dismissOnOutsideClick,
        bool focusTrap
    ) -> quint64;
    Q_INVOKABLE auto popLayer(quint64 id) -> void;
    Q_INVOKABLE auto positionLayer(QObject* object, qreal x, qreal y) -> void;
    Q_INVOKABLE auto dismissTopLayer() -> bool;
    Q_INVOKABLE auto consumeOutsideDismissal(QObject* object) -> bool;
    auto dismissTopLayerOutside() -> bool;

signals:
    auto registryChanged() -> void;
    auto contextChanged() -> void;
    auto layersChanged() -> void;

protected:
    auto eventFilter(QObject* watched, QEvent* event) -> bool override;

private:
    auto onScopeCreated(InteractionScope* scope) -> void;
    auto onScopeChanged(InteractionScope* scope) -> void;
    auto onScopeDestroyed(InteractionScope* scope) -> void;
    auto attachWindow(QQuickWindow* window) -> void;
    auto syncWindowEvent(QQuickWindow* window, QEvent* event) -> bool;
    auto resolveFocus(QQuickWindow* window) -> void;
    [[nodiscard]] auto scopeForItem(QQuickItem const* item) const -> InteractionScope*;
    [[nodiscard]] auto hitTestTarget(QQuickWindow* window, QPointF const& scenePosition) const -> quint64;
    auto recordContextChange(mts::interaction::ContextChange const& change) -> void;
    auto recordSelectionChange(mts::interaction::SelectionChange const& change) -> void;
    auto recordLayerChange(mts::interaction::LayerChange const& change) -> void;
    auto restoreFocus(mts::interaction::TargetId target) -> void;
    auto closeLayer(quint64 id) -> bool;
    auto closeLayerOutside(quint64 id) -> bool;

    mts::events::EventBus* m_bus = nullptr;
    std::unique_ptr<mts::interaction::TargetRegistry> m_registry;
    std::unique_ptr<mts::interaction::ContextStore> m_context;
    std::unique_ptr<mts::interaction::SelectionModel> m_selection;
    std::unique_ptr<mts::interaction::TraceRing> m_trace;
    std::unique_ptr<mts::interaction::LayerStack> m_layers;
    QHash<quint64, QPointer<QObject>> m_layerObjects;
    QPointer<QObject> m_outsideDismissedObject;
    QHash<InteractionScope*, quint64> m_bindings;
    QHash<QObject*, InteractionScope*> m_ownerIndex;
    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_focusConnection;
    std::size_t m_contextSubscription = 0;
    std::size_t m_selectionSubscription = 0;
    std::size_t m_layerSubscription = 0;
};
