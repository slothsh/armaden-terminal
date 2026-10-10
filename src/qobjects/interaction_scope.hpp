#pragma once

#include <QObject>
#include <QRectF>
#include <QString>
#include <QtQml/qqmlregistration.h>

class InteractionScope : public QObject {
    Q_OBJECT
    QML_ATTACHED(InteractionScope)
    QML_NAMED_ELEMENT(InteractionScope)
    QML_UNCREATABLE("InteractionScope is used as an attached property")

    Q_PROPERTY(QString domain READ domain WRITE setDomain NOTIFY domainChanged)
    Q_PROPERTY(QString role READ role WRITE setRole NOTIFY roleChanged)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged)
    Q_PROPERTY(int order READ order WRITE setOrder NOTIFY orderChanged)
    Q_PROPERTY(QString selectionMode READ selectionMode WRITE setSelectionMode NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool focusable READ focusable WRITE setFocusable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool selectable READ selectable WRITE setSelectable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool activatable READ activatable WRITE setActivatable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool contextMenuable READ contextMenuable WRITE setContextMenuable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool navigable READ navigable WRITE setNavigable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool editable READ editable WRITE setEditable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool draggable READ draggable WRITE setDraggable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool scrollable READ scrollable WRITE setScrollable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hitTestable READ hitTestable WRITE setHitTestable NOTIFY capabilitiesChanged)
    Q_PROPERTY(quint64 targetId READ targetId WRITE setTargetId NOTIFY targetIdChanged)

public:
    explicit InteractionScope(QObject* object);
    ~InteractionScope() override;

    [[nodiscard]] static auto qmlAttachedProperties(QObject* object) -> InteractionScope*;

    [[nodiscard]] auto domain() const -> QString;
    auto setDomain(QString const& value) -> void;

    [[nodiscard]] auto role() const -> QString;
    auto setRole(QString const& value) -> void;

    [[nodiscard]] auto label() const -> QString;
    auto setLabel(QString const& value) -> void;

    [[nodiscard]] auto group() const -> QString;
    auto setGroup(QString const& value) -> void;

    [[nodiscard]] auto order() const -> int;
    auto setOrder(int value) -> void;

    [[nodiscard]] auto selectionMode() const -> QString;
    auto setSelectionMode(QString const& value) -> void;

    [[nodiscard]] auto focusable() const -> bool;
    auto setFocusable(bool value) -> void;

    [[nodiscard]] auto selectable() const -> bool;
    auto setSelectable(bool value) -> void;

    [[nodiscard]] auto activatable() const -> bool;
    auto setActivatable(bool value) -> void;

    [[nodiscard]] auto contextMenuable() const -> bool;
    auto setContextMenuable(bool value) -> void;

    [[nodiscard]] auto navigable() const -> bool;
    auto setNavigable(bool value) -> void;

    [[nodiscard]] auto editable() const -> bool;
    auto setEditable(bool value) -> void;

    [[nodiscard]] auto draggable() const -> bool;
    auto setDraggable(bool value) -> void;

    [[nodiscard]] auto scrollable() const -> bool;
    auto setScrollable(bool value) -> void;

    [[nodiscard]] auto hitTestable() const -> bool;
    auto setHitTestable(bool value) -> void;

    [[nodiscard]] auto targetId() const -> quint64;
    auto setTargetId(quint64 value) -> void;

    [[nodiscard]] auto owner() const -> QObject*;
    [[nodiscard]] auto hasIdentity() const -> bool;
    [[nodiscard]] auto sceneGeometry() const -> QRectF;

    Q_INVOKABLE auto markDirty() -> void;

signals:
    auto domainChanged() -> void;
    auto roleChanged() -> void;
    auto labelChanged() -> void;
    auto groupChanged() -> void;
    auto orderChanged() -> void;
    auto capabilitiesChanged() -> void;
    auto targetIdChanged() -> void;

private:
    auto announceChanged() -> void;

    QObject* m_owner = nullptr;
    quint64 m_targetId = 0;
    QString m_domain;
    QString m_role;
    QString m_label;
    QString m_group;
    int m_order = 0;
    QString m_selectionMode;
    bool m_focusable = false;
    bool m_selectable = false;
    bool m_activatable = false;
    bool m_contextMenuable = false;
    bool m_navigable = false;
    bool m_editable = false;
    bool m_draggable = false;
    bool m_scrollable = false;
    bool m_hitTestable = false;
};
