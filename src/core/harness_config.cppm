export module harness:config;

import std;
import foundation;
import config;

export namespace mts::harness {

struct ServerConfig {
    std::string origin = "http://127.0.0.1:8000";
    std::string graphqlPath = "api/command/graphql";
    std::string schemaPath = "api/command/schema";
};

struct NamingConfig {
    std::string model;
};

struct BudgetConfig {
    std::uint32_t maxTurns = 16;
    std::uint64_t maxInputTokens = 0;
    std::uint64_t maxOutputTokens = 0;
    double maxCost = 0.0;
};

struct CompactionConfig {
    bool enabled = true;
    std::uint32_t maxMessages = 40;
    std::uint64_t maxBytes = 0;
};

[[nodiscard]] auto applyConfig(mts::config::ConfigView const& view, ServerConfig& target) -> mts::foundation::Status {
    auto const section = view.section("server");
    if (auto const status = section.readString("origin", target.origin); !status) {
        return status;
    }

    if (auto const status = section.readString("graphqlPath", target.graphqlPath); !status) {
        return status;
    }

    if (auto const status = section.readString("schemaPath", target.schemaPath); !status) {
        return status;
    }

    return mts::foundation::Status::ok();
}

[[nodiscard]] auto applyConfig(mts::config::ConfigView const& view, NamingConfig& target) -> mts::foundation::Status {
    auto const section = view.section("naming");
    if (auto const status = section.readString("model", target.model); !status) {
        return status;
    }

    return mts::foundation::Status::ok();
}

[[nodiscard]] auto applyConfig(mts::config::ConfigView const& view, BudgetConfig& target) -> mts::foundation::Status {
    auto const section = view.section("budgets");
    if (auto const status = section.readUInt32("maxTurns", target.maxTurns); !status) {
        return status;
    }

    if (auto const status = section.readUInt64("maxInputTokens", target.maxInputTokens); !status) {
        return status;
    }

    if (auto const status = section.readUInt64("maxOutputTokens", target.maxOutputTokens); !status) {
        return status;
    }

    if (auto const status = section.readDouble("maxCost", target.maxCost); !status) {
        return status;
    }

    return mts::foundation::Status::ok();
}

[[nodiscard]] auto applyConfig(mts::config::ConfigView const& view, CompactionConfig& target) -> mts::foundation::Status {
    auto const section = view.section("compaction");
    if (auto const status = section.readBool("enabled", target.enabled); !status) {
        return status;
    }

    if (auto const status = section.readUInt32("maxMessages", target.maxMessages); !status) {
        return status;
    }

    if (auto const status = section.readUInt64("maxBytes", target.maxBytes); !status) {
        return status;
    }

    return mts::foundation::Status::ok();
}

} // END OF NAMESPACE `mts::harness`
