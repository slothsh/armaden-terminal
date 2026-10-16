#include <catch2/catch_test_macros.hpp>

import std;
import core;
import config;
import provider;

TEST_CASE("config view reads the provider and vision sections", "[core][config]") {
    auto const parsed = mts::config::Config::fromJson(
        R"({"provider":{"model":"main-model","apiKey":"secret"},"vision":{"model":"vision-model"}})"
    );
    REQUIRE(parsed.isOk());

    auto const view = parsed.value().view();

    mts::model::ProviderConfig provider;
    mts::model::VisionConfig vision;
    REQUIRE(mts::model::applyConfig(view, provider).isOk());
    REQUIRE(mts::model::applyConfig(view, vision).isOk());

    REQUIRE(provider.model == "main-model");
    REQUIRE(provider.apiKey == "secret");
    REQUIRE(vision.model == "vision-model");
}

TEST_CASE("config view rejects malformed JSON", "[core][config]") {
    auto const parsed = mts::config::Config::fromJson("{ not json");

    REQUIRE(parsed.isErr());
}

TEST_CASE("domain config keeps defaults when its section is absent", "[core][config]") {
    auto const parsed = mts::config::Config::fromJson(R"({})");
    REQUIRE(parsed.isOk());

    auto const view = parsed.value().view();

    mts::model::ProviderConfig provider;
    REQUIRE(mts::model::applyConfig(view, provider).isOk());
    REQUIRE(provider.origin == "https://openrouter.ai");
    REQUIRE(provider.model.empty());
}

TEST_CASE("config merges an overlay for dynamic reloads", "[core][config]") {
    auto base = mts::config::Config::fromJson(R"({"provider":{"model":"base","apiKey":"key"}})");
    auto overlay = mts::config::Config::fromJson(R"({"provider":{"model":"override"}})");
    REQUIRE(base.isOk());
    REQUIRE(overlay.isOk());

    REQUIRE(base.value().merge(overlay.value()).isOk());

    mts::model::ProviderConfig provider;
    REQUIRE(mts::model::applyConfig(base.value().view(), provider).isOk());
    REQUIRE(provider.model == "override");
    REQUIRE(provider.apiKey == "key");
}
