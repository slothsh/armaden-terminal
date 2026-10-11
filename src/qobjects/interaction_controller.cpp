#include "interaction_controller.hpp"

#include "interaction_hub.hpp"
#include "interaction_scope.hpp"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QTimer>
#include <QtGlobal>
#include <QWheelEvent>

import std;
import events;
import interaction;

namespace {

auto buttonFrom(Qt::MouseButton button) -> mts::interaction::PointerButton {
    switch (button) {
        case Qt::LeftButton: {
            return mts::interaction::PointerButton::Primary;
        }

        case Qt::RightButton: {
            return mts::interaction::PointerButton::Secondary;
        }

        case Qt::MiddleButton: {
            return mts::interaction::PointerButton::Middle;
        }

        default: {
            return mts::interaction::PointerButton::None;
        }
    }
}

auto modifiersFrom(Qt::KeyboardModifiers modifiers) -> mts::interaction::Modifiers {
    mts::interaction::Modifiers result;
    result.shift = modifiers.testFlag(Qt::ShiftModifier);
    result.control = modifiers.testFlag(Qt::ControlModifier);
    result.alt = modifiers.testFlag(Qt::AltModifier);
    result.meta = modifiers.testFlag(Qt::MetaModifier);

    return result;
}

auto toPoint(QPointF const& position) -> mts::interaction::Point {
    return mts::interaction::Point{position.x(), position.y()};
}

auto wheelSteps(QWheelEvent const& event) -> qreal {
    QPoint const angleDelta = event.angleDelta();
    QPoint const pixelDelta = event.pixelDelta();

    return angleDelta.y() != 0
        ? qreal(angleDelta.y()) / 120.0
        : qreal(pixelDelta.y()) / 100.0;
}

auto selectionModeFrom(QString const& value) -> mts::interaction::SelectionMode {
    if (value == QStringLiteral("single")) {
        return mts::interaction::SelectionMode::Single;
    }

    if (value == QStringLiteral("multiple")) {
        return mts::interaction::SelectionMode::Multiple;
    }

    return mts::interaction::SelectionMode::None;
}

auto domainOf(mts::interaction::TargetRegistry const& registry, mts::interaction::TargetId target) -> std::string {
    auto const resolved = registry.find(target);

    return resolved.has_value() ? std::string(resolved->domain.path()) : std::string{};
}

auto navigationKeyFrom(int key) -> std::optional<mts::interaction::NavigationKey> {
    switch (key) {
        case Qt::Key_Tab:
        case Qt::Key_Backtab: {
            return mts::interaction::NavigationKey::Tab;
        }

        case Qt::Key_Return:
        case Qt::Key_Enter: {
            return mts::interaction::NavigationKey::Enter;
        }

        case Qt::Key_Space: {
            return mts::interaction::NavigationKey::Space;
        }

        case Qt::Key_Escape: {
            return mts::interaction::NavigationKey::Escape;
        }

        case Qt::Key_Up: {
            return mts::interaction::NavigationKey::ArrowUp;
        }

        case Qt::Key_Down: {
            return mts::interaction::NavigationKey::ArrowDown;
        }

        case Qt::Key_Left: {
            return mts::interaction::NavigationKey::ArrowLeft;
        }

        case Qt::Key_Right: {
            return mts::interaction::NavigationKey::ArrowRight;
        }

        case Qt::Key_Plus: {
            return mts::interaction::NavigationKey::Plus;
        }

        case Qt::Key_Minus: {
            return mts::interaction::NavigationKey::Minus;
        }

        case Qt::Key_Menu: {
            return mts::interaction::NavigationKey::Menu;
        }

        case Qt::Key_F10: {
            return mts::interaction::NavigationKey::F10;
        }

        case Qt::Key_A: {
            return mts::interaction::NavigationKey::A;
        }

        default: {
            return std::nullopt;
        }
    }
}

auto directionFrom(QString const& value) -> std::optional<mts::interaction::NavigationDirection> {
    if (value == QStringLiteral("up")) {
        return mts::interaction::NavigationDirection::Up;
    }
    if (value == QStringLiteral("down")) {
        return mts::interaction::NavigationDirection::Down;
    }
    if (value == QStringLiteral("left")) {
        return mts::interaction::NavigationDirection::Left;
    }
    if (value == QStringLiteral("right")) {
        return mts::interaction::NavigationDirection::Right;
    }

    return std::nullopt;
}

auto actionSurfaceFrom(QString const& value) -> mts::interaction::ActionSurface {
    if (value == QStringLiteral("contextMenu")) {
        return mts::interaction::ActionSurface::ContextMenu;
    }

    if (value == QStringLiteral("button")) {
        return mts::interaction::ActionSurface::Button;
    }

    if (value == QStringLiteral("shortcut")) {
        return mts::interaction::ActionSurface::Shortcut;
    }

    return mts::interaction::ActionSurface::Menu;
}

auto layerKindName(mts::interaction::LayerKind kind) -> QString {
    switch (kind) {
        case mts::interaction::LayerKind::Dialog: {
            return QStringLiteral("dialog");
        }

        case mts::interaction::LayerKind::Pane: {
            return QStringLiteral("pane");
        }

        case mts::interaction::LayerKind::ContextMenu: {
            return QStringLiteral("contextMenu");
        }

        case mts::interaction::LayerKind::Popup: {
            return QStringLiteral("popup");
        }
    }

    return QStringLiteral("popup");
}

auto commandKindFrom(QString const& value) -> std::optional<mts::interaction::CommandKind> {
    using Kind = mts::interaction::CommandKind;
    if (value == QStringLiteral("focus")) { return Kind::Focus; }
    if (value == QStringLiteral("focusNext")) { return Kind::FocusNext; }
    if (value == QStringLiteral("focusPrevious") || value == QStringLiteral("focusPrev")) { return Kind::FocusPrevious; }
    if (value == QStringLiteral("focusFirst")) { return Kind::FocusFirst; }
    if (value == QStringLiteral("focusLast")) { return Kind::FocusLast; }
    if (value == QStringLiteral("focusDirection")) { return Kind::FocusDirection; }
    if (value == QStringLiteral("select")) { return Kind::Select; }
    if (value == QStringLiteral("clearSelection")) { return Kind::ClearSelection; }
    if (value == QStringLiteral("selectAll")) { return Kind::SelectAll; }
    if (value == QStringLiteral("extendSelection")) { return Kind::ExtendSelection; }
    if (value == QStringLiteral("activate")) { return Kind::Activate; }
    if (value == QStringLiteral("invoke") || value == QStringLiteral("invokeAction")) { return Kind::InvokeAction; }
    if (value == QStringLiteral("openContextMenu")) { return Kind::OpenContextMenu; }
    if (value == QStringLiteral("closeContextMenu")) { return Kind::CloseContextMenu; }
    if (value == QStringLiteral("pushLayer")) { return Kind::PushLayer; }
    if (value == QStringLiteral("popLayer")) { return Kind::PopLayer; }

    return std::nullopt;
}

auto selectionIntentFrom(QString const& value) -> std::optional<mts::interaction::SelectionIntent> {
    if (value.isEmpty() || value == QStringLiteral("replace")) {
        return mts::interaction::SelectionIntent::Replace;
    }
    if (value == QStringLiteral("toggle")) {
        return mts::interaction::SelectionIntent::Toggle;
    }
    if (value == QStringLiteral("range")) {
        return mts::interaction::SelectionIntent::Range;
    }
    if (value == QStringLiteral("addRange")) {
        return mts::interaction::SelectionIntent::AddRange;
    }
    if (value == QStringLiteral("add")) {
        return mts::interaction::SelectionIntent::Add;
    }

    return std::nullopt;
}

auto commandStatusName(mts::interaction::CommandStatus status) -> QString {
    switch (status) {
        case mts::interaction::CommandStatus::Succeeded: { return QStringLiteral("succeeded"); }
        case mts::interaction::CommandStatus::InvalidRequest: { return QStringLiteral("invalidRequest"); }
        case mts::interaction::CommandStatus::InvalidTarget: { return QStringLiteral("invalidTarget"); }
        case mts::interaction::CommandStatus::StaleTarget: { return QStringLiteral("staleTarget"); }
        case mts::interaction::CommandStatus::CapabilityDenied: { return QStringLiteral("capabilityDenied"); }
        case mts::interaction::CommandStatus::LayerBlocked: { return QStringLiteral("layerBlocked"); }
        case mts::interaction::CommandStatus::NoFocusableTarget: { return QStringLiteral("noFocusableTarget"); }
        case mts::interaction::CommandStatus::InvalidDirection: { return QStringLiteral("invalidDirection"); }
        case mts::interaction::CommandStatus::ActionNotFound: { return QStringLiteral("actionNotFound"); }
        case mts::interaction::CommandStatus::ActionOutOfScope: { return QStringLiteral("actionOutOfScope"); }
        case mts::interaction::CommandStatus::ActionHidden: { return QStringLiteral("actionHidden"); }
        case mts::interaction::CommandStatus::ActionDisabled: { return QStringLiteral("actionDisabled"); }
        case mts::interaction::CommandStatus::ActionMissingCapability: { return QStringLiteral("actionMissingCapability"); }
        case mts::interaction::CommandStatus::ActionMissingHandler: { return QStringLiteral("actionMissingHandler"); }
        case mts::interaction::CommandStatus::LayerNotFound: { return QStringLiteral("layerNotFound"); }
        case mts::interaction::CommandStatus::Unsupported: { return QStringLiteral("unsupported"); }
        case mts::interaction::CommandStatus::WrongThread: { return QStringLiteral("wrongThread"); }
        case mts::interaction::CommandStatus::DispatchUnavailable: { return QStringLiteral("dispatchUnavailable"); }
        case mts::interaction::CommandStatus::FacadeClosed: { return QStringLiteral("facadeClosed"); }
    }

    return QStringLiteral("unsupported");
}

auto layerKindFrom(QString const& value) -> mts::interaction::LayerKind {
    if (value == QStringLiteral("dialog")) {
        return mts::interaction::LayerKind::Dialog;
    }

    if (value == QStringLiteral("pane")) {
        return mts::interaction::LayerKind::Pane;
    }

    if (value == QStringLiteral("contextMenu")) {
        return mts::interaction::LayerKind::ContextMenu;
    }

    return mts::interaction::LayerKind::Popup;
}

auto toDescriptor(InteractionScope const& scope) -> mts::interaction::TargetDescriptor {
    QRectF const geometry = scope.sceneGeometry();

    mts::interaction::TargetDescriptor descriptor;
    descriptor.domain = mts::interaction::Domain(scope.domain().toStdString());
    descriptor.role = mts::interaction::Role(scope.role().toStdString());
    descriptor.label = scope.label().toStdString();
    descriptor.group = scope.group().toStdString();
    descriptor.semanticKey = scope.semanticKey().toStdString();
    descriptor.order = scope.order();
    descriptor.geometry = mts::interaction::Rect{
        geometry.x(),
        geometry.y(),
        geometry.width(),
        geometry.height(),
    };
    if (auto* item = qobject_cast<QQuickItem*>(scope.owner()); item != nullptr) {
        descriptor.visible = item->isVisible();
        descriptor.enabled = item->isEnabled();
    }
    descriptor.capabilities.focusable = scope.focusable();
    descriptor.capabilities.selectable = scope.selectable();
    descriptor.capabilities.activatable = scope.activatable();
    descriptor.capabilities.contextMenuable = scope.contextMenuable();
    descriptor.capabilities.navigable = scope.navigable();
    descriptor.capabilities.editable = scope.editable();
    descriptor.capabilities.draggable = scope.draggable();
    descriptor.capabilities.scrollable = scope.scrollable();
    descriptor.capabilities.hitTestable = scope.hitTestable();
    descriptor.capabilities.pannable = scope.pannable();
    descriptor.capabilities.zoomable = scope.zoomable();
    descriptor.capabilities.selection = selectionModeFrom(scope.selectionMode());

    return descriptor;
}

} // END OF NAMESPACE anonymous

InteractionController::InteractionController(QObject* parent)
    : QObject(parent)
    , m_registry(std::make_unique<mts::interaction::TargetRegistry>())
    , m_context(std::make_unique<mts::interaction::ContextStore>(*m_registry))
    , m_selection(std::make_unique<mts::interaction::SelectionModel>(*m_registry))
    , m_trace(std::make_unique<mts::interaction::TraceRing>())
    , m_layers(std::make_unique<mts::interaction::LayerStack>())
    , m_actions(std::make_unique<mts::interaction::ActionRegistry>())
    , m_navigation(std::make_unique<mts::interaction::NavigationModel>(*m_registry, *m_layers))
    , m_keyRouter(std::make_unique<mts::interaction::KeyRouter>()) {
    m_contextSubscription = m_context->subscribe([this](mts::interaction::ContextChange const& change) {
        this->recordContextChange(change);
        emit this->contextChanged();
        m_actionRevision += 1;
        emit this->actionsChanged();
    });

    m_selectionSubscription = m_selection->subscribe([this](mts::interaction::SelectionChange const& change) {
        this->recordSelectionChange(change);
        m_selectionRevision += 1;
        emit this->selectionChanged(QString::fromUtf8(
            change.domain.path().data(),
            static_cast<qsizetype>(change.domain.path().size())
        ));
        m_actionRevision += 1;
        emit this->actionsChanged();
    });

    m_layerSubscription = m_layers->subscribe([this](mts::interaction::LayerChange const& change) {
        this->recordLayerChange(change);
        emit this->layersChanged();
    });

    InteractionHub* hub = InteractionHub::instance();
    this->connect(hub, &InteractionHub::scopeCreated, this, &InteractionController::onScopeCreated);
    this->connect(hub, &InteractionHub::scopeChanged, this, &InteractionController::onScopeChanged);
    this->connect(hub, &InteractionHub::scopeDestroyed, this, &InteractionController::onScopeDestroyed);
    this->installDefaultActions();

    mts::interaction::UiCommandHandlers handlers;
    handlers.focus = [this](mts::interaction::TargetId target) {
        return this->focusTargetOnGui(target);
    };
    handlers.activate = [this](mts::interaction::TargetId target) {
        return this->activateTargetOnGui(target);
    };
    handlers.openContextMenu = [this](mts::interaction::TargetId target, mts::interaction::Point const& position) {
        emit this->contextMenuForTarget(target.packed(), position.x, position.y);

        return true;
    };
    handlers.closeContextMenu = [this] {
        return this->closeContextMenuOnGui();
    };
    handlers.selectionChanged = [this](mts::interaction::Domain const& domain, mts::interaction::TargetId cursor) {
        emit this->selectionCommandRequested(
            QString::fromUtf8(domain.path().data(), static_cast<qsizetype>(domain.path().size())),
            cursor.packed()
        );
    };
    handlers.commandCompleted = [this](mts::interaction::CommandResult const& result) {
        m_lastCommandResult = this->commandResultMap(result);
        emit this->commandResultChanged();
    };
    handlers.pushLayer = [this](mts::interaction::LayerDescriptor const& descriptor) -> std::optional<mts::interaction::u64> {
        return m_layers->push(descriptor);
    };
    handlers.popLayer = [this](mts::interaction::u64 id) {
        return this->popLayerOnGui(id);
    };

    m_commands = std::make_unique<mts::interaction::CommandFacade>(
        *m_registry,
        *m_context,
        *m_selection,
        *m_layers,
        *m_actions,
        *m_navigation,
        std::move(handlers),
        [this](std::function<void()> command) {
            return QMetaObject::invokeMethod(
                this,
                [command = std::move(command)]() mutable { command(); },
                Qt::QueuedConnection
            );
        }
    );

    if (QGuiApplication::instance() != nullptr) {
        QGuiApplication::instance()->installEventFilter(this);
    }
}

InteractionController::~InteractionController() {
    if (m_layers != nullptr) {
        m_layers->unsubscribe(m_layerSubscription);
    }

    if (m_selection != nullptr) {
        m_selection->unsubscribe(m_selectionSubscription);
    }

    if (m_context != nullptr) {
        m_context->unsubscribe(m_contextSubscription);
    }

    if (QGuiApplication::instance() != nullptr) {
        QGuiApplication::instance()->removeEventFilter(this);
    }
}

auto InteractionController::targetCount() const -> int {
    return static_cast<int>(m_registry->size());
}

auto InteractionController::focusedTarget() const -> quint64 {
    return m_context->focusedId().packed();
}

auto InteractionController::focusedLabel() const -> QString {
    auto const target = m_registry->find(m_context->focusedId());

    return target.has_value() ? QString::fromStdString(target->label) : QString{};
}

auto InteractionController::focusedDomain() const -> QString {
    auto const target = m_registry->find(m_context->focusedId());

    return target.has_value() ? QString::fromStdString(std::string(target->domain.path())) : QString{};
}

auto InteractionController::currentTarget() const -> quint64 {
    return m_context->currentId().packed();
}

auto InteractionController::hoveredTarget() const -> quint64 {
    return m_context->hoveredId().packed();
}

auto InteractionController::pointerPosition() const -> QPointF {
    mts::interaction::PointerState const pointer = m_context->pointer();

    return QPointF(pointer.x, pointer.y);
}

auto InteractionController::layerDepth() const -> int {
    return static_cast<int>(m_layers->depth());
}

auto InteractionController::topLayerId() const -> quint64 {
    auto const top = m_layers->top();

    return top.has_value() ? top->id : 0;
}

auto InteractionController::actionRevision() const -> int {
    return m_actionRevision;
}

auto InteractionController::selectionRevision() const -> int {
    return m_selectionRevision;
}

auto InteractionController::setEventBus(mts::events::EventBus* bus) -> void {
    m_bus = bus;
}

auto InteractionController::registry() -> mts::interaction::TargetRegistry& {
    return *m_registry;
}

auto InteractionController::selection() -> mts::interaction::SelectionModel& {
    return *m_selection;
}

auto InteractionController::trace() -> mts::interaction::TraceRing& {
    return *m_trace;
}

auto InteractionController::layers() -> mts::interaction::LayerStack& {
    return *m_layers;
}

auto InteractionController::actions() -> mts::interaction::ActionRegistry& {
    return *m_actions;
}

auto InteractionController::navigation() -> mts::interaction::NavigationModel& {
    return *m_navigation;
}

auto InteractionController::keyRouter() -> mts::interaction::KeyRouter& {
    return *m_keyRouter;
}

auto InteractionController::commands() -> mts::interaction::CommandFacade& {
    return *m_commands;
}

auto InteractionController::lastCommandResult() const -> QVariantMap {
    return m_lastCommandResult;
}

auto InteractionController::commandResultMap(mts::interaction::CommandResult const& result) const -> QVariantMap {
    QVariantMap map;
    map.insert(QStringLiteral("ok"), result.succeeded());
    map.insert(QStringLiteral("status"), commandStatusName(result.status));
    map.insert(QStringLiteral("target"), QVariant::fromValue<qulonglong>(result.target.packed()));
    map.insert(QStringLiteral("layerId"), QVariant::fromValue<qulonglong>(result.layerId));
    map.insert(QStringLiteral("actionId"), QString::fromStdString(result.actionId));
    map.insert(QStringLiteral("message"), QString::fromStdString(result.message));

    return map;
}

auto InteractionController::executeAndReport(mts::interaction::CommandRequest const& request) -> mts::interaction::CommandResult {
    return m_commands->execute(request);
}

auto InteractionController::selectionCount(QString const& domain) const -> int {
    return static_cast<int>(m_selection->count(mts::interaction::Domain(domain.toStdString())));
}

auto InteractionController::isSelected(quint64 target) const -> bool {
    mts::interaction::TargetId const id = mts::interaction::TargetId::fromPacked(target);
    auto const resolved = m_registry->find(id);
    return resolved.has_value() && m_selection->contains(resolved->domain, id);
}

auto InteractionController::selectionIntent(bool shift, bool control) const -> QString {
    mts::interaction::SelectionIntent const intent = mts::interaction::intentFrom(shift, control);
    switch (intent) {
        case mts::interaction::SelectionIntent::Replace: { return QStringLiteral("replace"); }
        case mts::interaction::SelectionIntent::Toggle: { return QStringLiteral("toggle"); }
        case mts::interaction::SelectionIntent::Range: { return QStringLiteral("range"); }
        case mts::interaction::SelectionIntent::AddRange: { return QStringLiteral("addRange"); }
        case mts::interaction::SelectionIntent::Add: { return QStringLiteral("add"); }
    }

    return QStringLiteral("replace");
}

auto InteractionController::selectedTargets(QString const& domain) const -> QVariantList {
    QVariantList result;
    for (mts::interaction::TargetId const target : m_selection->ordered(mts::interaction::Domain(domain.toStdString()))) {
        result.push_back(QVariant::fromValue<qulonglong>(target.packed()));
    }

    return result;
}

auto InteractionController::selectionSummary(QString const& domain) const -> QVariantMap {
    mts::interaction::Domain const scope(domain.toStdString());
    QVariantMap result;
    result.insert(QStringLiteral("domain"), domain);
    result.insert(QStringLiteral("count"), this->selectionCount(domain));
    result.insert(QStringLiteral("targets"), this->selectedTargets(domain));
    result.insert(QStringLiteral("anchor"), QVariant::fromValue<qulonglong>(m_selection->anchor(scope).packed()));
    result.insert(QStringLiteral("cursor"), QVariant::fromValue<qulonglong>(m_selection->cursor(scope).packed()));

    return result;
}

auto InteractionController::layersSnapshot() const -> QVariantList {
    QVariantList result;
    for (mts::interaction::LayerEntry const& layer : m_layers->layers()) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), QVariant::fromValue<qulonglong>(layer.id));
        item.insert(QStringLiteral("name"), QString::fromStdString(layer.descriptor.name));
        item.insert(QStringLiteral("kind"), layerKindName(layer.descriptor.kind));
        item.insert(QStringLiteral("domain"), QString::fromUtf8(
            layer.descriptor.domain.path().data(),
            static_cast<qsizetype>(layer.descriptor.domain.path().size())
        ));
        item.insert(QStringLiteral("dismissOnEscape"), layer.descriptor.dismissOnEscape);
        item.insert(QStringLiteral("dismissOnOutsideClick"), layer.descriptor.dismissOnOutsideClick);
        item.insert(QStringLiteral("focusTrap"), layer.descriptor.focusTrap);
        result.push_back(item);
    }

    return result;
}

auto InteractionController::executeCommand(QVariantMap const& input) -> QVariantMap {
    auto const kind = commandKindFrom(input.value(QStringLiteral("kind")).toString());
    if (!kind.has_value()) {
        mts::interaction::CommandResult const result{
            mts::interaction::CommandStatus::Unsupported,
            mts::interaction::TargetId::invalid(),
            0,
            {},
            "unknown interaction command",
        };
        m_lastCommandResult = this->commandResultMap(result);
        emit this->commandResultChanged();

        return m_lastCommandResult;
    }

    mts::interaction::CommandRequest request;
    request.kind = *kind;
    request.target = mts::interaction::TargetId::fromPacked(input.value(QStringLiteral("target")).toULongLong());
    request.domain = mts::interaction::Domain(input.value(QStringLiteral("domain")).toString().toStdString());
    request.group = input.value(QStringLiteral("group")).toString().toStdString();
    request.wrap = input.value(QStringLiteral("wrap"), true).toBool();
    request.updateSelection = input.value(QStringLiteral("updateSelection"), true).toBool();
    auto const intent = selectionIntentFrom(input.value(QStringLiteral("intent")).toString());
    if (!intent.has_value()) {
        mts::interaction::CommandResult const result{
            mts::interaction::CommandStatus::InvalidRequest,
            request.target,
            0,
            {},
            "selection intent is invalid",
        };
        m_lastCommandResult = this->commandResultMap(result);
        emit this->commandResultChanged();

        return m_lastCommandResult;
    }
    request.selectionIntent = *intent;
    request.actionId = input.value(QStringLiteral("actionId")).toString().toStdString();
    request.layerId = input.value(QStringLiteral("layerId")).toULongLong();
    request.position = mts::interaction::Point{
        input.value(QStringLiteral("x")).toDouble(),
        input.value(QStringLiteral("y")).toDouble(),
    };

    if (auto const direction = directionFrom(input.value(QStringLiteral("direction")).toString()); direction.has_value()) {
        request.direction = *direction;
    } else if (*kind == mts::interaction::CommandKind::FocusDirection
        || *kind == mts::interaction::CommandKind::ExtendSelection) {
        mts::interaction::CommandResult const result{
            mts::interaction::CommandStatus::InvalidDirection,
            request.target,
            0,
            request.actionId,
            "direction must be up, down, left or right",
        };
        m_lastCommandResult = this->commandResultMap(result);
        emit this->commandResultChanged();

        return m_lastCommandResult;
    }

    for (QVariant const& value : input.value(QStringLiteral("targets")).toList()) {
        request.targets.push_back(mts::interaction::TargetId::fromPacked(value.toULongLong()));
    }

    QVariantMap const arguments = input.value(QStringLiteral("arguments")).toMap();
    request.arguments.reserve(static_cast<std::size_t>(arguments.size()));
    for (auto iterator = arguments.cbegin(); iterator != arguments.cend(); ++iterator) {
        request.arguments.push_back(mts::interaction::ActionArgument{
            iterator.key().toStdString(),
            iterator.value().toString().toStdString(),
        });
    }

    if (*kind == mts::interaction::CommandKind::PushLayer) {
        request.layer.name = input.value(QStringLiteral("name")).toString().toStdString();
        request.layer.kind = layerKindFrom(input.value(QStringLiteral("layerKind")).toString());
        request.layer.domain = request.domain;
        request.layer.owner = mts::interaction::TargetId::fromPacked(input.value(QStringLiteral("ownerTarget")).toULongLong());
        request.layer.previousFocus = m_context->focusedId();
        request.layer.dismissOnEscape = input.value(QStringLiteral("dismissOnEscape"), true).toBool();
        request.layer.dismissOnOutsideClick = input.value(QStringLiteral("dismissOnOutsideClick"), true).toBool();
        request.layer.focusTrap = input.value(QStringLiteral("focusTrap"), false).toBool();
    }

    return this->commandResultMap(this->executeAndReport(request));
}

auto InteractionController::context() -> mts::interaction::ContextStore& {
    return *m_context;
}

auto InteractionController::targetSemanticKey(quint64 target) const -> QString {
    auto const resolved = m_registry->find(mts::interaction::TargetId::fromPacked(target));

    return resolved.has_value() ? QString::fromStdString(resolved->semanticKey) : QString{};
}

auto InteractionController::targetForDomain(QString const& domain, QString const& role) const -> quint64 {
    auto const targets = m_registry->byDomain(mts::interaction::Domain(domain.toStdString()));
    for (mts::interaction::TargetRef const& target : targets) {
        if (role.isEmpty() || target.role.name() == role.toStdString()) {
            return target.id.packed();
        }
    }

    return 0;
}

auto InteractionController::actionsFor(quint64 target, QString const& surface) const -> QVariantList {
    mts::interaction::TargetId const targetId = mts::interaction::TargetId::fromPacked(target);
    auto const resolved = m_registry->find(targetId);
    if (!resolved.has_value()) {
        return {};
    }

    mts::interaction::ActionContext const context = m_actions->makeContext(*resolved, *m_context, *m_selection);
    mts::interaction::ActionSurface const actionSurface = actionSurfaceFrom(surface);

    QVariantList result;
    for (mts::interaction::ResolvedAction const& action : m_actions->resolve(context, actionSurface)) {
        QVariantMap model;
        model.insert(QStringLiteral("id"), QString::fromStdString(action.id));
        model.insert(QStringLiteral("label"), QString::fromStdString(action.label));
        model.insert(QStringLiteral("group"), QString::fromStdString(action.group));
        model.insert(QStringLiteral("icon"), QString::fromStdString(action.icon));
        model.insert(QStringLiteral("shortcut"), QString::fromStdString(action.shortcut));
        model.insert(QStringLiteral("enabled"), action.enabled);
        model.insert(QStringLiteral("disabledReason"), QString::fromStdString(action.disabledReason));
        model.insert(QStringLiteral("order"), action.order);
        result.push_back(model);
    }

    return result;
}

auto InteractionController::actionEnabled(
    QString const& actionId,
    quint64 target,
    QString const& surface
) const -> bool {
    mts::interaction::TargetId const targetId = mts::interaction::TargetId::fromPacked(target);
    auto const resolved = m_registry->find(targetId);
    if (!resolved.has_value()) {
        return false;
    }

    mts::interaction::ActionContext const context = m_actions->makeContext(*resolved, *m_context, *m_selection);

    return m_actions->isEnabled(actionId.toStdString(), context, actionSurfaceFrom(surface));
}

auto InteractionController::invokeAction(
    QString const& actionId,
    quint64 target,
    QVariantMap const& arguments
) -> bool {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::InvokeAction;
    request.target = mts::interaction::TargetId::fromPacked(target);
    request.actionId = actionId.toStdString();
    request.arguments.reserve(static_cast<std::size_t>(arguments.size()));
    for (auto iterator = arguments.cbegin(); iterator != arguments.cend(); ++iterator) {
        request.arguments.push_back(mts::interaction::ActionArgument{
            iterator.key().toStdString(),
            iterator.value().toString().toStdString(),
        });
    }

    return this->executeAndReport(request).succeeded();
}

auto InteractionController::invoke(
    QString const& actionId,
    quint64 target,
    QVariantMap const& arguments
) -> bool {
    return this->invokeAction(actionId, target, arguments);
}

auto InteractionController::openContextMenu(quint64 target, qreal x, qreal y) -> QVariantMap {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::OpenContextMenu;
    request.target = mts::interaction::TargetId::fromPacked(target);
    request.position = mts::interaction::Point{x, y};

    return this->commandResultMap(this->executeAndReport(request));
}

auto InteractionController::closeContextMenu() -> QVariantMap {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::CloseContextMenu;

    return this->commandResultMap(this->executeAndReport(request));
}

auto InteractionController::setBusinessValue(QString const& key, QString const& value) -> void {
    m_actions->setBusinessValue(key.toStdString(), value.toStdString());
    m_actionRevision += 1;
    emit this->actionsChanged();
}

auto InteractionController::focus(quint64 target) -> bool {
    return this->focusTarget(target);
}

auto InteractionController::focusTarget(quint64 target) -> bool {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::Focus;
    request.target = mts::interaction::TargetId::fromPacked(target);

    return this->executeAndReport(request).succeeded();
}

auto InteractionController::focusTargetOnGui(mts::interaction::TargetId target) -> bool {
    auto const resolved = m_registry->find(target);
    if (!resolved.has_value()
        || !resolved->capabilities.focusable
        || !resolved->visible
        || !resolved->enabled
        || !m_layers->ownsInput(target, *m_registry)) {
        return false;
    }

    for (auto iterator = m_bindings.cbegin(); iterator != m_bindings.cend(); ++iterator) {
        if (iterator.value() != target.packed()) {
            continue;
        }

        if (auto* item = qobject_cast<QQuickItem*>(iterator.key()->owner()); item != nullptr) {
            item->forceActiveFocus(Qt::TabFocusReason);
            m_context->setFocused(target);
            m_navigation->remember(resolved->domain, target);

            return true;
        }
    }

    return false;
}

auto InteractionController::focusNext(QString const& domain, QString const& group, bool wrap) -> bool {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::FocusNext;
    request.domain = mts::interaction::Domain(domain.toStdString());
    request.group = group.toStdString();
    request.wrap = wrap;

    return this->executeAndReport(request).succeeded();
}

auto InteractionController::focusPrevious(QString const& domain, QString const& group, bool wrap) -> bool {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::FocusPrevious;
    request.domain = mts::interaction::Domain(domain.toStdString());
    request.group = group.toStdString();
    request.wrap = wrap;

    return this->executeAndReport(request).succeeded();
}

auto InteractionController::focusPrev(QString const& domain, QString const& group, bool wrap) -> bool {
    return this->focusPrevious(domain, group, wrap);
}

auto InteractionController::focusFirst(QString const& domain, QString const& group) -> bool {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::FocusFirst;
    request.domain = mts::interaction::Domain(domain.toStdString());
    request.group = group.toStdString();

    return this->executeAndReport(request).succeeded();
}

auto InteractionController::focusLast(QString const& domain, QString const& group) -> bool {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::FocusLast;
    request.domain = mts::interaction::Domain(domain.toStdString());
    request.group = group.toStdString();

    return this->executeAndReport(request).succeeded();
}

auto InteractionController::focusDirection(
    QString const& domain,
    QString const& direction,
    QString const& group
) -> bool {
    auto const navigationDirection = directionFrom(direction);
    if (!navigationDirection.has_value()) {
        mts::interaction::CommandResult const result{
            mts::interaction::CommandStatus::InvalidDirection,
            mts::interaction::TargetId::invalid(),
            0,
            {},
            "direction must be up, down, left or right",
        };
        m_lastCommandResult = this->commandResultMap(result);
        emit this->commandResultChanged();

        return false;
    }

    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::FocusDirection;
    request.domain = mts::interaction::Domain(domain.toStdString());
    request.direction = *navigationDirection;
    request.group = group.toStdString();

    return this->executeAndReport(request).succeeded();
}

auto InteractionController::focusWithin(QString const& domain, QString const& group) -> bool {
    return this->focusFirst(domain, group);
}

auto InteractionController::activate(quint64 target) -> bool {
    return this->activateTarget(target);
}

auto InteractionController::activateTarget(quint64 target) -> bool {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::Activate;
    request.target = mts::interaction::TargetId::fromPacked(target);

    return this->executeAndReport(request).succeeded();
}

auto InteractionController::closeContextMenuOnGui() -> bool {
    auto const top = m_layers->top();
    if (!top.has_value() || top->descriptor.kind != mts::interaction::LayerKind::ContextMenu) {
        return false;
    }

    return this->closeLayer(top->id);
}

auto InteractionController::activateTargetOnGui(mts::interaction::TargetId target) -> bool {
    auto const resolved = m_registry->find(target);
    if (!resolved.has_value()
        || !resolved->capabilities.activatable
        || !resolved->visible
        || !resolved->enabled
        || !m_layers->ownsInput(target, *m_registry)) {
        return false;
    }

    for (auto iterator = m_bindings.cbegin(); iterator != m_bindings.cend(); ++iterator) {
        if (iterator.value() != target.packed()) {
            continue;
        }

        QObject* const owner = iterator.key()->owner();
        return QMetaObject::invokeMethod(owner, "activate", Qt::DirectConnection);
    }

    return false;
}

auto InteractionController::select(
    QVariantList const& targets,
    QString const& domain,
    QString const& intent
) -> QVariantMap {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::Select;
    request.domain = mts::interaction::Domain(domain.toStdString());
    auto const selectionIntent = selectionIntentFrom(intent);
    if (!selectionIntent.has_value()) {
        mts::interaction::CommandResult const result{
            mts::interaction::CommandStatus::InvalidRequest,
            mts::interaction::TargetId::invalid(),
            0,
            {},
            "selection intent is invalid",
        };
        m_lastCommandResult = this->commandResultMap(result);
        emit this->commandResultChanged();

        return m_lastCommandResult;
    }
    request.selectionIntent = *selectionIntent;
    request.targets.reserve(static_cast<std::size_t>(targets.size()));
    for (QVariant const& target : targets) {
        request.targets.push_back(mts::interaction::TargetId::fromPacked(target.toULongLong()));
    }

    return this->commandResultMap(this->executeAndReport(request));
}

auto InteractionController::clearSelection(QString const& domain) -> QVariantMap {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::ClearSelection;
    request.domain = mts::interaction::Domain(domain.toStdString());

    return this->commandResultMap(this->executeAndReport(request));
}

auto InteractionController::selectAll(QString const& domain) -> void {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::SelectAll;
    request.domain = mts::interaction::Domain(domain.toStdString());
    this->executeAndReport(request);
}

auto InteractionController::extendSelection(QString const& domain, QString const& direction) -> bool {
    auto const navigationDirection = directionFrom(direction);
    if (!navigationDirection.has_value()) {
        mts::interaction::CommandResult const result{
            mts::interaction::CommandStatus::InvalidDirection,
            mts::interaction::TargetId::invalid(),
            0,
            {},
            "direction must be up, down, left or right",
        };
        m_lastCommandResult = this->commandResultMap(result);
        emit this->commandResultChanged();

        return false;
    }

    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::ExtendSelection;
    request.domain = mts::interaction::Domain(domain.toStdString());
    request.direction = *navigationDirection;

    return this->executeAndReport(request).succeeded();
}

auto InteractionController::pushLayer(
    QObject* object,
    QString const& kind,
    QString const& domain,
    quint64 ownerTarget,
    bool dismissOnEscape,
    bool dismissOnOutsideClick,
    bool focusTrap
) -> quint64 {
    if (object == nullptr) {
        return 0;
    }

    mts::interaction::LayerDescriptor descriptor;
    descriptor.name = object->objectName().toStdString();
    descriptor.kind = layerKindFrom(kind);
    descriptor.domain = mts::interaction::Domain(domain.toStdString());
    descriptor.owner = mts::interaction::TargetId::fromPacked(ownerTarget);
    if (!descriptor.owner.valid()) {
        descriptor.owner = m_context->currentId();
    }
    descriptor.previousFocus = m_context->focusedId();
    if (auto const parent = m_layers->top(); parent.has_value()) {
        auto const focused = m_registry->find(descriptor.previousFocus);
        if (focused.has_value() && focused->domain.isWithin(parent->descriptor.domain)) {
            descriptor.previousFocus = parent->descriptor.previousFocus;
        }
    }
    descriptor.dismissOnEscape = dismissOnEscape;
    descriptor.dismissOnOutsideClick = dismissOnOutsideClick;
    descriptor.focusTrap = focusTrap;

    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::PushLayer;
    request.layer = std::move(descriptor);
    auto const result = this->executeAndReport(request);
    if (!result.succeeded()) {
        return 0;
    }

    m_layerObjects.insert(result.layerId, object);

    return result.layerId;
}

auto InteractionController::positionLayer(QObject* object, qreal x, qreal y) -> void {
    if (object == nullptr) {
        return;
    }

    auto* parentItem = qobject_cast<QQuickItem*>(object->property("parent").value<QObject*>());
    QQuickWindow* window = parentItem != nullptr ? parentItem->window() : nullptr;
    if (parentItem == nullptr || window == nullptr) {
        object->setProperty("x", x);
        object->setProperty("y", y);

        return;
    }

    QPointF const origin = parentItem->mapToItem(window->contentItem(), 0.0, 0.0);
    qreal const width = object->property("width").toReal();
    qreal const height = object->property("height").toReal();
    qreal const maxX = std::max(0.0, window->width() - origin.x() - width);
    qreal const maxY = std::max(0.0, window->height() - origin.y() - height);

    object->setProperty("x", std::clamp(x, 0.0, maxX));
    object->setProperty("y", std::clamp(y, 0.0, maxY));
}

auto InteractionController::popLayer(quint64 id) -> void {
    mts::interaction::CommandRequest request;
    request.kind = mts::interaction::CommandKind::PopLayer;
    request.layerId = id;
    this->executeAndReport(request);
}

auto InteractionController::popLayerOnGui(quint64 id) -> bool {
    auto const layer = m_layers->find(id);
    if (!layer.has_value()) {
        return false;
    }

    mts::interaction::TargetId const previousFocus = layer->descriptor.previousFocus;
    auto const top = m_layers->top();
    bool const wasTop = top.has_value() && top->id == id;
    m_layers->remove(id);

    for (auto iterator = m_layerObjects.begin(); iterator != m_layerObjects.end();) {
        if (!m_layers->find(iterator.key()).has_value()) {
            iterator = m_layerObjects.erase(iterator);
        } else {
            ++iterator;
        }
    }

    if (wasTop) {
        this->restoreFocus(previousFocus);
    }

    return true;
}

auto InteractionController::dismissTopLayer() -> bool {
    auto const top = m_layers->top();
    if (!top.has_value() || !top->descriptor.dismissOnEscape) {
        return false;
    }

    return this->closeLayer(top->id);
}

auto InteractionController::consumeOutsideDismissal(QObject* object) -> bool {
    if (object == nullptr || m_outsideDismissedObject != object) {
        return false;
    }

    m_outsideDismissedObject = nullptr;

    return true;
}

auto InteractionController::dismissTopLayerOutside() -> bool {
    auto const top = m_layers->top();
    if (!top.has_value() || !top->descriptor.dismissOnOutsideClick) {
        return false;
    }

    return this->closeLayerOutside(top->id);
}

auto InteractionController::closeLayer(quint64 id) -> bool {
    QPointer<QObject> const object = m_layerObjects.value(id);
    if (object == nullptr) {
        return this->popLayerOnGui(id);
    }

    if (!QMetaObject::invokeMethod(object, "dismissLayer", Qt::DirectConnection)) {
        if (!QMetaObject::invokeMethod(object, "close", Qt::DirectConnection)) {
            this->popLayerOnGui(id);
        }
    }

    return true;
}

auto InteractionController::closeLayerOutside(quint64 id) -> bool {
    QPointer<QObject> const object = m_layerObjects.value(id);
    if (object == nullptr) {
        return this->popLayerOnGui(id);
    }

    m_outsideDismissedObject = object;
    if (!QMetaObject::invokeMethod(object, "dismissLayerOutside", Qt::DirectConnection)) {
        if (!QMetaObject::invokeMethod(object, "dismissLayer", Qt::DirectConnection)) {
            if (!QMetaObject::invokeMethod(object, "close", Qt::DirectConnection)) {
                this->popLayerOnGui(id);
            }
        }
    }

    return true;
}

auto InteractionController::eventFilter(QObject* watched, QEvent* event) -> bool {
    QQuickWindow* window = qobject_cast<QQuickWindow*>(watched);
    bool const windowCoordinates = window != nullptr;
    if (window == nullptr) {
        if (auto* item = qobject_cast<QQuickItem*>(watched); item != nullptr) {
            window = item->window();
        }
    }

    if (window != nullptr) {
        this->attachWindow(window);

        if (event->type() == QEvent::KeyPress && this->routeKeyEvent(window, event)) {
            event->accept();

            return true;
        }

        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            this->syncWindowEvent(window, event);
        } else if (windowCoordinates && this->syncWindowEvent(window, event)) {
            event->accept();

            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

auto InteractionController::routeKeyEvent(QQuickWindow* window, QEvent* event) -> bool {
    if (event->type() != QEvent::KeyPress) {
        return false;
    }

    auto const* keyEvent = static_cast<QKeyEvent*>(event);
    auto const navigationKey = navigationKeyFrom(keyEvent->key());
    if (!navigationKey.has_value()) {
        return false;
    }

    mts::interaction::Modifiers const modifiers = modifiersFrom(keyEvent->modifiers());
    m_context->recordModifiers(modifiers);
    mts::interaction::TargetId current = m_context->focusedId();
    if (!current.valid()) {
        current = m_context->currentId();
    }

    auto const target = m_registry->find(current);
    auto const top = m_layers->top();
    mts::interaction::Domain domain;
    if (top.has_value() && top->descriptor.focusTrap) {
        domain = top->descriptor.domain;
    } else if (target.has_value()) {
        domain = target->domain;
    }

    std::optional<mts::interaction::LayerKind> const layerKind = top.has_value()
        ? std::optional<mts::interaction::LayerKind>{top->descriptor.kind}
        : std::nullopt;
    auto const binding = m_keyRouter->resolve(
        mts::interaction::KeyStroke{*navigationKey, modifiers},
        domain,
        layerKind
    );
    if (!binding.has_value()) {
        return false;
    }

    bool const traversal = binding->command == mts::interaction::NavigationCommand::FocusNext
        || binding->command == mts::interaction::NavigationCommand::FocusPrevious;
    bool const layerDismiss = binding->command == mts::interaction::NavigationCommand::DismissLayer;
    bool const contextMenu = binding->command == mts::interaction::NavigationCommand::ShowContextMenu;
    bool const textSubmit = binding->command == mts::interaction::NavigationCommand::Activate
        && keyEvent->modifiers().testFlag(Qt::ControlModifier)
        && (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter);
    if (target.has_value()
        && target->capabilities.editable
        && !traversal
        && !layerDismiss
        && !contextMenu
        && !textSubmit) {
        return false;
    }

    QString const domainName = QString::fromUtf8(domain.path().data(), static_cast<qsizetype>(domain.path().size()));
    QString const group = target.has_value() ? QString::fromStdString(target->group) : QString{};

    switch (binding->command) {
        case mts::interaction::NavigationCommand::FocusNext: {
            return this->focusNext(domainName, group, true);
        }

        case mts::interaction::NavigationCommand::FocusPrevious: {
            return this->focusPrevious(domainName, group, true);
        }

        case mts::interaction::NavigationCommand::FocusFirst: {
            return this->focusFirst(domainName, group);
        }

        case mts::interaction::NavigationCommand::FocusLast: {
            return this->focusLast(domainName, group);
        }

        case mts::interaction::NavigationCommand::MoveUp: {
            return this->focusDirection(domainName, QStringLiteral("up"), group);
        }

        case mts::interaction::NavigationCommand::MoveDown: {
            return this->focusDirection(domainName, QStringLiteral("down"), group);
        }

        case mts::interaction::NavigationCommand::MoveLeft: {
            return this->focusDirection(domainName, QStringLiteral("left"), group);
        }

        case mts::interaction::NavigationCommand::MoveRight: {
            return this->focusDirection(domainName, QStringLiteral("right"), group);
        }

        case mts::interaction::NavigationCommand::ExtendUp: {
            return this->extendSelection(domainName, QStringLiteral("up"));
        }

        case mts::interaction::NavigationCommand::ExtendDown: {
            return this->extendSelection(domainName, QStringLiteral("down"));
        }

        case mts::interaction::NavigationCommand::ExtendLeft: {
            return this->extendSelection(domainName, QStringLiteral("left"));
        }

        case mts::interaction::NavigationCommand::ExtendRight: {
            return this->extendSelection(domainName, QStringLiteral("right"));
        }

        case mts::interaction::NavigationCommand::PanUp:
        case mts::interaction::NavigationCommand::PanDown:
        case mts::interaction::NavigationCommand::PanLeft:
        case mts::interaction::NavigationCommand::PanRight: {
            if (!target.has_value()) {
                return false;
            }

            QPointF delta;
            switch (binding->command) {
                case mts::interaction::NavigationCommand::PanUp: {
                    delta.setY(-48.0);
                    break;
                }

                case mts::interaction::NavigationCommand::PanDown: {
                    delta.setY(48.0);
                    break;
                }

                case mts::interaction::NavigationCommand::PanLeft: {
                    delta.setX(-48.0);
                    break;
                }

                case mts::interaction::NavigationCommand::PanRight: {
                    delta.setX(48.0);
                    break;
                }

                default: {
                    return false;
                }
            }

            QPointF const position(
                target->geometry.x + target->geometry.width * 0.5,
                target->geometry.y + target->geometry.height * 0.5
            );
            mts::interaction::TargetId const panTarget = this->targetForGesture(current, position, true);

            return panTarget.valid() && this->dispatchPan(panTarget.packed(), delta);
        }

        case mts::interaction::NavigationCommand::ZoomIn:
        case mts::interaction::NavigationCommand::ZoomOut: {
            if (!target.has_value()) {
                return false;
            }

            QPointF const position(
                target->geometry.x + target->geometry.width * 0.5,
                target->geometry.y + target->geometry.height * 0.5
            );
            mts::interaction::TargetId const zoomTarget = this->targetForGesture(current, position, false);
            QQuickItem* const item = this->itemForTarget(zoomTarget.packed());
            if (!zoomTarget.valid() || item == nullptr) {
                return false;
            }

            qreal const delta = binding->command == mts::interaction::NavigationCommand::ZoomIn ? 1.0 : -1.0;

            return this->dispatchZoom(zoomTarget.packed(), delta, item->boundingRect().center());
        }

        case mts::interaction::NavigationCommand::Activate: {
            return this->activateTarget(current.packed());
        }

        case mts::interaction::NavigationCommand::SelectAll: {
            bool hasSelectableTarget = false;
            for (mts::interaction::TargetId const id : m_navigation->focusRing(domain)) {
                auto const candidate = m_registry->find(id);
                if (candidate.has_value() && candidate->capabilities.selectable) {
                    hasSelectableTarget = true;
                    break;
                }
            }
            if (!hasSelectableTarget) {
                return false;
            }

            this->selectAll(domainName);

            return true;
        }

        case mts::interaction::NavigationCommand::ShowContextMenu: {
            if (!target.has_value() || !target->capabilities.contextMenuable) {
                return false;
            }

            mts::interaction::CommandRequest request;
            request.kind = mts::interaction::CommandKind::OpenContextMenu;
            request.target = target->id;
            request.position = mts::interaction::Point{
                target->geometry.x + target->geometry.width * 0.5,
                target->geometry.y + target->geometry.height * 0.5,
            };

            return this->executeAndReport(request).succeeded();
        }

        case mts::interaction::NavigationCommand::DismissLayer: {
            if (this->dismissTopLayer()) {
                return true;
            }

            for (auto iterator = m_bindings.cbegin(); iterator != m_bindings.cend(); ++iterator) {
                if (iterator.value() == current.packed()) {
                    return QMetaObject::invokeMethod(iterator.key()->owner(), "cancelEditing", Qt::DirectConnection);
                }
            }

            return false;
        }
    }

    static_cast<void>(window);

    return false;
}

auto InteractionController::attachWindow(QQuickWindow* window) -> void {
    if (m_window == window || window == nullptr) {
        return;
    }

    m_window = window;
    QPointer<QQuickWindow> const guardedWindow(window);
    m_focusConnection = this->connect(window, &QQuickWindow::activeFocusItemChanged, this, [this, guardedWindow] {
        QTimer::singleShot(0, this, [this, guardedWindow] {
            if (guardedWindow != nullptr) {
                this->resolveFocus(guardedWindow);
            }
        });
    });
    QTimer::singleShot(0, this, [this, guardedWindow] {
        if (guardedWindow != nullptr) {
            this->resolveFocus(guardedWindow);
        }
    });
}

auto InteractionController::syncWindowEvent(QQuickWindow* window, QEvent* event) -> bool {
    switch (event->type()) {
        case QEvent::MouseMove: {
            auto const* mouse = static_cast<QMouseEvent*>(event);
            m_context->recordMove(toPoint(mouse->position()), modifiersFrom(mouse->modifiers()));
            emit this->pointerMoved(mouse->position());
            m_context->setHovered(mts::interaction::TargetId::fromPacked(this->hitTestTarget(window, mouse->position())));

            if (m_pointerPanTarget == 0) {
                break;
            }

            auto const panTarget = mts::interaction::TargetId::fromPacked(m_pointerPanTarget);
            auto const resolved = m_registry->find(panTarget);
            if (!resolved.has_value() || !resolved->capabilities.pannable) {
                m_pointerPanTarget = 0;
                break;
            }

            if (!m_layers->ownsInput(panTarget, *m_registry)) {
                m_pointerPanTarget = 0;

                return true;
            }

            QQuickItem* const item = this->itemForTarget(m_pointerPanTarget);
            if (item == nullptr) {
                m_pointerPanTarget = 0;
                break;
            }

            QPointF const previousPosition = item->mapFromScene(m_lastPointerPosition);
            QPointF const currentPosition = item->mapFromScene(mouse->position());
            m_lastPointerPosition = mouse->position();
            static_cast<void>(this->dispatchPan(m_pointerPanTarget, currentPosition - previousPosition));

            return true;
        }

        case QEvent::MouseButtonPress: {
            auto const* mouse = static_cast<QMouseEvent*>(event);
            mts::interaction::TargetId const target = mts::interaction::TargetId::fromPacked(
                this->hitTestTarget(window, mouse->position())
            );

            m_context->recordMove(toPoint(mouse->position()), modifiersFrom(mouse->modifiers()));
            emit this->pointerMoved(mouse->position());
            m_context->setHovered(target);
            m_context->recordPress(buttonFrom(mouse->button()), toPoint(mouse->position()), modifiersFrom(mouse->modifiers()));

            if (m_layers->outsideClickDismisses(target, *m_registry)) {
                this->dismissTopLayerOutside();

                return false;
            }

            if (!m_layers->ownsInput(target, *m_registry)) {
                return true;
            }

            if (target.valid()) {
                m_context->recordClick(target, buttonFrom(mouse->button()), toPoint(mouse->position()));
                auto const resolved = m_registry->find(target);
                if (resolved.has_value() && resolved->capabilities.focusable) {
                    this->focusTarget(target.packed());
                } else {
                    this->resolveFocus(window);
                }
                if (mouse->button() == Qt::MiddleButton) {
                    mts::interaction::TargetId const panTarget = this->targetForGesture(target, mouse->position(), true);
                    if (panTarget.valid()) {
                        m_pointerPanTarget = panTarget.packed();
                        m_lastPointerPosition = mouse->position();
                        static_cast<void>(window->setMouseGrabEnabled(true));

                        return true;
                    }
                }
            } else {
                m_context->setCurrent(mts::interaction::TargetId::invalid());
                this->resolveFocus(window);
            }
            break;
        }

        case QEvent::MouseButtonRelease: {
            auto const* mouse = static_cast<QMouseEvent*>(event);
            emit this->pointerMoved(mouse->position());
            m_context->recordRelease(buttonFrom(mouse->button()), toPoint(mouse->position()), modifiersFrom(mouse->modifiers()));
            bool const panRelease = mouse->button() == Qt::MiddleButton && m_pointerPanTarget != 0;
            if (panRelease) {
                m_pointerPanTarget = 0;
                static_cast<void>(window->setMouseGrabEnabled(false));
            }
            if (m_outsideDismissedObject != nullptr) {
                QPointer<QObject> const dismissed = m_outsideDismissedObject;
                m_outsideDismissedObject = nullptr;
                QTimer::singleShot(0, this, [this, dismissed] {
                    if (m_outsideDismissedObject == dismissed) {
                        m_outsideDismissedObject = nullptr;
                    }
                });
            }
            if (panRelease) {
                return true;
            }
            break;
        }

        case QEvent::Wheel: {
            auto const* wheel = static_cast<QWheelEvent*>(event);
            mts::interaction::TargetId const target = mts::interaction::TargetId::fromPacked(
                this->hitTestTarget(window, wheel->position())
            );
            m_context->recordMove(toPoint(wheel->position()), modifiersFrom(wheel->modifiers()));
            emit this->pointerMoved(wheel->position());
            m_context->setHovered(target);
            if (!m_layers->ownsInput(target, *m_registry)) {
                return true;
            }

            qreal const delta = wheelSteps(*wheel);
            if (!qFuzzyIsNull(delta)) {
                mts::interaction::TargetId const zoomTarget = this->targetForGesture(target, wheel->position(), false);
                QQuickItem* const item = this->itemForTarget(zoomTarget.packed());
                if (zoomTarget.valid() && item != nullptr) {
                    QPointF const position = item->mapFromScene(wheel->position());
                    if (this->dispatchZoom(zoomTarget.packed(), delta, position)) {
                        return true;
                    }
                }

                mts::interaction::Point const position = toPoint(wheel->position());
                std::optional<mts::interaction::TargetRef> scrollTarget;
                double scrollTargetArea = std::numeric_limits<double>::max();
                for (mts::interaction::TargetRef const& candidate : m_registry->targets()) {
                    bool const scrollSurface = candidate.role.name() == "scroll-area"
                        || candidate.role.name() == "code-block";
                    if (!scrollSurface
                        || !candidate.capabilities.scrollable
                        || !candidate.visible
                        || !candidate.enabled
                        || !candidate.geometry.contains(position)) {
                        continue;
                    }

                    double const area = candidate.geometry.width * candidate.geometry.height;
                    if (!scrollTarget.has_value() || area < scrollTargetArea) {
                        scrollTarget = candidate;
                        scrollTargetArea = area;
                    }
                }

                if (scrollTarget.has_value()) {
                    QQuickItem* const scrollItem = this->itemForTarget(scrollTarget->id.packed());
                    if (scrollItem != nullptr
                        && scrollItem->metaObject()->indexOfMethod("interactionScrollRequested(qreal)") >= 0
                        && QMetaObject::invokeMethod(
                            scrollItem,
                            "interactionScrollRequested",
                            Qt::DirectConnection,
                            Q_ARG(qreal, delta)
                        )) {
                        return true;
                    }
                }
            }
            break;
        }

        case QEvent::KeyPress:
        case QEvent::KeyRelease: {
            auto const* key = static_cast<QKeyEvent*>(event);
            m_context->recordModifiers(modifiersFrom(key->modifiers()));
            break;
        }

        case QEvent::WindowDeactivate: {
            if (m_pointerPanTarget != 0) {
                m_pointerPanTarget = 0;
                static_cast<void>(window->setMouseGrabEnabled(false));
            }
            break;
        }

        default: {
            break;
        }
    }

    return false;
}

auto InteractionController::resolveFocus(QQuickWindow* window) -> void {
    if (window == nullptr) {
        return;
    }

    QQuickItem const* focusItem = window->activeFocusItem();
    InteractionScope* scope = this->scopeForItem(focusItem);

    if (scope == nullptr) {
        m_context->setFocused(mts::interaction::TargetId::invalid());

        return;
    }

    auto const binding = m_bindings.find(scope);
    if (binding == m_bindings.end()) {
        return;
    }

    mts::interaction::TargetId const focused = mts::interaction::TargetId::fromPacked(binding.value());
    m_context->setFocused(focused);
    if (auto const target = m_registry->find(focused); target.has_value()) {
        m_navigation->remember(target->domain, focused);
    }
}

auto InteractionController::scopeForItem(QQuickItem const* item) const -> InteractionScope* {
    for (QQuickItem const* current = item; current != nullptr; current = current->parentItem()) {
        if (auto const found = m_ownerIndex.constFind(const_cast<QQuickItem*>(current)); found != m_ownerIndex.end()) {
            return found.value();
        }
    }

    return nullptr;
}

auto InteractionController::hitTestTarget(QQuickWindow* window, QPointF const& scenePosition) const -> quint64 {
    auto const providerTargets = m_registry->hitTest(mts::interaction::Point{scenePosition.x(), scenePosition.y()});
    for (mts::interaction::TargetRef const& target : providerTargets) {
        if (target.visible) {
            return target.id.packed();
        }
    }

    mts::interaction::TargetId best;
    double bestArea = std::numeric_limits<double>::max();
    std::size_t bestDomainDepth = 0;
    bool bestHitTestable = false;

    for (auto const& [owner, scope] : m_ownerIndex.asKeyValueRange()) {
        auto* item = qobject_cast<QQuickItem*>(owner);
        if (item == nullptr || item->window() != window || !item->isVisible() || item->opacity() <= 0.0) {
            continue;
        }

        QRectF const geometry = scope->sceneGeometry();
        if (geometry.isEmpty() || !geometry.contains(scenePosition)) {
            continue;
        }

        auto const binding = m_bindings.find(scope);
        if (binding == m_bindings.end()) {
            continue;
        }

        mts::interaction::TargetId const candidateId = mts::interaction::TargetId::fromPacked(binding.value());
        auto const candidate = m_registry->find(candidateId);
        if (!candidate.has_value()) {
            continue;
        }

        double const area = geometry.width() * geometry.height();
        std::size_t const domainDepth = static_cast<std::size_t>(std::count(
            candidate->domain.path().begin(),
            candidate->domain.path().end(),
            '.'
        ));
        bool const hitTestable = candidate->capabilities.hitTestable;
        bool const better = !best.valid()
            || area < bestArea
            || (area == bestArea && hitTestable && !bestHitTestable)
            || (area == bestArea && hitTestable == bestHitTestable && domainDepth > bestDomainDepth)
            || (area == bestArea && hitTestable == bestHitTestable && domainDepth == bestDomainDepth && candidateId.index < best.index);
        if (!better) {
            continue;
        }

        best = candidateId;
        bestArea = area;
        bestDomainDepth = domainDepth;
        bestHitTestable = hitTestable;
    }

    return best.packed();
}

auto InteractionController::itemForTarget(quint64 target) const -> QQuickItem* {
    for (auto const& [scope, packedTarget] : m_bindings.asKeyValueRange()) {
        if (packedTarget == target) {
            return qobject_cast<QQuickItem*>(scope->owner());
        }
    }

    return nullptr;
}

auto InteractionController::targetForGesture(
    mts::interaction::TargetId target,
    QPointF position,
    bool pan
) const -> mts::interaction::TargetId {
    auto const origin = m_registry->find(target);
    if (!origin.has_value()) {
        return mts::interaction::TargetId::invalid();
    }

    auto const supportsGesture = [pan](mts::interaction::TargetRef const& candidate) {
        return pan ? candidate.capabilities.pannable : candidate.capabilities.zoomable;
    };
    if (supportsGesture(*origin) && origin->visible && origin->enabled) {
        return target;
    }

    mts::interaction::TargetId best;
    std::size_t bestDomainSize = 0;
    double bestArea = std::numeric_limits<double>::max();
    for (mts::interaction::TargetRef const& candidate : m_registry->targets()) {
        if (!supportsGesture(candidate)
            || !candidate.visible
            || !candidate.enabled
            || !candidate.geometry.contains(toPoint(position))
            || !origin->domain.isWithin(candidate.domain)) {
            continue;
        }

        double const area = candidate.geometry.width * candidate.geometry.height;
        std::size_t const domainSize = candidate.domain.path().size();
        if (!best.valid() || domainSize > bestDomainSize || (domainSize == bestDomainSize && area < bestArea)) {
            best = candidate.id;
            bestDomainSize = domainSize;
            bestArea = area;
        }
    }

    return best;
}

auto InteractionController::dispatchPan(quint64 target, QPointF delta) -> bool {
    auto const resolved = m_registry->find(mts::interaction::TargetId::fromPacked(target));
    QQuickItem* const item = this->itemForTarget(target);
    if (!resolved.has_value() || !resolved->capabilities.pannable || item == nullptr) {
        return false;
    }

    return QMetaObject::invokeMethod(
        item,
        "interactionPan",
        Qt::DirectConnection,
        Q_ARG(qreal, delta.x()),
        Q_ARG(qreal, delta.y())
    );
}

auto InteractionController::dispatchZoom(quint64 target, qreal delta, QPointF position) -> bool {
    auto const resolved = m_registry->find(mts::interaction::TargetId::fromPacked(target));
    QQuickItem* const item = this->itemForTarget(target);
    if (!resolved.has_value() || !resolved->capabilities.zoomable || item == nullptr) {
        return false;
    }

    return QMetaObject::invokeMethod(
        item,
        "interactionZoom",
        Qt::DirectConnection,
        Q_ARG(qreal, delta),
        Q_ARG(qreal, position.x()),
        Q_ARG(qreal, position.y())
    );
}

auto InteractionController::onScopeCreated(InteractionScope* scope) -> void {
    if (scope == nullptr || !scope->hasIdentity()) {
        return;
    }

    auto const target = m_registry->attach(toDescriptor(*scope));
    quint64 const packed = target.packed();
    m_bindings.insert(scope, packed);
    m_ownerIndex.insert(scope->owner(), scope);
    scope->setTargetId(packed);
    emit this->registryChanged();
    m_actionRevision += 1;
    emit this->actionsChanged();
}

auto InteractionController::onScopeChanged(InteractionScope* scope) -> void {
    if (scope == nullptr || !scope->hasIdentity()) {
        return;
    }

    if (!m_bindings.contains(scope)) {
        this->onScopeCreated(scope);

        return;
    }

    m_registry->rekey(
        mts::interaction::TargetId::fromPacked(m_bindings.value(scope)),
        toDescriptor(*scope)
    );
    m_actionRevision += 1;
    emit this->actionsChanged();
}

auto InteractionController::onScopeDestroyed(InteractionScope* scope) -> void {
    auto const binding = m_bindings.find(scope);
    if (binding == m_bindings.end()) {
        return;
    }

    mts::interaction::TargetId const target = mts::interaction::TargetId::fromPacked(binding.value());

    if (m_context->focusedId() == target) {
        m_context->setFocused(mts::interaction::TargetId::invalid());
    }

    if (m_context->currentId() == target) {
        m_context->setCurrent(mts::interaction::TargetId::invalid());
    }

    if (m_context->hoveredId() == target) {
        m_context->setHovered(mts::interaction::TargetId::invalid());
    }

    if (m_context->lastClickedId() == target) {
        m_context->setLastClicked(mts::interaction::TargetId::invalid());
    }

    m_registry->detach(target);
    m_navigation->forget(target);
    m_ownerIndex.remove(scope->owner());
    m_bindings.erase(binding);
    emit this->registryChanged();
    m_actionRevision += 1;
    emit this->actionsChanged();
}

auto InteractionController::recordContextChange(mts::interaction::ContextChange const& change) -> void {
    mts::interaction::TargetId const target = change.target;
    mts::interaction::TargetId const previous = change.previous;
    std::string const domain = domainOf(*m_registry, target);
    mts::interaction::u64 const targetPacked = target.packed();
    mts::interaction::u64 const previousPacked = previous.packed();

    switch (change.kind) {
        case mts::interaction::ContextChangeKind::FocusChanged: {
            m_trace->record(mts::interaction::TraceKind::FocusChanged, target, mts::interaction::Domain(domain));
            if (m_bus != nullptr) {
                m_bus->publishDirect(mts::events::FocusChanged{previousPacked, targetPacked, domain});
            }
            mts::log::debug("focus changed {} -> {} ({})", previousPacked, targetPacked, domain);
            break;
        }

        case mts::interaction::ContextChangeKind::CurrentChanged: {
            m_trace->record(mts::interaction::TraceKind::CurrentChanged, target, mts::interaction::Domain(domain));
            if (m_bus != nullptr) {
                m_bus->publishDirect(mts::events::CurrentTargetChanged{previousPacked, targetPacked, domain});
            }
            mts::log::debug("current target changed {} -> {} ({})", previousPacked, targetPacked, domain);
            break;
        }

        case mts::interaction::ContextChangeKind::HoverChanged: {
            m_trace->record(mts::interaction::TraceKind::HoverChanged, target, mts::interaction::Domain(domain));
            if (m_bus != nullptr) {
                m_bus->publishDirect(mts::events::HoverChanged{previousPacked, targetPacked, domain});
            }
            mts::log::trace("hover changed {} -> {} ({})", previousPacked, targetPacked, domain);
            break;
        }

        case mts::interaction::ContextChangeKind::Pressed: {
            m_trace->record(mts::interaction::TraceKind::Pressed, target, mts::interaction::Domain(domain), std::format("{}@{},{}", static_cast<int>(change.button), change.position.x, change.position.y));
            mts::log::debug("pressed {} at {},{} ({})", static_cast<int>(change.button), change.position.x, change.position.y, domain);
            break;
        }

        case mts::interaction::ContextChangeKind::Released: {
            m_trace->record(mts::interaction::TraceKind::Released, target, mts::interaction::Domain(domain), std::format("{}@{},{}", static_cast<int>(change.button), change.position.x, change.position.y));
            mts::log::debug("released {} at {},{} ({})", static_cast<int>(change.button), change.position.x, change.position.y, domain);
            break;
        }

        case mts::interaction::ContextChangeKind::ModifiersChanged: {
            m_trace->record(mts::interaction::TraceKind::ModifiersChanged, target, mts::interaction::Domain(domain));
            mts::log::trace("modifiers changed ({} domain)", domain);
            break;
        }

        case mts::interaction::ContextChangeKind::Clicked: {
            m_trace->record(mts::interaction::TraceKind::Clicked, target, mts::interaction::Domain(domain), std::format("{}@{},{}", static_cast<int>(change.button), change.position.x, change.position.y));
            mts::log::debug("clicked {} {} at {},{} ({})", targetPacked, static_cast<int>(change.button), change.position.x, change.position.y, domain);
            break;
        }
    }
}

auto InteractionController::recordSelectionChange(mts::interaction::SelectionChange const& change) -> void {
    std::string const domain(change.domain.path());
    mts::interaction::TraceKind const kind = change.kind == mts::interaction::SelectionChangeKind::Added
        ? mts::interaction::TraceKind::SelectionAdded
        : (change.kind == mts::interaction::SelectionChangeKind::Removed
            ? mts::interaction::TraceKind::SelectionRemoved
            : mts::interaction::TraceKind::SelectionReset);

    auto const pack = [](std::vector<mts::interaction::TargetId> const& list) {
        std::vector<mts::interaction::u64> result;
        result.reserve(list.size());
        for (mts::interaction::TargetId const id : list) {
            result.push_back(id.packed());
        }

        return result;
    };

    m_trace->record(kind, mts::interaction::TargetId::invalid(), change.domain, std::format("{} targets", pack(change.added).size() + pack(change.removed).size()));

    if (m_bus != nullptr) {
        m_bus->publishDirect(mts::events::SelectionChanged{
            domain,
            m_selection->count(change.domain),
            pack(m_selection->ordered(change.domain)),
            pack(change.added),
            pack(change.removed),
        });
    }

    mts::log::debug("selection {} in {}: +{} -{} ({})", static_cast<int>(kind), domain, pack(change.added).size(), pack(change.removed).size(), m_selection->count(change.domain));
}

auto InteractionController::installDefaultActions() -> void {
    auto const alwaysVisible = [](mts::interaction::ActionContext const&) { return true; };
    auto const registerUiAction = [this, alwaysVisible](
        std::string id,
        std::string label,
        std::string domain,
        std::string role,
        int order,
        std::vector<mts::interaction::ActionSurface> surfaces,
        mts::interaction::ActionRequirements requirements,
        mts::interaction::ActionPolicy::Predicate enabled,
        std::string disabledReason
    ) {
        mts::interaction::Action action;
        action.id = std::move(id);
        action.label = std::move(label);
        if (action.id == "app.quit") {
            action.group = "secondary";
        } else if (action.id.starts_with("app.")) {
            action.group = "primary";
        }
        action.scope.domain = mts::interaction::Domain(std::move(domain));
        action.scope.role = mts::interaction::Role(std::move(role));
        action.order = order;
        action.surfaces = std::move(surfaces);
        action.requirements = requirements;
        action.disabledReason = std::move(disabledReason);
        std::string const actionId = action.id;
        action.policy = mts::interaction::ActionPolicy(
            alwaysVisible,
            std::move(enabled),
            [this, actionId](mts::interaction::ActionContext const& context) {
                QString const actionName = QString::fromStdString(actionId);
                this->recordActionInvocation(actionName, context.target.id);
                emit this->actionRequested(
                    actionName,
                    context.target.id.packed(),
                    QString::fromStdString(context.target.semanticKey)
                );
            }
        );
        m_actions->registerAction(std::move(action));
    };

    auto const truePredicate = [](mts::interaction::ActionContext const&) { return true; };
    auto const sessionNotRunning = [](mts::interaction::ActionContext const& context) {
        auto const running = context.businessValue("session.running");

        return !running.has_value() || *running != "true";
    };

    mts::interaction::ActionRequirements const menuTarget{.contextMenuable = true};
    mts::interaction::ActionRequirements const activatableTarget{.activatable = true};
    mts::interaction::ActionRequirements const mapTarget{
        .activatable = true,
        .contextMenuable = true,
        .hitTestable = true,
    };
    mts::interaction::ActionRequirements const none;

    registerUiAction(
        "session.rename",
        "Rename",
        "app.sessionList",
        "session-row",
        10,
        {mts::interaction::ActionSurface::ContextMenu},
        menuTarget,
        sessionNotRunning,
        "The running session cannot be renamed."
    );
    registerUiAction(
        "session.remove",
        "Remove",
        "app.sessionList",
        "session-row",
        20,
        {mts::interaction::ActionSurface::ContextMenu},
        menuTarget,
        sessionNotRunning,
        "The running session cannot be removed."
    );
    registerUiAction(
        "history.clear",
        "Clear history",
        "app.historyMenu",
        "layer-content",
        10,
        {mts::interaction::ActionSurface::Menu},
        activatableTarget,
        truePredicate,
        {}
    );
    registerUiAction(
        "app.about",
        "About",
        "app.appMenu",
        "layer-content",
        10,
        {mts::interaction::ActionSurface::Menu},
        activatableTarget,
        truePredicate,
        {}
    );
    registerUiAction(
        "app.quit",
        "Quit",
        "app",
        "",
        20,
        {mts::interaction::ActionSurface::Menu, mts::interaction::ActionSurface::Button},
        activatableTarget,
        truePredicate,
        {}
    );
    registerUiAction(
        "prompt.send",
        "Send message",
        "app.prompt",
        "input",
        10,
        {mts::interaction::ActionSurface::Button, mts::interaction::ActionSurface::Shortcut, mts::interaction::ActionSurface::ContextMenu},
        activatableTarget,
        [](mts::interaction::ActionContext const& context) {
            auto const text = context.businessValue("prompt.text");
            auto const running = context.businessValue("chat.running");

            return text.has_value()
                && !text->empty()
                && (!running.has_value() || *running != "true");
        },
        "Enter a message while the agent is idle."
    );
    registerUiAction(
        "prompt.cancel",
        "Stop response",
        "app.prompt",
        "input",
        20,
        {mts::interaction::ActionSurface::Button, mts::interaction::ActionSurface::ContextMenu},
        activatableTarget,
        [](mts::interaction::ActionContext const& context) {
            auto const running = context.businessValue("chat.running");

            return running.has_value() && *running == "true";
        },
        "There is no response to stop."
    );
    registerUiAction(
        "map.fit",
        "Fit map to view",
        "app.map",
        "surface",
        10,
        {mts::interaction::ActionSurface::ContextMenu},
        mapTarget,
        [](mts::interaction::ActionContext const& context) {
            auto const ready = context.businessValue("map.ready");

            return !ready.has_value() || *ready == "true";
        },
        "The map is not ready."
    );
    registerUiAction(
        "map.toggle",
        "Toggle map pane",
        "app",
        "window",
        30,
        {mts::interaction::ActionSurface::Button},
        activatableTarget,
        truePredicate,
        {}
    );

    m_actionRevision += 1;
    emit this->actionsChanged();
}

auto InteractionController::recordActionInvocation(QString const& actionId, mts::interaction::TargetId target) -> void {
    auto const resolved = m_registry->find(target);
    std::string const domain = resolved.has_value() ? std::string(resolved->domain.path()) : std::string{};
    std::string const semanticKey = resolved.has_value() ? resolved->semanticKey : std::string{};

    m_trace->record(
        mts::interaction::TraceKind::ActionInvoked,
        target,
        mts::interaction::Domain(domain),
        actionId.toStdString()
    );

    if (m_bus != nullptr) {
        m_bus->publishDirect(mts::events::ActionInvoked{
            actionId.toStdString(),
            target.packed(),
            domain,
            semanticKey,
        });
    }

    mts::log::debug("action invoked {} target={} domain={}", actionId.toStdString(), target.packed(), domain);
}

auto InteractionController::recordLayerChange(mts::interaction::LayerChange const& change) -> void {
    std::string const domain(change.layer.descriptor.domain.path());
    std::string const action = change.kind == mts::interaction::LayerChangeKind::Pushed ? "pushed" : "popped";
    mts::interaction::TargetId const target = change.layer.descriptor.owner;

    m_trace->record(
        mts::interaction::TraceKind::LayerChanged,
        target,
        change.layer.descriptor.domain,
        std::format("{} {} depth={}", action, change.layer.id, change.depth)
    );

    if (m_bus != nullptr) {
        m_bus->publishDirect(mts::events::LayerChanged{
            change.layer.id,
            change.depth,
            domain,
            action,
        });
    }

    mts::log::debug("layer {} {} ({}) depth={}", action, change.layer.id, domain, change.depth);
}

auto InteractionController::restoreFocus(mts::interaction::TargetId target) -> void {
    if (!target.valid()) {
        return;
    }

    for (auto iterator = m_bindings.cbegin(); iterator != m_bindings.cend(); ++iterator) {
        if (iterator.value() != target.packed()) {
            continue;
        }

        if (auto* item = qobject_cast<QQuickItem*>(iterator.key()->owner()); item != nullptr) {
            item->forceActiveFocus(Qt::OtherFocusReason);
            m_context->setFocused(target);
        }

        return;
    }
}
