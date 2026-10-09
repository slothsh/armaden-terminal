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
