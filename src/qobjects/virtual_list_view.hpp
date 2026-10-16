#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QJSValue>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickItem>
#include <QRectF>
#include <QTimer>
#include <QVariant>
#include <QVector>
#include <QtQml/qqmlregistration.h>

#include "virtual_list_layout.hpp"

class VirtualListView : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(VirtualListView)

    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY contentYChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY contentHeightChanged)
    Q_PROPERTY(qreal contentWidth READ contentWidth WRITE setContentWidth NOTIFY contentWidthChanged)
    Q_PROPERTY(qreal viewportHeight READ viewportHeight WRITE setViewportHeight NOTIFY viewportHeightChanged)
    Q_PROPERTY(qreal cacheBuffer READ cacheBuffer WRITE setCacheBuffer NOTIFY cacheBufferChanged)
    Q_PROPERTY(bool reuseItems READ reuseItems WRITE setReuseItems NOTIFY reuseItemsChanged)
    Q_PROPERTY(bool followTail READ followTail WRITE setFollowTail NOTIFY followTailChanged)
    Q_PROPERTY(qreal estimatedItemHeight READ estimatedItemHeight WRITE setEstimatedItemHeight NOTIFY estimatedItemHeightChanged)
    Q_PROPERTY(QJSValue sizeHint READ sizeHint WRITE setSizeHint NOTIFY sizeHintChanged)
    Q_PROPERTY(int maxItemsPerFrame READ maxItemsPerFrame WRITE setMaxItemsPerFrame NOTIFY maxItemsPerFrameChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int mountedCount READ mountedCount NOTIFY mountedCountChanged)
    Q_PROPERTY(bool atEnd READ atEnd NOTIFY scrollInfoChanged)
    Q_PROPERTY(bool atBeginning READ atBeginning NOTIFY scrollInfoChanged)
    Q_PROPERTY(qreal visibleRatio READ visibleRatio NOTIFY scrollInfoChanged)
    Q_PROPERTY(qreal positionRatio READ positionRatio NOTIFY scrollInfoChanged)

public:
    explicit VirtualListView(QQuickItem* parent = nullptr);
    ~VirtualListView() override;

    [[nodiscard]] auto model() const -> QAbstractItemModel*;
    auto setModel(QAbstractItemModel* model) -> void;

    [[nodiscard]] auto delegate() const -> QQmlComponent*;
    auto setDelegate(QQmlComponent* component) -> void;

    [[nodiscard]] auto contentY() const -> qreal;
    auto setContentY(qreal value) -> void;

    [[nodiscard]] auto contentHeight() const -> qreal;
    [[nodiscard]] auto contentWidth() const -> qreal;
    auto setContentWidth(qreal value) -> void;

    [[nodiscard]] auto viewportHeight() const -> qreal;
    auto setViewportHeight(qreal value) -> void;

    [[nodiscard]] auto cacheBuffer() const -> qreal;
    auto setCacheBuffer(qreal value) -> void;

    [[nodiscard]] auto reuseItems() const -> bool;
    auto setReuseItems(bool value) -> void;

    [[nodiscard]] auto followTail() const -> bool;
    auto setFollowTail(bool value) -> void;

    [[nodiscard]] auto estimatedItemHeight() const -> qreal;
    auto setEstimatedItemHeight(qreal value) -> void;

    [[nodiscard]] auto sizeHint() const -> QJSValue;
    auto setSizeHint(QJSValue const& value) -> void;

    [[nodiscard]] auto maxItemsPerFrame() const -> int;
    auto setMaxItemsPerFrame(int value) -> void;

    [[nodiscard]] auto count() const -> int;
    [[nodiscard]] auto mountedCount() const -> int;

    [[nodiscard]] auto atEnd() const -> bool;
    [[nodiscard]] auto atBeginning() const -> bool;
    [[nodiscard]] auto visibleRatio() const -> qreal;
    [[nodiscard]] auto positionRatio() const -> qreal;

    Q_INVOKABLE auto positionAtBeginning() -> void;
    Q_INVOKABLE auto positionAtEnd() -> void;
    [[nodiscard]] Q_INVOKABLE auto indexAt(qreal y) const -> int;
    Q_INVOKABLE auto itemOffset(int index) const -> qreal;
    Q_INVOKABLE auto invalidateSize(int index) -> void;
    Q_INVOKABLE auto invalidateAllSizes() -> void;
    Q_INVOKABLE auto requestRework() -> void;
    Q_INVOKABLE auto debugCounters() const -> QString;
    Q_INVOKABLE auto scrollToBottom() -> void;
    [[nodiscard]] Q_INVOKABLE auto itemForIndex(int index) const -> QObject*;

signals:
    void modelChanged();
    void delegateChanged();
    void contentYChanged();
    void contentHeightChanged();
    void contentWidthChanged();
    void viewportHeightChanged();
    void cacheBufferChanged();
    void reuseItemsChanged();
    void followTailChanged();
    void estimatedItemHeightChanged();
    void sizeHintChanged();
    void maxItemsPerFrameChanged();
    void countChanged();
    void mountedCountChanged();
    void scrollInfoChanged();
    void scrollAdjustmentRequested(qreal delta);
    void itemRecycled(int index, QObject* item);
    void itemActivated(int index, QObject* item);

protected:
    auto componentComplete() -> void override;
    auto geometryChange(QRectF const& newGeometry, QRectF const& oldGeometry) -> void override;
    auto itemChange(ItemChange change, ItemChangeData const& value) -> void override;
    auto updatePolish() -> void override;

private:
    struct MountedItem {
        QQuickItem* item = nullptr;
        qreal trackedHeight = 0.0;
        QMetaObject::Connection heightConnection;
        QMetaObject::Connection implicitHeightConnection;
        QMetaObject::Connection widthConnection;
        QVector<QByteArray> boundRoles;
    };

    [[nodiscard]] auto isReady() const -> bool;
    [[nodiscard]] auto delegateWidth() const -> qreal;
    [[nodiscard]] auto effectiveViewportHeight() const -> qreal;

    auto connectModel() -> void;
    auto disconnectModel() -> void;

    auto handleRowsInserted(QModelIndex const& parent, int first, int last) -> void;
    auto handleRowsRemoved(QModelIndex const& parent, int first, int last) -> void;
    auto handleRowsMoved(
        QModelIndex const& sourceParent,
        int sourceStart,
        int sourceEnd,
        QModelIndex const& destinationParent,
        int destinationRow
    ) -> void;
    auto handleModelReset() -> void;
    auto handleDataChanged(QModelIndex const& topLeft, QModelIndex const& bottomRight, QVector<int> const& roles) -> void;

    auto resetLayout() -> void;
    auto rework() -> void;
    auto reworkOnce() -> void;
    auto scheduleRework() -> void;
    auto queueScrollAdjustment(qreal delta) -> void;
    auto releaseAllMounted() -> void;
    auto mountRange() const -> VirtualListLayout::Range;
    auto updateMounted(bool wasAtEnd, bool follow, VirtualListLayout::Anchor const& anchor) -> void;
    auto acquireItem(int index) -> QQuickItem*;
    auto releaseItem(int index) -> void;
    auto bindItemIndex(QQuickItem* item, int index) -> void;
    auto bindItem(int index, QQuickItem* item) -> void;
    auto clearItemRoles(QQuickItem* item, QVector<QByteArray> const& roles) -> void;
    auto positionMounted() -> void;
    auto measureItem(int index, qreal height) -> void;
    auto handleItemHeightChanged(QQuickItem* item) -> void;
    auto clampContentY() -> void;
    auto publishContentHeight() -> void;
    auto rebuildEstimates() -> void;
    auto estimateForIndex(int index) -> qreal;
    auto destroyPooledItems() -> void;

    VirtualListLayout m_layout;
    QPointer<QAbstractItemModel> m_model;
    QPointer<QQmlComponent> m_delegate;
    QHash<int, MountedItem> m_mounted;
    QHash<QQuickItem*, int> m_itemIndex;
    QVector<QQuickItem*> m_pool;
    QTimer* m_reworkTimer = nullptr;
    QTimer* m_adjustTimer = nullptr;
    qreal m_pendingAdjustment = 0.0;
    QVector<QMetaObject::Connection> m_modelConnections;
    qreal m_contentY = 0.0;
    qreal m_contentWidth = -1.0;
    qreal m_viewportHeight = -1.0;
    qreal m_cacheBuffer = 320.0;
    qreal m_estimatedItemHeight = 48.0;
    bool m_reuseItems = true;
    bool m_followTail = false;
    bool m_componentComplete = false;
    bool m_updating = false;
    bool m_layoutDirty = false;
    bool m_estimatesDirty = true;
    bool m_reworkActive = false;
    int m_maxItemsPerFrame = 8;
    int m_createdThisFrame = 0;
    int m_publishedCount = 0;
    int m_debugReworks = 0;
    int m_debugCreates = 0;
    int m_debugPoolBinds = 0;
    int m_debugReleases = 0;
    int m_debugEstimateCalls = 0;
    int m_debugDataBinds = 0;
    qreal m_publishedHeight = -1.0;
    QJSValue m_sizeHint;
};