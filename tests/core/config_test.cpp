#include <catch2/catch_test_macros.hpp>

import std;
import config;

TEST_CASE("config parses provider and vision fields", "[core][config]") {
    auto const parsed = mts::config::parseConfig(
        R"({"provider":{"model":"main-model","apiKey":"secret"},"vision":{"model":"vision-model"}})");

    REQUIRE(parsed.isOk());
    REQUIRE(parsed.value().provider.model == "main-model");
    REQUIRE(parsed.value().provider.apiKey == "secret");
    REQUIRE(parsed.value().vision.model == "vision-model");
}

TEST_CASE("config rejects malformed JSON", "[core][config]") {
    auto const parsed = mts::config::parseConfig("{ not json");

    REQUIRE(parsed.isErr());
}

TEST_CASE("default config supplies stable defaults", "[core][config]") {
    auto const defaults = mts::config::defaultConfig();

    REQUIRE(defaults.provider.origin == "https://openrouter.ai");
    REQUIRE(defaults.provider.model.empty());
    REQUIRE(defaults.provider.apiKey.empty());
}
