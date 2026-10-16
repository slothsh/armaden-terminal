#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "virtual_list_layout.hpp"

namespace {

constexpr double kTolerance = 1e-6;

auto approx(double value, double expected) -> bool {
    return std::abs(value - expected) < kTolerance;
}

auto measuredLayout() -> VirtualListLayout {
    VirtualListLayout layout;
    layout.reset(4, 10.0);
    layout.setMeasuredHeight(0, 30.0);
    layout.setMeasuredHeight(1, 20.0);
    layout.setMeasuredHeight(2, 50.0);
    layout.setMeasuredHeight(3, 40.0);

    return layout;
}

} // END OF NAMESPACE anonymous

TEST_CASE("virtual list layout tracks heterogeneous offsets", "[virtualization][layout]") {
    VirtualListLayout layout = measuredLayout();

    REQUIRE(layout.count() == 4);
    REQUIRE(approx(layout.totalHeight(), 140.0));
    REQUIRE(approx(layout.itemOffset(0), 0.0));
    REQUIRE(approx(layout.itemOffset(1), 30.0));
    REQUIRE(approx(layout.itemOffset(2), 50.0));
    REQUIRE(approx(layout.itemOffset(3), 100.0));
    REQUIRE(approx(layout.itemOffset(4), 140.0));
}

TEST_CASE("virtual list layout resolves index at arbitrary positions", "[virtualization][layout]") {
    VirtualListLayout layout = measuredLayout();

    REQUIRE(layout.indexAt(0.0) == 0);
    REQUIRE(layout.indexAt(29.0) == 0);
    REQUIRE(layout.indexAt(30.0) == 1);
    REQUIRE(layout.indexAt(49.0) == 1);
    REQUIRE(layout.indexAt(50.0) == 2);
    REQUIRE(layout.indexAt(99.0) == 2);
    REQUIRE(layout.indexAt(100.0) == 3);
    REQUIRE(layout.indexAt(139.0) == 3);
    REQUIRE(layout.indexAt(1000.0) == 3);
    REQUIRE(layout.indexAt(-5.0) == 0);
}

TEST_CASE("virtual list layout computes visible and mount ranges", "[virtualization][layout]") {
    VirtualListLayout layout = measuredLayout();

    VirtualListLayout::Range const visible = layout.visibleRange(30.0, 25.0);
    REQUIRE(visible.first == 1);
    REQUIRE(visible.last == 2);

    VirtualListLayout::Range const tall = layout.visibleRange(0.0, 1000.0);
    REQUIRE(tall.first == 0);
    REQUIRE(tall.last == 3);

    VirtualListLayout::Range const mounted = layout.mountRange(30.0, 25.0, 30.0);
    REQUIRE(mounted.first == 0);
    REQUIRE(mounted.last == 2);
}

TEST_CASE("virtual list layout handles empty and degenerate viewports", "[virtualization][layout]") {
    VirtualListLayout layout;

    REQUIRE(layout.isEmpty());
    REQUIRE(layout.indexAt(10.0) == -1);
    REQUIRE(layout.visibleRange(0.0, 100.0).isEmpty());

    layout.reset(1, 20.0);
    REQUIRE(layout.visibleRange(0.0, 0.0).isEmpty());

    VirtualListLayout::Range const single = layout.visibleRange(0.0, 10.0);
    REQUIRE(single.first == 0);
    REQUIRE(single.last == 0);
}

TEST_CASE("virtual list layout refines measurements without desyncing offsets", "[virtualization][layout]") {
    VirtualListLayout layout;
    layout.reset(3, 10.0);

    REQUIRE(approx(layout.totalHeight(), 30.0));
    REQUIRE(!layout.hasMeasurement(1));

    layout.setMeasuredHeight(1, 40.0);
    REQUIRE(layout.hasMeasurement(1));
    REQUIRE(approx(layout.heightAt(1), 40.0));
    REQUIRE(approx(layout.totalHeight(), 60.0));
    REQUIRE(approx(layout.itemOffset(2), 50.0));

    layout.clearMeasurement(1);
    REQUIRE(!layout.hasMeasurement(1));
    REQUIRE(approx(layout.heightAt(1), 10.0));
    REQUIRE(approx(layout.totalHeight(), 30.0));
    REQUIRE(approx(layout.itemOffset(2), 20.0));
}

TEST_CASE("virtual list layout preserves an anchor across inserts", "[virtualization][layout]") {
    VirtualListLayout layout;
    layout.reset(3, 10.0);

    VirtualListLayout::Anchor const anchor = layout.resolveAnchor(25.0);
    REQUIRE(anchor.index == 2);
    REQUIRE(approx(anchor.offset, 5.0));

    layout.insertItems(0, 1);

    VirtualListLayout::Anchor const rebased = layout.anchorAfterInsert(anchor, 0, 1);
    REQUIRE(rebased.index == 3);
    REQUIRE(approx(layout.contentYForAnchor(rebased), 35.0));
    REQUIRE(approx(layout.contentYForAnchor(rebased) - layout.contentYForAnchor(anchor), 10.0));
}

TEST_CASE("virtual list layout leaves anchors untouched below an insert", "[virtualization][layout]") {
    VirtualListLayout layout;
    layout.reset(3, 10.0);

    VirtualListLayout::Anchor const anchor = layout.resolveAnchor(25.0);
    layout.insertItems(3, 2);

    VirtualListLayout::Anchor const rebased = layout.anchorAfterInsert(anchor, 3, 2);
    REQUIRE(rebased.index == anchor.index);
    REQUIRE(approx(layout.contentYForAnchor(rebased), 25.0));
}

TEST_CASE("virtual list layout preserves an anchor across removals", "[virtualization][layout]") {
    VirtualListLayout layout;
    layout.reset(3, 10.0);

    VirtualListLayout::Anchor const anchor = layout.resolveAnchor(25.0);
    layout.removeItems(0, 1);

    VirtualListLayout::Anchor const rebased = layout.anchorAfterRemove(anchor, 0, 1);
    REQUIRE(rebased.index == 1);
    REQUIRE(approx(rebased.offset, 5.0));
    REQUIRE(approx(layout.contentYForAnchor(rebased), 15.0));
}

TEST_CASE("virtual list layout clamps an anchor when its item is removed", "[virtualization][layout]") {
    VirtualListLayout layout;
    layout.reset(4, 10.0);

    VirtualListLayout::Anchor const anchor = layout.resolveAnchor(25.0);
    layout.removeItems(1, 2);

    VirtualListLayout::Anchor const rebased = layout.anchorAfterRemove(anchor, 1, 2);
    REQUIRE(rebased.index == 1);
    REQUIRE(approx(rebased.offset, 0.0));
}

TEST_CASE("virtual list layout tracks an anchor across moves", "[virtualization][layout]") {
    VirtualListLayout layout;
    layout.reset(5, 10.0);

    VirtualListLayout::Anchor const moved = layout.anchorAfterMove(VirtualListLayout::Anchor{2, 3.0}, 2, 4);
    REQUIRE(moved.index == 4);
    REQUIRE(approx(moved.offset, 3.0));

    VirtualListLayout::Anchor const shifted = layout.anchorAfterMove(VirtualListLayout::Anchor{4, 1.0}, 2, 4);
    REQUIRE(shifted.index == 3);

    VirtualListLayout::Anchor const untouched = layout.anchorAfterMove(VirtualListLayout::Anchor{0, 1.0}, 2, 4);
    REQUIRE(untouched.index == 0);
}

TEST_CASE("virtual list layout appends keep prefix sums consistent", "[virtualization][layout]") {
    VirtualListLayout layout;
    layout.reset(0, 60.0);

    for (int i = 0; i < 8; ++i) {
        layout.insertItems(i, 1);
    }

    REQUIRE(layout.count() == 8);
    REQUIRE(approx(layout.totalHeight(), 480.0));
    REQUIRE(approx(layout.itemOffset(8), 480.0));
    REQUIRE(layout.indexAt(300.0) == 5);

    layout.setMeasuredHeight(3, 100.0);
    REQUIRE(approx(layout.totalHeight(), 520.0));
    REQUIRE(approx(layout.itemOffset(4), 280.0));
    REQUIRE(approx(layout.itemOffset(8), 520.0));

    layout.setEstimate(5, 90.0);
    REQUIRE(approx(layout.totalHeight(), 550.0));
    REQUIRE(approx(layout.itemOffset(6), 430.0));

    layout.insertItems(8, 2);
    REQUIRE(layout.count() == 10);
    REQUIRE(approx(layout.totalHeight(), 670.0));
    REQUIRE(approx(layout.itemOffset(10), 670.0));
    REQUIRE(layout.indexAt(650.0) == 9);
}

TEST_CASE("virtual list layout invalidates estimates on demand", "[virtualization][layout]") {
    VirtualListLayout layout;
    layout.reset(3, 10.0);
    layout.setMeasuredHeight(0, 100.0);

    layout.clearAllMeasurements();

    REQUIRE(!layout.hasMeasurement(0));
    REQUIRE(approx(layout.totalHeight(), 30.0));

    layout.setEstimate(1, 25.0);
    REQUIRE(approx(layout.heightAt(1), 25.0));
    REQUIRE(approx(layout.totalHeight(), 45.0));
}
