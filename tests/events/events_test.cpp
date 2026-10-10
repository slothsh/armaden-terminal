#include <catch2/catch_test_macros.hpp>

import std;
import events;

TEST_CASE("event bus delivers to subscribers and stops after unsubscribe", "[core][events]") {
    mts::events::EventBus bus;
    int started = 0;
    int finished = 0;

    auto const startedId = bus.subscribe<mts::events::RunStarted>(
        [&started](mts::events::RunStarted const&) { ++started; }
    );
    bus.subscribe<mts::events::RunFinished>(
        [&finished](mts::events::RunFinished const&) { ++finished; }
    );

    bus.publish(mts::events::RunStarted{.session = "s"});
    REQUIRE(started == 1);
    REQUIRE(finished == 0);

    bus.unsubscribe<mts::events::RunStarted>(startedId);
    bus.publish(mts::events::RunStarted{.session = "s"});
    REQUIRE(started == 1);

    bus.publish(mts::events::RunFinished{.session = "s"});
    REQUIRE(finished == 1);
}

TEST_CASE("event bus publishes interaction events synchronously", "[core][events]") {
    mts::events::EventBus bus;
    mts::events::FocusChanged seen{
        .previous = 0,
        .current = 0,
        .domain = {},
    };

    [[maybe_unused]] auto const id = bus.subscribe<mts::events::FocusChanged>(
        [&seen](mts::events::FocusChanged const& event) {
            seen.previous = event.previous;
            seen.current = event.current;
            seen.domain = event.domain;
        }
    );

    bus.publishDirect(mts::events::FocusChanged{.previous = 3, .current = 7, .domain = "app.list"});

    REQUIRE(seen.previous == 3);
    REQUIRE(seen.current == 7);
    REQUIRE(seen.domain == "app.list");
}
