#include "interaction_controller.hpp"

#include "interaction_hub.hpp"
#include "interaction_scope.hpp"

import std;
import interaction;

namespace {

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
    , m_registry(std::make_unique<mts::interaction::TargetRegistry>()) {
    InteractionHub* hub = InteractionHub::instance();
    this->connect(hub, &InteractionHub::scopeCreated, this, &InteractionController::onScopeCreated);
    this->connect(hub, &InteractionHub::scopeChanged, this, &InteractionController::onScopeChanged);
    this->connect(hub, &InteractionHub::scopeDestroyed, this, &InteractionController::onScopeDestroyed);
}

InteractionController::~InteractionController() = default;

auto InteractionController::targetCount() const -> int {
    return static_cast<int>(m_registry->size());
}

auto InteractionController::registry() -> mts::interaction::TargetRegistry& {
    return *m_registry;
}

auto InteractionController::onScopeCreated(InteractionScope* scope) -> void {
    if (scope == nullptr || !scope->hasIdentity()) {
        return;
    }

    auto const target = m_registry->attach(toDescriptor(*scope));
    m_bindings.insert(scope, target.packed());
    scope->setTargetId(target.packed());
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

    m_registry->detach(mts::interaction::TargetId::fromPacked(binding.value()));
    m_bindings.erase(binding);
    emit this->registryChanged();
}
