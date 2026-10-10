#include "interaction_hub.hpp"

#include "interaction_scope.hpp"

InteractionHub::InteractionHub(QObject* parent)
    : QObject(parent) {}

auto InteractionHub::instance() -> InteractionHub* {
    static InteractionHub hub;

    return &hub;
}

auto InteractionHub::notifyScopeCreated(InteractionScope* scope) -> void {
    emit this->scopeCreated(scope);
}

auto InteractionHub::notifyScopeChanged(InteractionScope* scope) -> void {
    emit this->scopeChanged(scope);
}

auto InteractionHub::notifyScopeDestroyed(InteractionScope* scope) -> void {
    emit this->scopeDestroyed(scope);
}
