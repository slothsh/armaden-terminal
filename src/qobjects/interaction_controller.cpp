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
    descriptor.order = scope.order();
    descriptor.geometry = mts::interaction::Rect{
        geometry.x(),
        geometry.y(),
        geometry.width(),
        geometry.height(),
    };
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
    , m_layers(std::make_unique<mts::interaction::LayerStack>()) {
    m_contextSubscription = m_context->subscribe([this](mts::interaction::ContextChange const& change) {
        this->recordContextChange(change);
        emit this->contextChanged();
    });

    m_selectionSubscription = m_selection->subscribe([this](mts::interaction::SelectionChange const& change) {
        this->recordSelectionChange(change);
    });

    m_layerSubscription = m_layers->subscribe([this](mts::interaction::LayerChange const& change) {
        this->recordLayerChange(change);
        emit this->layersChanged();
    });

    InteractionHub* hub = InteractionHub::instance();
    this->connect(hub, &InteractionHub::scopeCreated, this, &InteractionController::onScopeCreated);
    this->connect(hub, &InteractionHub::scopeChanged, this, &InteractionController::onScopeChanged);
    this->connect(hub, &InteractionHub::scopeDestroyed, this, &InteractionController::onScopeDestroyed);

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

auto InteractionController::context() -> mts::interaction::ContextStore& {
    return *m_context;
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
    if (auto* window = qobject_cast<QQuickWindow*>(watched); window != nullptr) {
        this->attachWindow(window);

        if (event->type() == QEvent::KeyPress) {
            auto const* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Escape && this->dismissTopLayer()) {
                event->accept();

                return true;
            }
        }

        if (this->syncWindowEvent(window, event)) {
            event->accept();

            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

auto InteractionController::attachWindow(QQuickWindow* window) -> void {
    if (m_window == window || window == nullptr) {
        return;
    }

    m_window = window;
    m_focusConnection = this->connect(window, &QQuickWindow::activeFocusItemChanged, this, [this, window] {
        this->resolveFocus(window);
    });
    this->resolveFocus(window);
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
            } else {
                m_context->setCurrent(mts::interaction::TargetId::invalid());
            }

            this->resolveFocus(window);
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

    m_context->setFocused(mts::interaction::TargetId::fromPacked(binding.value()));
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

    for (auto const& [owner, scope] : m_ownerIndex.asKeyValueRange()) {
        auto* item = qobject_cast<QQuickItem*>(owner);
        if (item == nullptr || item->window() != window || !item->isVisible() || item->opacity() <= 0.0) {
            continue;
        }

        QRectF const geometry = scope->sceneGeometry();
        if (geometry.isEmpty() || !geometry.contains(scenePosition)) {
            continue;
        }

        double const area = geometry.width() * geometry.height();
        if (best.valid() && area >= bestArea) {
            continue;
        }

        bestArea = area;

        auto const binding = m_bindings.find(scope);
        if (binding != m_bindings.end()) {
            best = mts::interaction::TargetId::fromPacked(binding.value());
        }
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
    m_ownerIndex.remove(scope->owner());
    m_bindings.erase(binding);
    emit this->registryChanged();
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
