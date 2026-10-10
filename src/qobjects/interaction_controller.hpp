#pragma once

#include <QEvent>
#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QVariantList>
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
class ActionRegistry;
class NavigationModel;
class KeyRouter;
class CommandFacade;
struct CommandRequest;
struct CommandResult;
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
    Q_PROPERTY(QString focusedLabel READ focusedLabel NOTIFY contextChanged)
    Q_PROPERTY(QString focusedDomain READ focusedDomain NOTIFY contextChanged)
    Q_PROPERTY(quint64 currentTarget READ currentTarget NOTIFY contextChanged)
    Q_PROPERTY(quint64 hoveredTarget READ hoveredTarget NOTIFY contextChanged)
    Q_PROPERTY(int layerDepth READ layerDepth NOTIFY layersChanged)
    Q_PROPERTY(quint64 topLayerId READ topLayerId NOTIFY layersChanged)
    Q_PROPERTY(int actionRevision READ actionRevision NOTIFY actionsChanged)
    Q_PROPERTY(QVariantMap lastCommandResult READ lastCommandResult NOTIFY commandResultChanged)

public:
    explicit InteractionController(QObject* parent = nullptr);
    ~InteractionController() override;

    auto setEventBus(mts::events::EventBus* bus) -> void;

    [[nodiscard]] auto targetCount() const -> int;
    [[nodiscard]] auto focusedTarget() const -> quint64;
    [[nodiscard]] auto focusedLabel() const -> QString;
    [[nodiscard]] auto focusedDomain() const -> QString;
    [[nodiscard]] auto currentTarget() const -> quint64;
    [[nodiscard]] auto hoveredTarget() const -> quint64;
    [[nodiscard]] auto layerDepth() const -> int;
    [[nodiscard]] auto topLayerId() const -> quint64;
    [[nodiscard]] auto actionRevision() const -> int;

    [[nodiscard]] auto registry() -> mts::interaction::TargetRegistry&;
    [[nodiscard]] auto context() -> mts::interaction::ContextStore&;
    [[nodiscard]] auto selection() -> mts::interaction::SelectionModel&;
    [[nodiscard]] auto trace() -> mts::interaction::TraceRing&;
    [[nodiscard]] auto layers() -> mts::interaction::LayerStack&;
    [[nodiscard]] auto actions() -> mts::interaction::ActionRegistry&;
    [[nodiscard]] auto navigation() -> mts::interaction::NavigationModel&;
    [[nodiscard]] auto keyRouter() -> mts::interaction::KeyRouter&;
    [[nodiscard]] auto commands() -> mts::interaction::CommandFacade&;
    [[nodiscard]] auto lastCommandResult() const -> QVariantMap;

    Q_INVOKABLE auto targetForDomain(QString const& domain, QString const& role) const -> quint64;
    Q_INVOKABLE auto targetSemanticKey(quint64 target) const -> QString;
    Q_INVOKABLE auto actionsFor(quint64 target, QString const& surface) const -> QVariantList;
    Q_INVOKABLE auto actionEnabled(
        QString const& actionId,
        quint64 target,
        QString const& surface = QStringLiteral("button")
    ) const -> bool;
    Q_INVOKABLE auto invokeAction(
        QString const& actionId,
        quint64 target,
        QVariantMap const& arguments = {}
    ) -> bool;
    Q_INVOKABLE auto setBusinessValue(QString const& key, QString const& value) -> void;
    Q_INVOKABLE auto executeCommand(QVariantMap const& request) -> QVariantMap;
    Q_INVOKABLE auto selectionCount(QString const& domain) const -> int;
    Q_INVOKABLE auto selectedTargets(QString const& domain) const -> QVariantList;
    Q_INVOKABLE auto selectionSummary(QString const& domain) const -> QVariantMap;
    Q_INVOKABLE auto layersSnapshot() const -> QVariantList;
    Q_INVOKABLE auto focus(quint64 target) -> bool;
    Q_INVOKABLE auto focusTarget(quint64 target) -> bool;
    Q_INVOKABLE auto focusNext(QString const& domain, QString const& group = QString{}, bool wrap = true) -> bool;
    Q_INVOKABLE auto focusPrevious(QString const& domain, QString const& group = QString{}, bool wrap = true) -> bool;
    Q_INVOKABLE auto focusPrev(QString const& domain, QString const& group = QString{}, bool wrap = true) -> bool;
    Q_INVOKABLE auto focusFirst(QString const& domain, QString const& group = QString{}) -> bool;
    Q_INVOKABLE auto focusLast(QString const& domain, QString const& group = QString{}) -> bool;
    Q_INVOKABLE auto focusDirection(QString const& domain, QString const& direction, QString const& group = {}) -> bool;
    Q_INVOKABLE auto focusWithin(QString const& domain, QString const& group = {}) -> bool;
    Q_INVOKABLE auto activate(quint64 target) -> bool;
    Q_INVOKABLE auto activateTarget(quint64 target) -> bool;
    Q_INVOKABLE auto select(QVariantList const& targets, QString const& domain, QString const& intent) -> QVariantMap;
    Q_INVOKABLE auto clearSelection(QString const& domain) -> QVariantMap;
    Q_INVOKABLE auto selectAll(QString const& domain) -> void;
    Q_INVOKABLE auto extendSelection(QString const& domain, QString const& direction) -> bool;
    Q_INVOKABLE auto invoke(
        QString const& actionId,
        quint64 target,
        QVariantMap const& arguments = {}
    ) -> bool;
    Q_INVOKABLE auto openContextMenu(quint64 target, qreal x, qreal y) -> QVariantMap;
    Q_INVOKABLE auto closeContextMenu() -> QVariantMap;

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
    auto selectionChanged(QString domain) -> void;
    auto actionsChanged() -> void;
    auto commandResultChanged() -> void;
    auto actionRequested(QString actionId, quint64 target, QString semanticKey) -> void;
    auto contextMenuForTarget(quint64 target, qreal x, qreal y) -> void;
    auto selectionCommandRequested(QString domain, quint64 cursor) -> void;

protected:
    auto eventFilter(QObject* watched, QEvent* event) -> bool override;

private:
    auto onScopeCreated(InteractionScope* scope) -> void;
    auto onScopeChanged(InteractionScope* scope) -> void;
    auto onScopeDestroyed(InteractionScope* scope) -> void;
    auto attachWindow(QQuickWindow* window) -> void;
    auto syncWindowEvent(QQuickWindow* window, QEvent* event) -> bool;
    auto routeKeyEvent(QQuickWindow* window, QEvent* event) -> bool;
    auto resolveFocus(QQuickWindow* window) -> void;
    [[nodiscard]] auto scopeForItem(QQuickItem const* item) const -> InteractionScope*;
    [[nodiscard]] auto hitTestTarget(QQuickWindow* window, QPointF const& scenePosition) const -> quint64;
    auto recordContextChange(mts::interaction::ContextChange const& change) -> void;
    auto recordSelectionChange(mts::interaction::SelectionChange const& change) -> void;
    auto recordLayerChange(mts::interaction::LayerChange const& change) -> void;
    auto installDefaultActions() -> void;
    auto recordActionInvocation(QString const& actionId, mts::interaction::TargetId target) -> void;
    auto restoreFocus(mts::interaction::TargetId target) -> void;
    auto closeLayer(quint64 id) -> bool;
    auto closeLayerOutside(quint64 id) -> bool;
    auto focusTargetOnGui(mts::interaction::TargetId target) -> bool;
    auto activateTargetOnGui(mts::interaction::TargetId target) -> bool;
    auto closeContextMenuOnGui() -> bool;
    auto popLayerOnGui(quint64 id) -> bool;
    auto executeAndReport(mts::interaction::CommandRequest const& request) -> mts::interaction::CommandResult;
    auto commandResultMap(mts::interaction::CommandResult const& result) const -> QVariantMap;

    mts::events::EventBus* m_bus = nullptr;
    std::unique_ptr<mts::interaction::TargetRegistry> m_registry;
    std::unique_ptr<mts::interaction::ContextStore> m_context;
    std::unique_ptr<mts::interaction::SelectionModel> m_selection;
    std::unique_ptr<mts::interaction::TraceRing> m_trace;
    std::unique_ptr<mts::interaction::LayerStack> m_layers;
    std::unique_ptr<mts::interaction::ActionRegistry> m_actions;
    std::unique_ptr<mts::interaction::NavigationModel> m_navigation;
    std::unique_ptr<mts::interaction::KeyRouter> m_keyRouter;
    std::unique_ptr<mts::interaction::CommandFacade> m_commands;
    QHash<quint64, QPointer<QObject>> m_layerObjects;
    QPointer<QObject> m_outsideDismissedObject;
    QHash<InteractionScope*, quint64> m_bindings;
    QHash<QObject*, InteractionScope*> m_ownerIndex;
    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_focusConnection;
    std::size_t m_contextSubscription = 0;
    std::size_t m_selectionSubscription = 0;
    std::size_t m_layerSubscription = 0;
    int m_actionRevision = 0;
    QVariantMap m_lastCommandResult;
};
