#include <catch2/catch_test_macros.hpp>

import std;
import agent;

TEST_CASE("agent facade exposes message and tool value types", "[agent]") {
    STATIC_REQUIRE(sizeof(mts::agent::BaseMessage) > 0);
    STATIC_REQUIRE(sizeof(mts::agent::ToolSpec) > 0);

    auto const message = mts::agent::BaseMessage::human("hi");
    REQUIRE(message.content_ == "hi");
}
