#include "virtual_list_view.hpp"

#include <QAbstractItemModel>
#include <QJSEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTimer>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr qreal kMinimumItemHeight = 1.0;
constexpr qreal kCorrectionEpsilon = 0.01;

auto sanitizeHeight(qreal height) -> qreal {
    if (!std::isfinite(height) || height < kMinimumItemHeight) {
        return kMinimumItemHeight;
    }

    return height;
}

} // END OF NAMESPACE anonymous

VirtualListView::VirtualListView(QQuickItem* parent)
    : QQuickItem(parent)
    , m_reworkTimer(new QTimer(this))
    , m_adjustTimer(new QTimer(this)) {
    m_reworkTimer->setSingleShot(true);
    m_reworkTimer->setInterval(0);
    QObject::connect(m_reworkTimer, &QTimer::timeout, this, [this] { this->rework(); });

    m_adjustTimer->setSingleShot(true);
    m_adjustTimer->setInterval(0);
    QObject::connect(
        m_adjustTimer,
        &QTimer::timeout,
        this,
        [this] {
            qreal const delta = m_pendingAdjustment;
            m_pendingAdjustment = 0.0;
            if (std::abs(delta) > kCorrectionEpsilon) {
                emit scrollAdjustmentRequested(delta);
            }
        }
    );

    this->setClip(true);
}

VirtualListView::~VirtualListView() {
    this->disconnectModel();
}

auto VirtualListView::model() const -> QAbstractItemModel* {
    return m_model.data();
}

auto VirtualListView::setModel(QAbstractItemModel* model) -> void {
    if (m_model.data() == model) {
        return;
    }

    this->disconnectModel();
    m_model = model;
    emit modelChanged();

    if (m_model != nullptr) {
        this->connectModel();
    }

    this->resetLayout();
    this->scheduleRework();
}

auto VirtualListView::delegate() const -> QQmlComponent* {
    return m_delegate.data();
}

auto VirtualListView::setDelegate(QQmlComponent* component) -> void {
    if (m_delegate.data() == component) {
        return;
    }

    m_delegate = component;
    emit delegateChanged();

    this->releaseAllMounted();
    this->destroyPooledItems();

    emit mountedCountChanged();
    this->scheduleRework();
}

auto VirtualListView::contentY() const -> qreal {
    return m_contentY;
}

auto VirtualListView::setContentY(qreal value) -> void {
    if (qFuzzyCompare(m_contentY, value)) {
        return;
    }

    m_contentY = value;
    emit contentYChanged();
    emit scrollInfoChanged();

    if (m_reworkActive) {
        m_layoutDirty = true;
        return;
    }

    this->rework();
}

auto VirtualListView::contentHeight() const -> qreal {
    return m_layout.totalHeight();
}

auto VirtualListView::contentWidth() const -> qreal {
    return m_contentWidth;
}

auto VirtualListView::setContentWidth(qreal value) -> void {
    if (qFuzzyCompare(m_contentWidth, value)) {
        return;
    }

    m_contentWidth = value;
    emit contentWidthChanged();
}

auto VirtualListView::viewportHeight() const -> qreal {
    return m_viewportHeight;
}

auto VirtualListView::setViewportHeight(qreal value) -> void {
    if (qFuzzyCompare(m_viewportHeight, value)) {
        return;
    }

    m_viewportHeight = value;
    emit viewportHeightChanged();
    this->scheduleRework();
}

auto VirtualListView::cacheBuffer() const -> qreal {
    return m_cacheBuffer;
}

auto VirtualListView::setCacheBuffer(qreal value) -> void {
    if (qFuzzyCompare(m_cacheBuffer, value)) {
        return;
    }

    m_cacheBuffer = value;
    emit cacheBufferChanged();
    this->scheduleRework();
}

auto VirtualListView::reuseItems() const -> bool {
    return m_reuseItems;
}

auto VirtualListView::setReuseItems(bool value) -> void {
    if (m_reuseItems == value) {
        return;
    }

    m_reuseItems = value;
    emit reuseItemsChanged();

    if (!m_reuseItems) {
        this->destroyPooledItems();
    }
}

auto VirtualListView::followTail() const -> bool {
    return m_followTail;
}

auto VirtualListView::setFollowTail(bool value) -> void {
    if (m_followTail == value) {
        return;
    }

    m_followTail = value;
    emit followTailChanged();
    this->scheduleRework();
}

auto VirtualListView::estimatedItemHeight() const -> qreal {
    return m_estimatedItemHeight;
}

auto VirtualListView::setEstimatedItemHeight(qreal value) -> void {
    if (qFuzzyCompare(m_estimatedItemHeight, value)) {
        return;
    }

    m_estimatedItemHeight = sanitizeHeight(value);
    emit estimatedItemHeightChanged();
    m_estimatesDirty = true;
    this->scheduleRework();
}

auto VirtualListView::sizeHint() const -> QJSValue {
    return m_sizeHint;
}

auto VirtualListView::setSizeHint(QJSValue const& value) -> void {
    if (
        m_sizeHint.strictlyEquals(value)
        || (m_sizeHint.isCallable() && value.isCallable() && m_sizeHint.toString() == value.toString())
    ) {
        return;
    }

    m_sizeHint = value;
    emit sizeHintChanged();
    m_estimatesDirty = true;
    this->scheduleRework();
}

auto VirtualListView::maxItemsPerFrame() const -> int {
    return m_maxItemsPerFrame;
}

auto VirtualListView::setMaxItemsPerFrame(int value) -> void {
    int const clamped = std::max(1, value);
    if (m_maxItemsPerFrame == clamped) {
        return;
    }

    m_maxItemsPerFrame = clamped;
    emit maxItemsPerFrameChanged();
}

auto VirtualListView::count() const -> int {
    return m_layout.count();
}

auto VirtualListView::mountedCount() const -> int {
    return m_mounted.size();
}

auto VirtualListView::atEnd() const -> bool {
    qreal const total = m_layout.totalHeight();
    qreal const viewport = this->effectiveViewportHeight();

    if (viewport <= 0.0 || total <= viewport) {
        return true;
    }

    return m_contentY >= total - viewport - kCorrectionEpsilon;
}

auto VirtualListView::atBeginning() const -> bool {
    return m_contentY <= kCorrectionEpsilon;
}

auto VirtualListView::visibleRatio() const -> qreal {
    qreal const total = m_layout.totalHeight();
    if (total <= 0.0) {
        return 1.0;
    }

    return std::clamp(this->effectiveViewportHeight() / total, 0.0, 1.0);
}

auto VirtualListView::positionRatio() const -> qreal {
    qreal const total = m_layout.totalHeight();
    qreal const viewport = this->effectiveViewportHeight();
    qreal const range = std::max(0.0, total - viewport);

    if (range <= 0.0) {
        return 0.0;
    }

    return std::clamp(m_contentY / range, 0.0, 1.0);
}

auto VirtualListView::positionAtBeginning() -> void {
    emit scrollAdjustmentRequested(-m_contentY);
}

auto VirtualListView::positionAtEnd() -> void {
    qreal const total = m_layout.totalHeight();
    qreal const viewport = this->effectiveViewportHeight();
    qreal const target = std::max(0.0, total - viewport);

    emit scrollAdjustmentRequested(target - m_contentY);
}

auto VirtualListView::indexAt(qreal y) const -> int {
    return m_layout.indexAt(y);
}

auto VirtualListView::itemOffset(int index) const -> qreal {
    return m_layout.itemOffset(index);
}

auto VirtualListView::invalidateSize(int index) -> void {
    if (index < 0 || index >= m_layout.count()) {
        return;
    }

    if (m_mounted.contains(index)) {
        QQuickItem* const item = m_mounted[index].item;
        if (item != nullptr) {
            this->measureItem(index, item->height());
        }

        return;
    }

    m_layout.clearMeasurement(index);
    m_layout.setEstimate(index, this->estimateForIndex(index));
    this->publishContentHeight();
}

auto VirtualListView::invalidateAllSizes() -> void {
    for (auto it = m_mounted.begin(); it != m_mounted.end(); ++it) {
        QQuickItem* const item = it->item;
        if (item != nullptr) {
            this->measureItem(it.key(), item->height());
        }
    }

    m_layout.clearAllMeasurements();
    m_estimatesDirty = true;
    this->scheduleRework();
}

auto VirtualListView::requestRework() -> void {
    m_estimatesDirty = true;
    this->scheduleRework();
}

auto VirtualListView::scrollToBottom() -> void {
    this->positionAtEnd();
}

auto VirtualListView::itemForIndex(int index) const -> QObject* {
    auto const found = m_mounted.constFind(index);
    if (found == m_mounted.constEnd() || found->item == nullptr) {
        return nullptr;
    }

    return found->item;
}

auto VirtualListView::componentComplete() -> void {
    QQuickItem::componentComplete();
    m_componentComplete = true;
    this->scheduleRework();
}

auto VirtualListView::geometryChange(QRectF const& newGeometry, QRectF const& oldGeometry) -> void {
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (!m_componentComplete) {
        return;
    }

    bool const widthChanged = !qFuzzyCompare(newGeometry.width(), oldGeometry.width());
    bool const heightChanged = !qFuzzyCompare(newGeometry.height(), oldGeometry.height());

    if (widthChanged) {
        m_estimatesDirty = true;

        if (m_reworkActive) {
            m_layoutDirty = true;
            return;
        }

        this->rework();
        return;
    }

    if (heightChanged) {
        emit scrollInfoChanged();
        this->scheduleRework();
    }
}

auto VirtualListView::releaseAllMounted() -> void {
    QVector<int> indices;
    indices.reserve(m_mounted.size());
    for (auto it = m_mounted.cbegin(); it != m_mounted.cend(); ++it) {
        indices.append(it.key());
    }

    for (int const index : std::as_const(indices)) {
        this->releaseItem(index);
    }

    m_itemIndex.clear();

    if (!indices.isEmpty()) {
        emit mountedCountChanged();
    }
}

auto VirtualListView::itemChange(ItemChange change, ItemChangeData const& value) -> void {
    QQuickItem::itemChange(change, value);
}

auto VirtualListView::isReady() const -> bool {
    return m_componentComplete && m_model != nullptr && m_delegate != nullptr && m_delegate->isReady();
}

auto VirtualListView::delegateWidth() const -> qreal {
    if (m_contentWidth >= 0.0) {
        return m_contentWidth;
    }

    return this->width();
}

auto VirtualListView::effectiveViewportHeight() const -> qreal {
    if (m_viewportHeight >= 0.0) {
        return m_viewportHeight;
    }

    return this->height();
}

auto VirtualListView::connectModel() -> void {
    if (m_model == nullptr) {
        return;
    }

    QAbstractItemModel* const model = m_model;

    m_modelConnections.append(QObject::connect(
        model,
        &QAbstractItemModel::rowsInserted,
        this,
        [this](QModelIndex const& parent, int first, int last) {
            this->handleRowsInserted(parent, first, last);
        }
    ));

    m_modelConnections.append(QObject::connect(
        model,
        &QAbstractItemModel::rowsRemoved,
        this,
        [this](QModelIndex const& parent, int first, int last) {
            this->handleRowsRemoved(parent, first, last);
        }
    ));

    m_modelConnections.append(QObject::connect(
        model,
        &QAbstractItemModel::rowsMoved,
        this,
        [this](
            QModelIndex const& sourceParent,
            int sourceStart,
            int sourceEnd,
            QModelIndex const& destinationParent,
            int destinationRow
        ) {
            this->handleRowsMoved(sourceParent, sourceStart, sourceEnd, destinationParent, destinationRow);
        }
    ));

    m_modelConnections.append(QObject::connect(
        model,
        &QAbstractItemModel::modelReset,
        this,
        [this] { this->handleModelReset(); }
    ));

    m_modelConnections.append(QObject::connect(
        model,
        &QAbstractItemModel::layoutChanged,
        this,
        [this] { this->handleModelReset(); }
    ));

    m_modelConnections.append(QObject::connect(
        model,
        &QAbstractItemModel::dataChanged,
        this,
        [this](QModelIndex const& topLeft, QModelIndex const& bottomRight, QVector<int> const& roles) {
            this->handleDataChanged(topLeft, bottomRight, roles);
        }
    ));
}

auto VirtualListView::disconnectModel() -> void {
    for (QMetaObject::Connection const& connection : std::as_const(m_modelConnections)) {
        QObject::disconnect(connection);
    }
    m_modelConnections.clear();
}

auto VirtualListView::handleRowsInserted(QModelIndex const& parent, int first, int last) -> void {
    if (parent.isValid()) {
        return;
    }

    int const added = last - first + 1;

    qreal const viewport = this->effectiveViewportHeight();
    bool const follow = m_followTail;
    bool const wasAtEnd = m_contentY
        >= std::max(0.0, m_layout.totalHeight() - viewport) - kCorrectionEpsilon;

    VirtualListLayout::Anchor const anchor = m_layout.resolveAnchor(m_contentY);
    m_layout.insertItems(first, added);

    for (int i = 0; i < added; ++i) {
        m_layout.setEstimate(first + i, this->estimateForIndex(first + i));
    }

    QHash<int, MountedItem> shifted;
    shifted.reserve(m_mounted.size());
    for (auto it = m_mounted.begin(); it != m_mounted.end(); ++it) {
        int index = it.key();
        if (index >= first) {
            index += added;
        }
        if (index != it.key() && it->item != nullptr) {
            this->bindItemIndex(it->item, index);
        }
        shifted.insert(index, it.value());
    }
    m_mounted = shifted;

    m_itemIndex.clear();
    for (auto it = m_mounted.begin(); it != m_mounted.end(); ++it) {
        if (it->item != nullptr) {
            m_itemIndex.insert(it->item, it.key());
        }
    }

    m_estimatesDirty = true;
    this->publishContentHeight();

    if (follow && wasAtEnd) {
        qreal const target = std::max(0.0, m_layout.totalHeight() - viewport);
        if (std::abs(target - m_contentY) > kCorrectionEpsilon) {
            this->queueScrollAdjustment(target - m_contentY);
        }

        this->scheduleRework();
        return;
    }

    VirtualListLayout::Anchor const rebased = m_layout.anchorAfterInsert(anchor, first, added);
    qreal const desired = m_layout.contentYForAnchor(rebased);
    if (std::abs(desired - m_contentY) > kCorrectionEpsilon) {
        this->queueScrollAdjustment(desired - m_contentY);
    }

    m_estimatesDirty = true;
    this->publishContentHeight();
    this->scheduleRework();
}

auto VirtualListView::handleRowsRemoved(QModelIndex const& parent, int first, int last) -> void {
    if (parent.isValid()) {
        return;
    }

    int const removed = last - first + 1;

    qreal const viewport = this->effectiveViewportHeight();
    bool const follow = m_followTail;
    bool const wasAtEnd = m_contentY
        >= std::max(0.0, m_layout.totalHeight() - viewport) - kCorrectionEpsilon;

    VirtualListLayout::Anchor const anchor = m_layout.resolveAnchor(m_contentY);
    m_layout.removeItems(first, removed);

    for (int index = first; index <= last; ++index) {
        if (m_mounted.contains(index)) {
            this->releaseItem(index);
            m_mounted.remove(index);
        }
    }

    QHash<int, MountedItem> shifted;
    shifted.reserve(m_mounted.size());
    for (auto it = m_mounted.begin(); it != m_mounted.end(); ++it) {
        int index = it.key();
        if (index > last) {
            index -= removed;
        }
        if (index != it.key() && it->item != nullptr) {
            this->bindItemIndex(it->item, index);
        }
        shifted.insert(index, it.value());
    }
    m_mounted = shifted;

    m_itemIndex.clear();
    for (auto it = m_mounted.begin(); it != m_mounted.end(); ++it) {
        if (it->item != nullptr) {
            m_itemIndex.insert(it->item, it.key());
        }
    }

    m_estimatesDirty = true;
    this->publishContentHeight();

    if (follow && wasAtEnd) {
        qreal const target = std::max(0.0, m_layout.totalHeight() - viewport);
        if (std::abs(target - m_contentY) > kCorrectionEpsilon) {
            this->queueScrollAdjustment(target - m_contentY);
        }

        this->scheduleRework();
        return;
    }

    VirtualListLayout::Anchor const rebased = m_layout.anchorAfterRemove(anchor, first, removed);
    qreal const desired = m_layout.contentYForAnchor(rebased);
    if (std::abs(desired - m_contentY) > kCorrectionEpsilon) {
        this->queueScrollAdjustment(desired - m_contentY);
    }

    m_estimatesDirty = true;
    this->publishContentHeight();
    this->scheduleRework();
}

auto VirtualListView::handleRowsMoved(
    QModelIndex const& sourceParent,
    int,
    int,
    QModelIndex const& destinationParent,
    int
) -> void {
    if (sourceParent.isValid() || destinationParent.isValid()) {
        return;
    }

    this->releaseAllMounted();

    m_estimatesDirty = true;
    this->resetLayout();
    this->scheduleRework();
}

auto VirtualListView::handleModelReset() -> void {
    this->releaseAllMounted();

    this->resetLayout();
    this->scheduleRework();
}

auto VirtualListView::handleDataChanged(
    QModelIndex const& topLeft,
    QModelIndex const& bottomRight,
    QVector<int> const&
) -> void {
    if (!topLeft.isValid() || topLeft.parent().isValid()) {
        return;
    }

    int const last = std::min(bottomRight.row(), m_layout.count() - 1);

    qreal const viewport = this->effectiveViewportHeight();
    bool const follow = m_followTail;
    bool const wasAtEnd = m_contentY
        >= std::max(0.0, m_layout.totalHeight() - viewport) - kCorrectionEpsilon;
    VirtualListLayout::Anchor const anchor = m_layout.resolveAnchor(m_contentY);

    bool heightsChanged = false;

    for (int index = topLeft.row(); index <= last; ++index) {
        if (index < 0 || index >= m_layout.count()) {
            continue;
        }

        if (m_mounted.contains(index)) {
            QQuickItem* const item = m_mounted[index].item;
            if (item == nullptr) {
                continue;
            }

            ++m_debugDataBinds;
            this->bindItem(index, item);
            continue;
        }

        m_layout.clearMeasurement(index);
        m_layout.setEstimate(index, this->estimateForIndex(index));
        heightsChanged = true;
    }

    if (!heightsChanged) {
        return;
    }

    this->publishContentHeight();

    qreal const total = m_layout.totalHeight();
    qreal desiredY = m_layout.contentYForAnchor(anchor);
    if (follow && wasAtEnd) {
        desiredY = std::max(0.0, total - viewport);
    }

    qreal const delta = desiredY - m_contentY;
    if (std::abs(delta) > kCorrectionEpsilon) {
        this->queueScrollAdjustment(delta);
    }

    this->scheduleRework();
}

auto VirtualListView::resetLayout() -> void {
    int const rowCount = m_model != nullptr ? m_model->rowCount() : 0;
    m_layout.reset(rowCount, sanitizeHeight(m_estimatedItemHeight));
    m_estimatesDirty = true;
    this->publishContentHeight();
}

auto VirtualListView::queueScrollAdjustment(qreal delta) -> void {
    m_pendingAdjustment += delta;
    m_adjustTimer->start();
}

auto VirtualListView::scheduleRework() -> void {
    if (m_reworkTimer != nullptr) {
        m_reworkTimer->start();
    }
}

// Triggered from timer context only - never from inside a QML binding
// evaluation, so scrollAdjustmentRequested can safely write contentY.

auto VirtualListView::debugCounters() const -> QString {
    return QString("reworks=%1 creates=%2 poolBinds=%3 releases=%4 estCalls=%5 dataBinds=%6")
        .arg(m_debugReworks).arg(m_debugCreates).arg(m_debugPoolBinds)
        .arg(m_debugReleases).arg(m_debugEstimateCalls).arg(m_debugDataBinds);
}

auto VirtualListView::rework() -> void {
    if (m_reworkActive) {
        m_layoutDirty = true;
        return;
    }

    ++m_debugReworks;
    m_reworkActive = true;

    do {
        m_layoutDirty = false;
        this->reworkOnce();
    } while (m_layoutDirty);

    m_reworkActive = false;
}

auto VirtualListView::reworkOnce() -> void {
    m_createdThisFrame = 0;

    qreal const viewport = this->effectiveViewportHeight();
    bool const follow = m_followTail;
    bool const wasAtEnd = m_contentY
        >= std::max(0.0, m_layout.totalHeight() - viewport) - kCorrectionEpsilon;
    VirtualListLayout::Anchor const anchor = m_layout.resolveAnchor(m_contentY);

    if (m_estimatesDirty) {
        this->rebuildEstimates();
        m_estimatesDirty = false;
    }

    this->updateMounted(wasAtEnd, follow, anchor);
    this->positionMounted();
    this->publishContentHeight();
    this->polish();
}

auto VirtualListView::rebuildEstimates() -> void {
    int const rowCount = m_model != nullptr ? m_model->rowCount() : 0;
    if (rowCount != m_layout.count()) {
        m_layout.reset(rowCount, sanitizeHeight(m_estimatedItemHeight));
    }

    for (int index = 0; index < rowCount; ++index) {
        if (m_layout.hasMeasurement(index)) {
            continue;
        }

        m_layout.setEstimate(index, this->estimateForIndex(index));
    }
}

auto VirtualListView::estimateForIndex(int index) -> qreal {
    if (!m_sizeHint.isCallable() || m_model == nullptr) {
        return sanitizeHeight(m_estimatedItemHeight);
    }

    QQmlEngine* const engine = qmlEngine(this);
    ++m_debugEstimateCalls;

    if (engine == nullptr) {
        return sanitizeHeight(m_estimatedItemHeight);
    }

    QJSValueList arguments;
    arguments.reserve(3);
    arguments.append(engine->toScriptValue(static_cast<double>(index)));
    arguments.append(engine->toScriptValue(this->delegateWidth()));
    arguments.append(engine->toScriptValue(QVariant::fromValue<QObject*>(m_model.data())));

    QJSValue const result = m_sizeHint.call(arguments);
    double const height = result.isNumber() ? result.toNumber() : static_cast<double>(m_estimatedItemHeight);

    return sanitizeHeight(static_cast<qreal>(height));
}

auto VirtualListView::updateMounted(bool wasAtEnd, bool follow, VirtualListLayout::Anchor const& anchor) -> void {
    if (!this->isReady()) {
        this->releaseAllMounted();
        return;
    }

    VirtualListLayout::Range const desired = this->mountRange();
    bool incomplete = false;
    int const mountedBeforeFill = m_mounted.size();

    QVector<int> doomed;
    for (auto it = m_mounted.cbegin(); it != m_mounted.cend(); ++it) {
        int const index = it.key();
        if (index < desired.first || index > desired.last) {
            doomed.append(index);
        }
    }
    for (int const index : std::as_const(doomed)) {
        this->releaseItem(index);
    }

    if (!desired.isEmpty()) {
        m_updating = true;

        qreal const viewport = this->effectiveViewportHeight();

        for (int index = desired.first; index <= desired.last; ++index) {
            if (m_mounted.contains(index)) {
                continue;
            }

            if (m_pool.isEmpty() && m_createdThisFrame >= m_maxItemsPerFrame) {
                incomplete = true;
                break;
            }

            QQuickItem* const item = this->acquireItem(index);
            if (item == nullptr) {
                break;
            }

            this->bindItem(index, item);
        }

        m_updating = false;

        this->publishContentHeight();

        if (m_mounted.size() != mountedBeforeFill) {
            emit mountedCountChanged();
        }

        qreal const total = m_layout.totalHeight();
        qreal desiredY = m_layout.contentYForAnchor(anchor);
        if (follow && wasAtEnd) {
            desiredY = std::max(0.0, total - viewport);
        }

        qreal const delta = desiredY - m_contentY;
        if (std::abs(delta) > kCorrectionEpsilon) {
            this->queueScrollAdjustment(delta);
        }
    }

    if (incomplete) {
        this->scheduleRework();
    }
}

auto VirtualListView::updatePolish() -> void {
    if (m_mounted.isEmpty()) {
        return;
    }

    qreal const viewport = this->effectiveViewportHeight();
    bool const follow = m_followTail;
    bool const wasAtEnd = m_contentY
        >= std::max(0.0, m_layout.totalHeight() - viewport) - kCorrectionEpsilon;
    VirtualListLayout::Anchor const anchor = m_layout.resolveAnchor(m_contentY);

    m_updating = true;

    bool changed = false;
    for (auto it = m_mounted.begin(); it != m_mounted.end(); ++it) {
        QQuickItem* const item = it->item;
        if (item == nullptr) {
            continue;
        }

        qreal const height = item->height();
        if (height > 0.0 && !qFuzzyCompare(height, m_layout.heightAt(it.key()))) {
            m_layout.setMeasuredHeight(it.key(), height);
            it->trackedHeight = height;
            changed = true;
        }
    }

    m_updating = false;

    if (!changed) {
        return;
    }

    this->publishContentHeight();
    this->positionMounted();

    qreal const total = m_layout.totalHeight();
    qreal desiredY = m_layout.contentYForAnchor(anchor);
    if (follow && wasAtEnd) {
        desiredY = std::max(0.0, total - viewport);
    }

    qreal const delta = desiredY - m_contentY;
    if (std::abs(delta) > kCorrectionEpsilon) {
        this->queueScrollAdjustment(delta);
    }
}

auto VirtualListView::mountRange() const -> VirtualListLayout::Range {
    return m_layout.mountRange(m_contentY, this->effectiveViewportHeight(), m_cacheBuffer);
}

auto VirtualListView::acquireItem(int index) -> QQuickItem* {
    QQuickItem* item = nullptr;

    if (m_reuseItems && !m_pool.isEmpty()) {
        item = m_pool.takeFirst();
        if (item != nullptr) {
            ++m_debugPoolBinds;
            m_layout.clearMeasurement(index);
            item->setParentItem(this);
        }
    }

    if (item == nullptr) {
        QQmlContext* const context = QQmlEngine::contextForObject(this);
        QObject* object = nullptr;

        if (m_delegate->isReady()) {
            object = m_delegate->create(context);
        }

        if (object == nullptr) {
            return nullptr;
        }

        item = qobject_cast<QQuickItem*>(object);
        if (item == nullptr) {
            delete object;
            return nullptr;
        }

        item->setParent(this);
        item->setParentItem(this);
        ++m_debugCreates;
    }

    m_mounted.insert(index, MountedItem{});
    m_itemIndex.insert(item, index);

    return item;
}

auto VirtualListView::releaseItem(int index) -> void {
    auto const found = m_mounted.find(index);
    if (found == m_mounted.end()) {
        return;
    }

    QQuickItem* const item = found->item;

    if (found->heightConnection) {
        QObject::disconnect(found->heightConnection);
    }
    if (found->implicitHeightConnection) {
        QObject::disconnect(found->implicitHeightConnection);
    }
    if (found->widthConnection) {
        QObject::disconnect(found->widthConnection);
    }

    if (item != nullptr) {
        m_itemIndex.remove(item);

        if (m_reuseItems) {
            ++m_debugReleases;
            this->clearItemRoles(item, found->boundRoles);
            item->setParentItem(nullptr);
            m_pool.append(item);
            emit itemRecycled(index, item);
        } else {
            item->deleteLater();
        }
    }

    if (found != m_mounted.end()) {
        m_mounted.erase(found);
    }
}

auto VirtualListView::bindItemIndex(QQuickItem* item, int index) -> void {
    if (item == nullptr) {
        return;
    }

    if (item->metaObject()->indexOfProperty("index") >= 0) {
        item->setProperty("index", index);
    }
}

auto VirtualListView::bindItem(int index, QQuickItem* item) -> void {
    auto const found = m_mounted.find(index);
    if (found == m_mounted.end()) {
        m_mounted.insert(index, MountedItem{});
    }

    auto const entry = m_mounted.find(index);
    if (entry->item != item) {
        if (entry->heightConnection) {
            QObject::disconnect(entry->heightConnection);
        }
        if (entry->implicitHeightConnection) {
            QObject::disconnect(entry->implicitHeightConnection);
        }
        if (entry->widthConnection) {
            QObject::disconnect(entry->widthConnection);
        }
        entry->item = item;
        entry->trackedHeight = -1.0;
        m_itemIndex.insert(item, index);
    }

    this->bindItemIndex(item, index);

    entry->boundRoles.clear();

    if (m_model != nullptr) {
        QHash<int, QByteArray> const roleNames = m_model->roleNames();
        QModelIndex const modelIndex = m_model->index(index, 0);

        for (auto it = roleNames.cbegin(); it != roleNames.cend(); ++it) {
            QByteArray const name = it.value();
            if (item->metaObject()->indexOfProperty(name.constData()) < 0) {
                continue;
            }

            QVariant const value = m_model->data(modelIndex, it.key());
            item->setProperty(name.constData(), value);
            entry->boundRoles.append(name);
        }
    }

    item->setWidth(this->delegateWidth());
    item->setX(0.0);

    if (!entry->heightConnection) {
        entry->heightConnection = QObject::connect(
            item,
            &QQuickItem::heightChanged,
            this,
            [this, item] { this->handleItemHeightChanged(item); }
        );
    }

    if (!entry->implicitHeightConnection) {
        entry->implicitHeightConnection = QObject::connect(
            item,
            &QQuickItem::implicitHeightChanged,
            this,
            [this, item] { this->handleItemHeightChanged(item); }
        );
    }

    emit itemActivated(index, item);
}

auto VirtualListView::clearItemRoles(QQuickItem* item, QVector<QByteArray> const& roles) -> void {
    for (QByteArray const& role : roles) {
        if (item->metaObject()->indexOfProperty(role.constData()) >= 0) {
            item->setProperty(role.constData(), QVariant());
        }
    }
}

auto VirtualListView::handleItemHeightChanged(QQuickItem* item) -> void {
    if (m_updating) {
        return;
    }

    int const index = m_itemIndex.value(item, -1);
    if (index < 0) {
        return;
    }

    this->measureItem(index, item->height());
}

auto VirtualListView::measureItem(int index, qreal height) -> void {
    if (m_updating || height <= 0.0) {
        return;
    }

    auto const found = m_mounted.find(index);
    if (found == m_mounted.end() || found->item == nullptr) {
        return;
    }

    if (qFuzzyCompare(height, m_layout.heightAt(index))) {
        found->trackedHeight = height;
        return;
    }

    VirtualListLayout::Anchor const anchor = m_layout.resolveAnchor(m_contentY);
    qreal const viewport = this->effectiveViewportHeight();
    bool const follow = m_followTail;
    qreal const previousMax = std::max(0.0, m_layout.totalHeight() - viewport);
    bool const wasAtEnd = m_contentY >= previousMax - kCorrectionEpsilon;

    m_layout.setMeasuredHeight(index, height);
    found->trackedHeight = height;

    this->publishContentHeight();

    qreal const total = m_layout.totalHeight();
    qreal desiredY = m_layout.contentYForAnchor(anchor);
    if (follow && wasAtEnd) {
        desiredY = std::max(0.0, total - viewport);
    }

    qreal const delta = desiredY - m_contentY;
    if (std::abs(delta) > kCorrectionEpsilon) {
        this->queueScrollAdjustment(delta);
    }

    this->positionMounted();
    this->scheduleRework();
}

auto VirtualListView::positionMounted() -> void {
    for (auto it = m_mounted.begin(); it != m_mounted.end(); ++it) {
        QQuickItem* const item = it->item;
        if (item == nullptr) {
            continue;
        }

        item->setX(0.0);
        item->setY(m_layout.itemOffset(it.key()));
        item->setWidth(this->delegateWidth());
    }
}

auto VirtualListView::publishContentHeight() -> void {
    qreal const height = m_layout.totalHeight();
    int const rowCount = m_layout.count();

    bool changed = false;

    if (!qFuzzyCompare(height, m_publishedHeight)) {
        m_publishedHeight = height;
        emit contentHeightChanged();
        changed = true;
    }

    if (rowCount != m_publishedCount) {
        m_publishedCount = rowCount;
        emit countChanged();
        changed = true;
    }

    if (changed) {
        emit scrollInfoChanged();
    }
}

auto VirtualListView::destroyPooledItems() -> void {
    for (QQuickItem* const item : std::as_const(m_pool)) {
        if (item != nullptr) {
            item->deleteLater();
        }
    }
    m_pool.clear();
}