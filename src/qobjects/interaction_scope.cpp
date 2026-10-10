#include "interaction_scope.hpp"

#include <QQuickItem>

#include "interaction_hub.hpp"

auto InteractionScope::qmlAttachedProperties(QObject* object) -> InteractionScope* {
    return new InteractionScope(object);
}

InteractionScope::InteractionScope(QObject* object)
    : QObject(object)
    , m_owner(object) {
    if (auto* item = qobject_cast<QQuickItem*>(object); item != nullptr) {
        this->connect(item, &QQuickItem::xChanged, this, [this] { this->announceChanged(); });
        this->connect(item, &QQuickItem::yChanged, this, [this] { this->announceChanged(); });
        this->connect(item, &QQuickItem::widthChanged, this, [this] { this->announceChanged(); });
        this->connect(item, &QQuickItem::heightChanged, this, [this] { this->announceChanged(); });
        this->connect(item, &QQuickItem::visibleChanged, this, [this] { this->announceChanged(); });
        this->connect(item, &QQuickItem::enabledChanged, this, [this] { this->announceChanged(); });
    }

    InteractionHub::instance()->notifyScopeCreated(this);
}

InteractionScope::~InteractionScope() {
    InteractionHub::instance()->notifyScopeDestroyed(this);
}

auto InteractionScope::domain() const -> QString {
    return m_domain;
}

auto InteractionScope::setDomain(QString const& value) -> void {
    if (m_domain == value) {
        return;
    }

    m_domain = value;
    emit this->domainChanged();
    this->announceChanged();
}

auto InteractionScope::role() const -> QString {
    return m_role;
}

auto InteractionScope::setRole(QString const& value) -> void {
    if (m_role == value) {
        return;
    }

    m_role = value;
    emit this->roleChanged();
    this->announceChanged();
}

auto InteractionScope::label() const -> QString {
    return m_label;
}

auto InteractionScope::setLabel(QString const& value) -> void {
    if (m_label == value) {
        return;
    }

    m_label = value;
    emit this->labelChanged();
    this->announceChanged();
}

auto InteractionScope::group() const -> QString {
    return m_group;
}

auto InteractionScope::setGroup(QString const& value) -> void {
    if (m_group == value) {
        return;
    }

    m_group = value;
    emit this->groupChanged();
    this->announceChanged();
}

auto InteractionScope::semanticKey() const -> QString {
    return m_semanticKey;
}

auto InteractionScope::setSemanticKey(QString const& value) -> void {
    if (m_semanticKey == value) {
        return;
    }

    m_semanticKey = value;
    emit this->semanticKeyChanged();
    this->announceChanged();
}

auto InteractionScope::order() const -> int {
    return m_order;
}

auto InteractionScope::setOrder(int value) -> void {
    if (m_order == value) {
        return;
    }

    m_order = value;
    emit this->orderChanged();
    this->announceChanged();
}

auto InteractionScope::selectionMode() const -> QString {
    return m_selectionMode;
}

auto InteractionScope::setSelectionMode(QString const& value) -> void {
    if (m_selectionMode == value) {
        return;
    }

    m_selectionMode = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::focusable() const -> bool {
    return m_focusable;
}

auto InteractionScope::setFocusable(bool value) -> void {
    if (m_focusable == value) {
        return;
    }

    m_focusable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::selectable() const -> bool {
    return m_selectable;
}

auto InteractionScope::setSelectable(bool value) -> void {
    if (m_selectable == value) {
        return;
    }

    m_selectable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::activatable() const -> bool {
    return m_activatable;
}

auto InteractionScope::setActivatable(bool value) -> void {
    if (m_activatable == value) {
        return;
    }

    m_activatable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::contextMenuable() const -> bool {
    return m_contextMenuable;
}

auto InteractionScope::setContextMenuable(bool value) -> void {
    if (m_contextMenuable == value) {
        return;
    }

    m_contextMenuable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::navigable() const -> bool {
    return m_navigable;
}

auto InteractionScope::setNavigable(bool value) -> void {
    if (m_navigable == value) {
        return;
    }

    m_navigable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::editable() const -> bool {
    return m_editable;
}

auto InteractionScope::setEditable(bool value) -> void {
    if (m_editable == value) {
        return;
    }

    m_editable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::draggable() const -> bool {
    return m_draggable;
}

auto InteractionScope::setDraggable(bool value) -> void {
    if (m_draggable == value) {
        return;
    }

    m_draggable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::scrollable() const -> bool {
    return m_scrollable;
}

auto InteractionScope::setScrollable(bool value) -> void {
    if (m_scrollable == value) {
        return;
    }

    m_scrollable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::hitTestable() const -> bool {
    return m_hitTestable;
}

auto InteractionScope::setHitTestable(bool value) -> void {
    if (m_hitTestable == value) {
        return;
    }

    m_hitTestable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::pannable() const -> bool {
    return m_pannable;
}

auto InteractionScope::setPannable(bool value) -> void {
    if (m_pannable == value) {
        return;
    }

    m_pannable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::zoomable() const -> bool {
    return m_zoomable;
}

auto InteractionScope::setZoomable(bool value) -> void {
    if (m_zoomable == value) {
        return;
    }

    m_zoomable = value;
    emit this->capabilitiesChanged();
    this->announceChanged();
}

auto InteractionScope::targetId() const -> quint64 {
    return m_targetId;
}

auto InteractionScope::setTargetId(quint64 value) -> void {
    if (m_targetId == value) {
        return;
    }

    m_targetId = value;
    emit this->targetIdChanged();
}

auto InteractionScope::owner() const -> QObject* {
    return m_owner;
}

auto InteractionScope::hasIdentity() const -> bool {
    return !m_domain.isEmpty() && !m_role.isEmpty();
}

auto InteractionScope::sceneGeometry() const -> QRectF {
    if (auto* item = qobject_cast<QQuickItem*>(m_owner); item != nullptr) {
        return item->mapRectToScene(QRectF(0.0, 0.0, item->width(), item->height()));
    }

    return QRectF{};
}

auto InteractionScope::markDirty() -> void {
    this->announceChanged();
}

auto InteractionScope::announceChanged() -> void {
    InteractionHub::instance()->notifyScopeChanged(this);
}
