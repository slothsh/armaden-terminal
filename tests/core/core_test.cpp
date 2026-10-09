#include <catch2/catch_test_macros.hpp>

import std;
import core;

using namespace mts::core;

namespace {

enum class TestErrorKind : u8 {
    Unknown,
    InvalidArgument,
    NotFound,
    Internal,
    Unavailable,
    DataLoss,
};

enum class NetworkErrorKind : u8 {
    Timeout,
    Refused,
};

struct CustomError {
    [[nodiscard]] auto kind() const -> TestErrorKind {
        return TestErrorKind::Unavailable;
    }

    [[nodiscard]] auto message() const -> std::string_view {
        return "custom failure";
    }

    friend auto operator<<(std::ostream& stream, CustomError const& error) -> std::ostream& {
        return stream << "custom(" << error.message() << ")";
    }
};

static_assert(ErrorKind<TestErrorKind>);
static_assert(ErrorKind<NetworkErrorKind>);
static_assert(!ErrorKind<int>);
static_assert(UserError<CustomError>);

}

TEST_CASE("Result carries a value", "[core]") {
    Result<int> const result = Result<int>::ok(42);

    REQUIRE(result.isOk());
    REQUIRE(result.hasValue());
    REQUIRE_FALSE(result.isErr());
    REQUIRE(result.value() == 42);
    REQUIRE(*result == 42);
}

TEST_CASE("Result carries an error", "[core]") {
    Result<int> const result = Result<int>::err(Error(TestErrorKind::NotFound, "missing"));

    REQUIRE(result.isErr());
    REQUIRE_FALSE(result.hasValue());
    REQUIRE(result.error().kindName() == "NotFound");
    REQUIRE(result.error().kind<TestErrorKind>() == TestErrorKind::NotFound);
    REQUIRE(result.error().message() == "missing");
}

TEST_CASE("Status is Result<void>", "[core]") {
    Status const ok = Status::ok();
    REQUIRE(ok.isOk());

    Status const failed = Status::err(Error(TestErrorKind::Internal, "boom"));
    REQUIRE(failed.isErr());
    REQUIRE(failed.error().kind<TestErrorKind>() == TestErrorKind::Internal);
}

TEST_CASE("each domain defines its own error kind", "[core]") {
    Error const test = Error(TestErrorKind::InvalidArgument, "bad input");
    Error const network = Error(NetworkErrorKind::Timeout, "timed out");

    REQUIRE(test.kindName() == "InvalidArgument");
    REQUIRE(test.kind<TestErrorKind>() == TestErrorKind::InvalidArgument);
    REQUIRE(network.kindName() == "Timeout");
    REQUIRE(network.kind<NetworkErrorKind>() == NetworkErrorKind::Timeout);
}

TEST_CASE("Error formats kind, message and context", "[core]") {
    Error const error = Error(TestErrorKind::InvalidArgument, "bad input").with("field", "x");

    REQUIRE(error.format() == "InvalidArgument: bad input (field=x)");
    REQUIRE(error.context().size() == 1);
    REQUIRE(error.context()[0].key == "field");
    REQUIRE(error.context()[0].value == "x");
}

TEST_CASE("Error prints and formats at the boundary", "[core]") {
    Error const error = Error(TestErrorKind::InvalidArgument, "bad input").with("field", "x");

    std::ostringstream stream;
    stream << error;

    REQUIRE(stream.str() == "InvalidArgument: bad input (field=x)");
    REQUIRE(to_string(error) == "InvalidArgument: bad input (field=x)");
    REQUIRE(std::format("{}", error) == "InvalidArgument: bad input (field=x)");
    REQUIRE(std::format("error=[{}]", error) == "error=[InvalidArgument: bad input (field=x)]");
}

TEST_CASE("Error type-erases user errors", "[core]") {
    Error const error = Error(CustomError{});

    REQUIRE(error.kindName() == "Unavailable");
    REQUIRE(error.message() == "custom failure");
    REQUIRE(error.format() == "Unavailable: custom(custom failure)");
}

TEST_CASE("Error is copyable and shares its payload", "[core]") {
    Error const original = Error(TestErrorKind::DataLoss, "corrupt");
    Error const copy = original;
    Error const extended = original.with("offset", "12");

    REQUIRE(copy.format() == "DataLoss: corrupt");
    REQUIRE(extended.format() == "DataLoss: corrupt (offset=12)");
    REQUIRE(original.context().empty());
}
