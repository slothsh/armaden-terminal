#include "virtual_list_layout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kMinimumHeight = 1.0;
constexpr double kPositionEpsilon = 1e-6;

auto sanitizeHeight(double height) -> double {
    if (!std::isfinite(height) || height < kMinimumHeight) {
        return kMinimumHeight;
    }

    return height;
}

auto highestPowerOfTwo(int value) -> int {
    int power = 1;
    while (power <= value / 2) {
        power <<= 1;
    }

    return power;
}

} // END OF NAMESPACE anonymous

auto VirtualListLayout::count() const -> int {
    return static_cast<int>(m_heights.size());
}

auto VirtualListLayout::isEmpty() const -> bool {
    return m_heights.empty();
}

auto VirtualListLayout::defaultEstimate() const -> double {
    return m_defaultEstimate;
}

auto VirtualListLayout::setDefaultEstimate(double estimate) -> void {
    m_defaultEstimate = sanitizeHeight(estimate);

    for (std::size_t i = 0; i < m_estimates.size(); ++i) {
        if (!m_measured[i] && m_estimates[i] != m_defaultEstimate) {
            m_estimates[i] = m_defaultEstimate;
            m_heights[i] = m_defaultEstimate;
        }
    }

    this->rebuild();
}

auto VirtualListLayout::reset(int itemCount, double estimate) -> void {
    m_defaultEstimate = sanitizeHeight(estimate);
    int const clamped = std::max(0, itemCount);

    m_heights.assign(static_cast<std::size_t>(clamped), m_defaultEstimate);
    m_estimates.assign(static_cast<std::size_t>(clamped), m_defaultEstimate);
    m_measured.assign(static_cast<std::size_t>(clamped), 0);

    this->rebuild();
}

auto VirtualListLayout::resize(int itemCount) -> void {
    int const clamped = std::max(0, itemCount);
    if (clamped == this->count()) {
        return;
    }

    if (clamped > this->count()) {
        int const added = clamped - this->count();
        for (int i = 0; i < added; ++i) {
            this->appendHeight(m_defaultEstimate);
        }
    } else {
        m_heights.resize(static_cast<std::size_t>(clamped));
        m_estimates.resize(static_cast<std::size_t>(clamped));
        m_measured.resize(static_cast<std::size_t>(clamped));
        this->rebuild();
    }
}

auto VirtualListLayout::insertItems(int index, int itemCount) -> void {
    int const clamped = std::max(0, itemCount);
    if (clamped == 0) {
        return;
    }

    int const position = std::clamp(index, 0, this->count());
    if (position == this->count()) {
        for (int i = 0; i < clamped; ++i) {
            this->appendHeight(m_defaultEstimate);
        }

        return;
    }

    auto const offset = static_cast<std::ptrdiff_t>(position);
    m_heights.insert(m_heights.begin() + offset, static_cast<std::size_t>(clamped), m_defaultEstimate);
    m_estimates.insert(m_estimates.begin() + offset, static_cast<std::size_t>(clamped), m_defaultEstimate);
    m_measured.insert(m_measured.begin() + offset, static_cast<std::size_t>(clamped), 0);

    this->rebuild();
}

auto VirtualListLayout::removeItems(int index, int itemCount) -> void {
    if (this->isEmpty()) {
        return;
    }

    int const position = std::clamp(index, 0, this->count());
    int const available = this->count() - position;
    int const removed = std::min(std::max(0, itemCount), available);
    if (removed == 0) {
        return;
    }

    auto const first = static_cast<std::ptrdiff_t>(position);
    auto const last = first + removed;
    m_heights.erase(m_heights.begin() + first, m_heights.begin() + last);
    m_estimates.erase(m_estimates.begin() + first, m_estimates.begin() + last);
    m_measured.erase(m_measured.begin() + first, m_measured.begin() + last);

    this->rebuild();
}

auto VirtualListLayout::moveItem(int from, int to) -> void {
    if (from == to || from < 0 || from >= this->count()) {
        return;
    }

    int const destination = std::clamp(to, 0, this->count() - 1);
    if (destination == from) {
        return;
    }

    double const height = m_heights[static_cast<std::size_t>(from)];
    double const estimate = m_estimates[static_cast<std::size_t>(from)];
    char const measured = m_measured[static_cast<std::size_t>(from)];

    auto const offset = static_cast<std::ptrdiff_t>(from);
    m_heights.erase(m_heights.begin() + offset);
    m_estimates.erase(m_estimates.begin() + offset);
    m_measured.erase(m_measured.begin() + offset);

    auto const target = static_cast<std::ptrdiff_t>(destination);
    m_heights.insert(m_heights.begin() + target, height);
    m_estimates.insert(m_estimates.begin() + target, estimate);
    m_measured.insert(m_measured.begin() + target, measured);

    this->rebuild();
}

auto VirtualListLayout::hasMeasurement(int index) const -> bool {
    if (index < 0 || index >= this->count()) {
        return false;
    }

    return m_measured[static_cast<std::size_t>(index)] != 0;
}

auto VirtualListLayout::heightAt(int index) const -> double {
    if (index < 0 || index >= this->count()) {
        return m_defaultEstimate;
    }

    return m_heights[static_cast<std::size_t>(index)];
}

auto VirtualListLayout::estimateAt(int index) const -> double {
    if (index < 0 || index >= this->count()) {
        return m_defaultEstimate;
    }

    return m_estimates[static_cast<std::size_t>(index)];
}

auto VirtualListLayout::setEstimate(int index, double height) -> void {
    if (index < 0 || index >= this->count()) {
        return;
    }

    auto const position = static_cast<std::size_t>(index);
    double const sanitized = sanitizeHeight(height);
    m_estimates[position] = sanitized;

    if (m_measured[position] == 0) {
        this->treeAdd(index, sanitized - m_heights[position]);
        m_heights[position] = sanitized;
    }
}

auto VirtualListLayout::setMeasuredHeight(int index, double height) -> void {
    if (index < 0 || index >= this->count()) {
        return;
    }

    auto const position = static_cast<std::size_t>(index);
    double const sanitized = sanitizeHeight(height);

    m_measured[position] = 1;
    this->treeAdd(index, sanitized - m_heights[position]);
    m_heights[position] = sanitized;
}

auto VirtualListLayout::clearMeasurement(int index) -> void {
    if (index < 0 || index >= this->count()) {
        return;
    }

    auto const position = static_cast<std::size_t>(index);
    if (m_measured[position] == 0) {
        return;
    }

    m_measured[position] = 0;
    this->treeAdd(index, m_estimates[position] - m_heights[position]);
    m_heights[position] = m_estimates[position];
}

auto VirtualListLayout::clearAllMeasurements() -> void {
    for (int i = 0; i < this->count(); ++i) {
        this->clearMeasurement(i);
    }
}

auto VirtualListLayout::totalHeight() const -> double {
    return this->prefixSum(this->count());
}

auto VirtualListLayout::itemOffset(int index) const -> double {
    if (index <= 0) {
        return 0.0;
    }

    return this->prefixSum(std::min(index, this->count()));
}

auto VirtualListLayout::indexAt(double y) const -> int {
    int const itemCount = this->count();
    if (itemCount == 0) {
        return -1;
    }

    if (y <= 0.0) {
        return 0;
    }

    if (y >= this->totalHeight()) {
        return itemCount - 1;
    }

    int position = 0;
    double remaining = y;

    for (int step = highestPowerOfTwo(itemCount); step > 0; step >>= 1) {
        int const next = position + step;
        if (next <= itemCount && m_tree[static_cast<std::size_t>(next)] <= remaining) {
            position = next;
            remaining -= m_tree[static_cast<std::size_t>(next)];
        }
    }

    return std::min(position, itemCount - 1);
}

auto VirtualListLayout::visibleRange(double contentY, double viewportHeight) const -> Range {
    if (this->isEmpty() || viewportHeight <= 0.0) {
        return {};
    }

    double const top = std::max(0.0, contentY);
    int const first = this->indexAt(top);
    int const last = this->indexAt(top + viewportHeight - kPositionEpsilon);

    return Range{first, std::max(first, last)};
}

auto VirtualListLayout::mountRange(double contentY, double viewportHeight, double cacheBuffer) const -> Range {
    Range range = this->visibleRange(contentY, viewportHeight);
    if (range.isEmpty()) {
        return range;
    }

    double const buffer = std::max(0.0, cacheBuffer);
    double const top = std::max(0.0, contentY);
    double const bottom = top + viewportHeight + buffer;

    while (range.first > 0 && this->itemOffset(range.first) > top - buffer) {
        --range.first;
    }

    while (range.last + 1 < this->count() && this->itemOffset(range.last + 1) < bottom) {
        ++range.last;
    }

    return range;
}

auto VirtualListLayout::resolveAnchor(double contentY) const -> Anchor {
    if (this->isEmpty()) {
        return {};
    }

    int const index = this->indexAt(std::max(0.0, contentY));

    return Anchor{index, std::max(0.0, contentY) - this->itemOffset(index)};
}

auto VirtualListLayout::contentYForAnchor(Anchor const& anchor) const -> double {
    if (this->isEmpty()) {
        return 0.0;
    }

    int const index = std::clamp(anchor.index, 0, this->count() - 1);

    return this->itemOffset(index) + anchor.offset;
}

auto VirtualListLayout::anchorAfterInsert(Anchor const& anchor, int index, int itemCount) const -> Anchor {
    Anchor adjusted = anchor;
    if (anchor.index >= index) {
        adjusted.index = anchor.index + std::max(0, itemCount);
    }

    return adjusted;
}

auto VirtualListLayout::anchorAfterRemove(Anchor const& anchor, int index, int itemCount) const -> Anchor {
    int const removed = std::max(0, itemCount);

    Anchor adjusted = anchor;
    if (anchor.index >= index + removed) {
        adjusted.index = anchor.index - removed;
    } else if (anchor.index >= index) {
        adjusted.index = index;
        adjusted.offset = 0.0;
    }

    return adjusted;
}

auto VirtualListLayout::anchorAfterMove(Anchor const& anchor, int from, int to) const -> Anchor {
    if (anchor.index == from) {
        return Anchor{to, anchor.offset};
    }

    Anchor adjusted = anchor;
    if (from < anchor.index && to >= anchor.index) {
        adjusted.index = anchor.index - 1;
    } else if (from > anchor.index && to <= anchor.index) {
        adjusted.index = anchor.index + 1;
    }

    return adjusted;
}

auto VirtualListLayout::effectiveHeight(int index) const -> double {
    if (m_measured[static_cast<std::size_t>(index)] != 0) {
        return m_heights[static_cast<std::size_t>(index)];
    }

    return m_estimates[static_cast<std::size_t>(index)];
}

auto VirtualListLayout::appendHeight(double height) -> void {
    double const sanitized = sanitizeHeight(height);

    m_heights.push_back(sanitized);
    m_estimates.push_back(sanitized);
    m_measured.push_back(0);
    m_tree.push_back(0.0);

    int const node = this->count();
    int const lowbit = node & -node;
    int const coveredStart = node - lowbit;
    double const existing = this->prefixSum(node - 1)
        - (coveredStart > 0 ? this->prefixSum(coveredStart) : 0.0);
    m_tree[static_cast<std::size_t>(node)] = existing + sanitized;
}

auto VirtualListLayout::rebuild() -> void {
    m_tree.assign(m_heights.size() + 1, 0.0);

    for (int i = 0; i < this->count(); ++i) {
        m_heights[static_cast<std::size_t>(i)] = this->effectiveHeight(i);
        this->treeAdd(i, m_heights[static_cast<std::size_t>(i)]);
    }
}

auto VirtualListLayout::treeAdd(int index, double delta) -> void {
    if (delta == 0.0) {
        return;
    }

    for (int i = index + 1; i <= this->count(); i += i & -i) {
        m_tree[static_cast<std::size_t>(i)] += delta;
    }
}

auto VirtualListLayout::prefixSum(int end) const -> double {
    double total = 0.0;

    for (int i = std::min(end, this->count()); i > 0; i -= i & -i) {
        total += m_tree[static_cast<std::size_t>(i)];
    }

    return total;
}
