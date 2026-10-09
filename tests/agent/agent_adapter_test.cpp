#include <catch2/catch_test_macros.hpp>

import std;
import agent;
import core;

using namespace mts::core;

TEST_CASE("ok status converts to an ok core status", "[agent][adapter]") {
    Status const converted = mts::agent::toFoundation(mts::agent::Status::ok());

    REQUIRE(converted.isOk());
}

TEST_CASE("lgc error status converts to a core error", "[agent][adapter]") {
    Status const converted = mts::agent::toFoundation(mts::agent::Status::notFound("missing thing"));

    REQUIRE(converted.isErr());
    REQUIRE(converted.error().kindName() == "NotFound");
    REQUIRE(converted.error().kind<mts::agent::AgentErrorKind>() == mts::agent::AgentErrorKind::NotFound);
    REQUIRE(converted.error().message() == "missing thing");
}

TEST_CASE("empty lgc messages fall back to the code name", "[agent][adapter]") {
    Status const converted = mts::agent::toFoundation(mts::agent::Status::unavailable());

    REQUIRE(converted.isErr());
    REQUIRE(converted.error().message() == "unavailable");
}

TEST_CASE("core error converts back to an lgc status", "[agent][adapter]") {
    Status const error = Status::err(Error(mts::agent::AgentErrorKind::Unavailable, "down"));

    mts::agent::Status const converted = mts::agent::fromFoundation(error);

    REQUIRE(converted.isOk() == false);
    REQUIRE(converted.code() == mts::agent::StatusCode::Unavailable);
    REQUIRE(converted.message() == "down");
}

TEST_CASE("status round-trips through the adapter", "[agent][adapter]") {
    for (auto const code : {
             mts::agent::StatusCode::InvalidArgument,
             mts::agent::StatusCode::NotFound,
             mts::agent::StatusCode::ResourceExhausted,
             mts::agent::StatusCode::Internal,
         }) {
        mts::agent::Status const original(code, "detail");
        mts::agent::Status const roundTripped = mts::agent::fromFoundation(mts::agent::toFoundation(original));

        REQUIRE(roundTripped.code() == code);
        REQUIRE(roundTripped.message() == "detail");
    }
}

TEST_CASE("result values convert to core results", "[agent][adapter]") {
    Result<int> const ok = mts::agent::toFoundation(mts::agent::Result<int>::fromValue(7));

    REQUIRE(ok.isOk());
    REQUIRE(ok.value() == 7);

    Result<int> const failed = mts::agent::toFoundation(
        mts::agent::Result<int>::fromStatus(mts::agent::Status::internal("boom"))
    );

    REQUIRE(failed.isErr());
    REQUIRE(failed.error().kindName() == "Internal");
    REQUIRE(failed.error().message() == "boom");
}

TEST_CASE("void results convert to core status", "[agent][adapter]") {
    Status const ok = mts::agent::toFoundation(mts::agent::Result<void>::ok());
    REQUIRE(ok.isOk());

    Status const failed = mts::agent::toFoundation(
        mts::agent::Result<void>::fromStatus(mts::agent::Status::dataLoss("corrupt"))
    );

    REQUIRE(failed.isErr());
    REQUIRE(failed.error().kindName() == "DataLoss");
}
