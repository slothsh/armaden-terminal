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
    [[maybe_unused]] auto const finishedId = bus.subscribe<mts::events::RunFinished>(
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

TEST_CASE("event bus publishes layer lifecycle events synchronously", "[core][events]") {
    mts::events::EventBus bus;
    mts::events::LayerChanged seen{
        .id = 0,
        .depth = 0,
        .domain = {},
        .action = {},
    };

    [[maybe_unused]] auto const id = bus.subscribe<mts::events::LayerChanged>(
        [&seen](mts::events::LayerChanged const& event) {
            seen = event;
        }
    );

    bus.publishDirect(mts::events::LayerChanged{
        .id = 9,
        .depth = 2,
        .domain = "app.dialog",
        .action = "pushed",
    });

    REQUIRE(seen.id == 9);
    REQUIRE(seen.depth == 2);
    REQUIRE(seen.domain == "app.dialog");
    REQUIRE(seen.action == "pushed");
}

TEST_CASE("event bus publishes action invocation events synchronously", "[core][events]") {
    mts::events::EventBus bus;
    mts::events::ActionInvoked seen{
        .actionId = {},
        .target = 0,
        .domain = {},
        .semanticKey = {},
    };

    [[maybe_unused]] auto const id = bus.subscribe<mts::events::ActionInvoked>(
        [&seen](mts::events::ActionInvoked const& event) {
            seen = event;
        }
    );

    bus.publishDirect(mts::events::ActionInvoked{
        .actionId = "session.remove",
        .target = 42,
        .domain = "app.sessionList",
        .semanticKey = "session-1",
    });

    REQUIRE(seen.actionId == "session.remove");
    REQUIRE(seen.target == 42);
    REQUIRE(seen.domain == "app.sessionList");
    REQUIRE(seen.semanticKey == "session-1");
}
