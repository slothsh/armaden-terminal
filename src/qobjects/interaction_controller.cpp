#include "interaction_controller.hpp"

#include "interaction_hub.hpp"
#include "interaction_scope.hpp"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QTimer>

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

auto InteractionController::context() -> mts::interaction::ContextStore& {
    return *m_context;
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
    mts::interaction::TargetId targetId = mts::interaction::TargetId::fromPacked(target);
    if (!targetId.valid()) {
        targetId = m_context->currentId();
    }
    if (!targetId.valid()) {
        targetId = m_context->focusedId();
    }

    auto const resolved = m_registry->find(targetId);
    if (!resolved.has_value()) {
        return false;
    }

    std::vector<mts::interaction::ActionArgument> actionArguments;
    actionArguments.reserve(static_cast<std::size_t>(arguments.size()));
    for (auto iterator = arguments.cbegin(); iterator != arguments.cend(); ++iterator) {
        actionArguments.push_back(mts::interaction::ActionArgument{
            iterator.key().toStdString(),
            iterator.value().toString().toStdString(),
        });
    }

    mts::interaction::ActionContext const context = m_actions->makeContext(
        *resolved,
        *m_context,
        *m_selection,
        std::move(actionArguments)
    );
    auto const result = m_actions->invoke(actionId.toStdString(), context);
    if (!result.invoked()) {
        return false;
    }

    this->recordActionInvocation(actionId, targetId);

    return true;
}

auto InteractionController::setBusinessValue(QString const& key, QString const& value) -> void {
    m_actions->setBusinessValue(key.toStdString(), value.toStdString());
    m_actionRevision += 1;
    emit this->actionsChanged();
}

auto InteractionController::focusTarget(quint64 target) -> bool {
    mts::interaction::TargetId const id = mts::interaction::TargetId::fromPacked(target);
    auto const resolved = m_registry->find(id);
    if (!resolved.has_value()
        || !resolved->capabilities.focusable
        || !resolved->visible
        || !resolved->enabled
        || !m_layers->ownsInput(id, *m_registry)) {
        return false;
    }

    for (auto iterator = m_bindings.cbegin(); iterator != m_bindings.cend(); ++iterator) {
        if (iterator.value() != target) {
            continue;
        }

        if (auto* item = qobject_cast<QQuickItem*>(iterator.key()->owner()); item != nullptr) {
            item->forceActiveFocus(Qt::TabFocusReason);
            m_context->setFocused(id);
            m_navigation->remember(resolved->domain, id);

            return true;
        }
    }

    return false;
}

auto InteractionController::focusNext(QString const& domain, QString const& group, bool wrap) -> bool {
    mts::interaction::TargetId current = m_context->focusedId();
    if (!current.valid()) {
        auto const remembered = m_navigation->remembered(mts::interaction::Domain(domain.toStdString()));
        current = remembered.value_or(m_context->currentId());
    }

    auto const target = m_navigation->focusNext(
        mts::interaction::Domain(domain.toStdString()),
        current,
        wrap,
        group.toStdString()
    );

    return target.has_value() && this->focusTarget(target->packed());
}

auto InteractionController::focusPrevious(QString const& domain, QString const& group, bool wrap) -> bool {
    mts::interaction::TargetId current = m_context->focusedId();
    if (!current.valid()) {
        auto const remembered = m_navigation->remembered(mts::interaction::Domain(domain.toStdString()));
        current = remembered.value_or(m_context->currentId());
    }

    auto const target = m_navigation->focusPrevious(
        mts::interaction::Domain(domain.toStdString()),
        current,
        wrap,
        group.toStdString()
    );

    return target.has_value() && this->focusTarget(target->packed());
}

auto InteractionController::focusFirst(QString const& domain, QString const& group) -> bool {
    auto const target = m_navigation->focusFirst(
        mts::interaction::Domain(domain.toStdString()),
        group.toStdString()
    );

    return target.has_value() && this->focusTarget(target->packed());
}

auto InteractionController::focusLast(QString const& domain, QString const& group) -> bool {
    auto const target = m_navigation->focusLast(
        mts::interaction::Domain(domain.toStdString()),
        group.toStdString()
    );

    return target.has_value() && this->focusTarget(target->packed());
}

auto InteractionController::focusDirection(
    QString const& domain,
    QString const& direction,
    QString const& group
) -> bool {
    auto const navigationDirection = directionFrom(direction);
    if (!navigationDirection.has_value()) {
        return false;
    }

    mts::interaction::TargetId current = m_context->focusedId();
    if (!current.valid()) {
        current = m_context->currentId();
    }

    auto const target = m_navigation->focusDirection(
        mts::interaction::Domain(domain.toStdString()),
        current,
        *navigationDirection,
        group.toStdString()
    );
    if (!target.has_value()) {
        return false;
    }

    auto const resolved = m_registry->find(*target);
    if (!resolved.has_value()) {
        return false;
    }

    bool const focused = this->focusTarget(target->packed());
    if (focused && resolved->capabilities.selectable) {
        m_selection->select(resolved->domain, *target, mts::interaction::SelectionIntent::Replace);
    }

    return focused;
}

auto InteractionController::focusWithin(QString const& domain, QString const& group) -> bool {
    return this->focusFirst(domain, group);
}

auto InteractionController::activateTarget(quint64 target) -> bool {
    auto const resolved = m_registry->find(mts::interaction::TargetId::fromPacked(target));
    if (!resolved.has_value()
        || !resolved->capabilities.activatable
        || !resolved->visible
        || !resolved->enabled
        || !m_layers->ownsInput(resolved->id, *m_registry)) {
        return false;
    }

    for (auto iterator = m_bindings.cbegin(); iterator != m_bindings.cend(); ++iterator) {
        if (iterator.value() != target) {
            continue;
        }

        QObject* const owner = iterator.key()->owner();
        return QMetaObject::invokeMethod(owner, "activate", Qt::DirectConnection);
    }

    return false;
}

auto InteractionController::selectAll(QString const& domain) -> void {
    mts::interaction::Domain const scope(domain.toStdString());
    std::vector<mts::interaction::TargetId> candidates;
    for (mts::interaction::TargetId const id : m_navigation->focusRing(scope)) {
        auto const target = m_registry->find(id);
        if (target.has_value() && target->capabilities.selectable) {
            candidates.push_back(id);
        }
    }

    m_selection->selectAll(scope, candidates);
}

auto InteractionController::extendSelection(QString const& domain, QString const& direction) -> bool {
    auto const navigationDirection = directionFrom(direction);
    if (!navigationDirection.has_value()) {
        return false;
    }

    mts::interaction::TargetId current = m_context->focusedId();
    if (!current.valid()) {
        current = m_context->currentId();
    }

    mts::interaction::Domain const scope(domain.toStdString());
    auto const currentTarget = m_registry->find(current);
    auto const target = m_navigation->focusDirection(
        scope,
        current,
        *navigationDirection,
        currentTarget.has_value() ? std::string_view(currentTarget->group) : std::string_view{}
    );
    if (!target.has_value()) {
        return false;
    }

    auto const nextTarget = m_registry->find(*target);
    if (!currentTarget.has_value()
        || !nextTarget.has_value()
        || !currentTarget->capabilities.selectable
        || !nextTarget->capabilities.selectable) {
        return false;
    }

    if (!m_selection->anchor(scope).valid()) {
        m_selection->setAnchor(scope, current);
    }

    m_selection->select(scope, *target, mts::interaction::SelectionIntent::Range);

    return this->focusTarget(target->packed());
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

    quint64 const id = m_layers->push(std::move(descriptor));
    m_layerObjects.insert(id, object);

    return id;
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
    auto const layer = m_layers->find(id);
    if (!layer.has_value()) {
        return;
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
        this->popLayer(id);

        return true;
    }

    if (!QMetaObject::invokeMethod(object, "dismissLayer", Qt::DirectConnection)) {
        if (!QMetaObject::invokeMethod(object, "close", Qt::DirectConnection)) {
            this->popLayer(id);
        }
    }

    return true;
}

auto InteractionController::closeLayerOutside(quint64 id) -> bool {
    QPointer<QObject> const object = m_layerObjects.value(id);
    if (object == nullptr) {
        this->popLayer(id);

        return true;
    }

    m_outsideDismissedObject = object;
    if (!QMetaObject::invokeMethod(object, "dismissLayerOutside", Qt::DirectConnection)) {
        if (!QMetaObject::invokeMethod(object, "dismissLayer", Qt::DirectConnection)) {
            if (!QMetaObject::invokeMethod(object, "close", Qt::DirectConnection)) {
                this->popLayer(id);
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
    if (target.has_value() && target->capabilities.editable && !traversal && !layerDismiss && !contextMenu) {
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

            emit this->contextMenuForTarget(
                target->id.packed(),
                target->geometry.x + target->geometry.width * 0.5,
                target->geometry.y + target->geometry.height * 0.5
            );

            return true;
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
            m_context->setHovered(mts::interaction::TargetId::fromPacked(this->hitTestTarget(window, mouse->position())));
            break;
        }

        case QEvent::MouseButtonPress: {
            auto const* mouse = static_cast<QMouseEvent*>(event);
            mts::interaction::TargetId const target = mts::interaction::TargetId::fromPacked(
                this->hitTestTarget(window, mouse->position())
            );

            m_context->recordMove(toPoint(mouse->position()), modifiersFrom(mouse->modifiers()));
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
            } else {
                m_context->setCurrent(mts::interaction::TargetId::invalid());
                this->resolveFocus(window);
            }
            break;
        }

        case QEvent::MouseButtonRelease: {
            auto const* mouse = static_cast<QMouseEvent*>(event);
            m_context->recordRelease(buttonFrom(mouse->button()), toPoint(mouse->position()), modifiersFrom(mouse->modifiers()));
            if (m_outsideDismissedObject != nullptr) {
                QPointer<QObject> const dismissed = m_outsideDismissedObject;
                m_outsideDismissedObject = nullptr;
                QTimer::singleShot(0, this, [this, dismissed] {
                    if (m_outsideDismissedObject == dismissed) {
                        m_outsideDismissedObject = nullptr;
                    }
                });
            }
            break;
        }

        case QEvent::KeyPress:
        case QEvent::KeyRelease: {
            auto const* key = static_cast<QKeyEvent*>(event);
            m_context->recordModifiers(modifiersFrom(key->modifiers()));
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
                emit this->actionRequested(
                    QString::fromStdString(actionId),
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
