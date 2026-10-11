#include <catch2/catch_test_macros.hpp>

import config;
import harness;

TEST_CASE("harness server config reads a bearer token", "[harness][config]") {
    auto const parsed = mts::config::Config::fromJson(
        R"({"server":{"origin":"https://game.example","bearerToken":"abc123"}})"
    );
    REQUIRE(parsed.isOk());

    mts::harness::ServerConfig server;
    REQUIRE(mts::harness::applyConfig(parsed.value().view(), server).isOk());

    REQUIRE(server.origin == "https://game.example");
    REQUIRE(server.bearerToken == "abc123");
}

TEST_CASE("harness server config leaves the bearer token empty when unset", "[harness][config]") {
    auto const parsed = mts::config::Config::fromJson(R"({"server":{"origin":"https://game.example"}})");
    REQUIRE(parsed.isOk());

    mts::harness::ServerConfig server;
    REQUIRE(mts::harness::applyConfig(parsed.value().view(), server).isOk());

    REQUIRE(server.bearerToken.empty());
}
