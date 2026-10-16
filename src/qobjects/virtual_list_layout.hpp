#pragma once

#include <cstddef>
#include <vector>

class VirtualListLayout {
public:
    struct Range {
        int first = 0;
        int last = -1;

        [[nodiscard]] auto isEmpty() const -> bool {
            return last < first;
        }
    };

    struct Anchor {
        int index = 0;
        double offset = 0.0;
    };

    VirtualListLayout() = default;

    [[nodiscard]] auto count() const -> int;
    [[nodiscard]] auto isEmpty() const -> bool;

    [[nodiscard]] auto defaultEstimate() const -> double;
    auto setDefaultEstimate(double estimate) -> void;

    auto reset(int count, double estimate) -> void;
    auto resize(int count) -> void;

    auto insertItems(int index, int count) -> void;
    auto removeItems(int index, int count) -> void;
    auto moveItem(int from, int to) -> void;

    [[nodiscard]] auto hasMeasurement(int index) const -> bool;
    [[nodiscard]] auto heightAt(int index) const -> double;
    [[nodiscard]] auto estimateAt(int index) const -> double;

    auto setEstimate(int index, double height) -> void;
    auto setMeasuredHeight(int index, double height) -> void;
    auto clearMeasurement(int index) -> void;
    auto clearAllMeasurements() -> void;

    [[nodiscard]] auto totalHeight() const -> double;
    [[nodiscard]] auto itemOffset(int index) const -> double;
    [[nodiscard]] auto indexAt(double y) const -> int;

    [[nodiscard]] auto visibleRange(double contentY, double viewportHeight) const -> Range;
    [[nodiscard]] auto mountRange(double contentY, double viewportHeight, double cacheBuffer) const -> Range;

    [[nodiscard]] auto resolveAnchor(double contentY) const -> Anchor;
    [[nodiscard]] auto contentYForAnchor(Anchor const& anchor) const -> double;
    [[nodiscard]] auto anchorAfterInsert(Anchor const& anchor, int index, int count) const -> Anchor;
    [[nodiscard]] auto anchorAfterRemove(Anchor const& anchor, int index, int count) const -> Anchor;
    [[nodiscard]] auto anchorAfterMove(Anchor const& anchor, int from, int to) const -> Anchor;

private:
    [[nodiscard]] auto effectiveHeight(int index) const -> double;

    auto appendHeight(double height) -> void;
    auto rebuild() -> void;
    auto treeAdd(int index, double delta) -> void;
    [[nodiscard]] auto prefixSum(int end) const -> double;

    std::vector<double> m_heights;
    std::vector<double> m_estimates;
    std::vector<char> m_measured;
    std::vector<double> m_tree;
    double m_defaultEstimate = 48.0;
};
