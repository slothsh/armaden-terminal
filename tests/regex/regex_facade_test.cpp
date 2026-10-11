#include <catch2/catch_test_macros.hpp>

import std;
import regex;

using namespace mts::regex;

TEST_CASE("regex facade validates a code-fence language identifier", "[regex]") {
    REQUIRE(fullMatch(Pattern::CodeFenceLanguage, "cpp"));
    REQUIRE(fullMatch(Pattern::CodeFenceLanguage, "c++"));
    REQUIRE_FALSE(fullMatch(Pattern::CodeFenceLanguage, "cpp linenumbers"));
}

