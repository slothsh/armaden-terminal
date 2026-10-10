#include <catch2/catch_test_macros.hpp>

import std;
import interaction;

using namespace mts::interaction;

namespace {

auto makeClock(std::int64_t& tick) -> TraceRing::Clock {
    return [&tick] { return tick; };
}

}

TEST_CASE("trace ring records entries in order with timestamps", "[interaction][trace]") {
    std::int64_t tick = 100;
    TraceRing ring(16, makeClock(tick));

    Domain const list("app.list");
    TargetId const row{1, 1};

    ring.record(TraceKind::FocusChanged, row, list);
    tick = 250;
    ring.record(TraceKind::Pressed, row, list, "primary@10,20");

    auto const entries = ring.entries();

    REQUIRE(entries.size() == 2);
    REQUIRE(entries[0].kind == TraceKind::FocusChanged);
    REQUIRE(entries[0].timeMs == 100);
    REQUIRE(entries[0].target == row);
    REQUIRE(entries[0].domain.path() == "app.list");
    REQUIRE(entries[1].kind == TraceKind::Pressed);
    REQUIRE(entries[1].detail == "primary@10,20");
}

TEST_CASE("trace ring drops oldest entries beyond capacity", "[interaction][trace]") {
    std::int64_t tick = 0;
    TraceRing ring(3, makeClock(tick));

    Domain const list("app.list");
    for (int index = 0; index < 5; ++index) {
        ring.record(TraceKind::FocusChanged, TargetId{static_cast<u32>(index), 1}, list);
        tick += 1;
    }

    REQUIRE(ring.size() == 3);
    REQUIRE(ring.dropped() == 2);

    auto const entries = ring.entries();
    REQUIRE(entries[0].target.index == 2);
    REQUIRE(entries[2].target.index == 4);
}

TEST_CASE("trace ring honours a zero capacity as disabled", "[interaction][trace]") {
    TraceRing ring(0);

    Domain const list("app.list");
    ring.record(TraceKind::FocusChanged, TargetId{1, 1}, list);

    REQUIRE(ring.size() == 0);
    REQUIRE(ring.entries().empty());
    REQUIRE(ring.dropped() == 0);
}

TEST_CASE("trace ring limits queried entries to the most recent", "[interaction][trace]") {
    std::int64_t tick = 0;
    TraceRing ring(8, makeClock(tick));

    Domain const list("app.list");
    for (int index = 0; index < 4; ++index) {
        ring.record(TraceKind::HoverChanged, TargetId{static_cast<u32>(index), 1}, list);
        tick += 1;
    }

    auto const recent = ring.entries(2);

    REQUIRE(recent.size() == 2);
    REQUIRE(recent[0].target.index == 2);
    REQUIRE(recent[1].target.index == 3);
}

TEST_CASE("trace ring shrinks and clears", "[interaction][trace]") {
    std::int64_t tick = 0;
    TraceRing ring(8, makeClock(tick));

    Domain const list("app.list");
    for (int index = 0; index < 6; ++index) {
        ring.record(TraceKind::CurrentChanged, TargetId{static_cast<u32>(index), 1}, list);
        tick += 1;
    }

    ring.setCapacity(4);
    REQUIRE(ring.size() == 4);
    REQUIRE(ring.dropped() == 2);

    ring.clear();
    REQUIRE(ring.size() == 0);
    REQUIRE(ring.capacity() == 4);
}
