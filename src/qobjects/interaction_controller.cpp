#include "interaction_controller.hpp"

#include "interaction_hub.hpp"
#include "interaction_scope.hpp"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickItem>

import std;
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
    , m_context(std::make_unique<mts::interaction::ContextStore>(*m_registry)) {
    m_contextSubscription = m_context->subscribe([this](mts::interaction::ContextChange const&) {
        emit this->contextChanged();
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

auto InteractionController::registry() -> mts::interaction::TargetRegistry& {
    return *m_registry;
}

auto InteractionController::context() -> mts::interaction::ContextStore& {
    return *m_context;
}

auto InteractionController::eventFilter(QObject* watched, QEvent* event) -> bool {
    if (auto* window = qobject_cast<QQuickWindow*>(watched); window != nullptr) {
        this->attachWindow(window);
        this->syncWindowEvent(window, event);
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

auto InteractionController::syncWindowEvent(QQuickWindow* window, QEvent* event) -> void {
    switch (event->type()) {
        case QEvent::MouseMove: {
            auto const* mouse = static_cast<QMouseEvent*>(event);
            m_context->recordMove(toPoint(mouse->position()), modifiersFrom(mouse->modifiers()));
            m_context->setHovered(mts::interaction::TargetId::fromPacked(this->hitTestTarget(window, mouse->position())));
            break;
        }

        case QEvent::MouseButtonPress: {
            auto const* mouse = static_cast<QMouseEvent*>(event);
            mts::interaction::PointerButton const button = buttonFrom(mouse->button());
            m_context->recordPress(button, toPoint(mouse->position()), modifiersFrom(mouse->modifiers()));
            m_context->setHovered(mts::interaction::TargetId::fromPacked(this->hitTestTarget(window, mouse->position())));

            mts::interaction::TargetId const target = m_context->hoveredId();
            if (target.valid()) {
                m_context->recordClick(target, button, toPoint(mouse->position()));
            } else {
                m_context->setCurrent(mts::interaction::TargetId::invalid());
            }

            this->resolveFocus(window);
            break;
        }

        case QEvent::MouseButtonRelease: {
            auto const* mouse = static_cast<QMouseEvent*>(event);
            m_context->recordRelease(buttonFrom(mouse->button()), toPoint(mouse->position()), modifiersFrom(mouse->modifiers()));
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
