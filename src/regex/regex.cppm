#include <ctre.hpp>

export module regex;

import std;

export namespace mts::regex {

enum class Pattern {
    CodeFenceLanguage,
};

[[nodiscard]] auto fullMatch(Pattern pattern, std::string_view value) -> bool;

}

auto mts::regex::fullMatch(Pattern pattern, std::string_view value) -> bool {
    switch (pattern) {
        case Pattern::CodeFenceLanguage: {
            return ctre::match<"^[A-Za-z0-9_][A-Za-z0-9_.\\+\\-]*$">(value);
        }

    }

    return false;
}
