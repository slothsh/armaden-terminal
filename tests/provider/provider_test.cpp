#include <catch2/catch_test_macros.hpp>

import std;
import agent;
import provider;

static_assert(mts::model::ModelProvider<mts::model::OpenRouterProvider>);
static_assert(mts::model::ModelProvider<mts::model::FakeModelProvider>);

TEST_CASE("fake provider returns a scripted response", "[core][provider]") {
    mts::model::FakeModelProvider fake{{mts::agent::BaseMessage::ai("hello")}};
    mts::model::ProviderHandle provider{std::move(fake)};

    auto const result = provider.invoke({mts::agent::BaseMessage::human("hi")});

    REQUIRE(result.isOk());
    REQUIRE(result.value().content_ == "hello");
}

TEST_CASE("fake provider without scripted responses reports an error", "[core][provider]") {
    mts::model::ProviderHandle provider{mts::model::FakeModelProvider{}};

    auto const result = provider.invoke({mts::agent::BaseMessage::human("hi")});

    REQUIRE(result.isErr());
}
