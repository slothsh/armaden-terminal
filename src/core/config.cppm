export module config;

import std;
import agent;

export namespace mts::config {

struct ProviderConfig {
    std::string origin = "https://openrouter.ai";
    std::string path = "/api/v1/chat/completions";
    std::string model;
    std::string apiKey;
    std::string referer;
    std::string title;
};

struct ServerConfig {
    std::string origin = "http://127.0.0.1:8000";
    std::string graphqlPath = "api/command/graphql";
    std::string schemaPath = "api/command/schema";
};

struct RuntimeConfig {
    std::uint32_t workers = 4;
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

struct NamingConfig {
    std::string model;
};

struct VisionConfig {
    std::string model;
    std::uint32_t maxPixels = 1024;
    std::uint32_t timeoutMs = 30000;
};

struct CacheConfig {
    std::string tilesDir = "~/.cache/mindtosis/tiles";
};

struct AppConfig {
    ProviderConfig provider;
    ServerConfig server;
    RuntimeConfig runtime;
    BudgetConfig budgets;
    CompactionConfig compaction;
    NamingConfig naming;
    CacheConfig cache;
    VisionConfig vision;
};

[[nodiscard]] auto defaultConfig() -> AppConfig;
[[nodiscard]] auto parseConfig(std::string_view jsonText) -> mts::agent::Result<AppConfig>;
[[nodiscard]] auto loadConfig(std::filesystem::path const& path) -> mts::agent::Result<AppConfig>;
[[nodiscard]] auto loadConfigOrDefaults(std::filesystem::path const& path, std::string& error) -> AppConfig;
[[nodiscard]] auto configPath() -> std::filesystem::path;

}

namespace mts::config::detail {

using mts::agent::Config;
using mts::agent::Status;

auto readString(Config const& config, std::string_view key, std::string& target) -> Status {
    auto const value = config.get(key);
    if (!value)
        return Status::ok();
    auto const text = value->asString();
    if (!text)
        return text.status();
    target = text.value();
    return Status::ok();
}

auto readBool(Config const& config, std::string_view key, bool& target) -> Status {
    auto const value = config.get(key);
    if (!value)
        return Status::ok();
    auto const flag = value->asBool();
    if (!flag)
        return flag.status();
    target = flag.value();
    return Status::ok();
}

auto readDouble(Config const& config, std::string_view key, double& target) -> Status {
    auto const value = config.get(key);
    if (!value)
        return Status::ok();
    auto const number = value->asDouble();
    if (!number)
        return number.status();
    target = number.value();
    return Status::ok();
}

auto readUInt32(Config const& config, std::string_view key, std::uint32_t& target) -> Status {
    auto const value = config.get(key);
    if (!value)
        return Status::ok();
    auto const number = value->asInt64();
    if (!number)
        return number.status();
    std::int64_t const raw = number.value();
    if (raw < 0 || static_cast<std::uint64_t>(raw) > std::numeric_limits<std::uint32_t>::max())
        return Status::outOfRange(std::string(key) + " is out of range");
    target = static_cast<std::uint32_t>(raw);
    return Status::ok();
}

auto readUInt64(Config const& config, std::string_view key, std::uint64_t& target) -> Status {
    auto const value = config.get(key);
    if (!value)
        return Status::ok();
    auto const number = value->asInt64();
    if (!number)
        return number.status();
    std::int64_t const raw = number.value();
    if (raw < 0)
        return Status::outOfRange(std::string(key) + " is out of range");
    target = static_cast<std::uint64_t>(raw);
    return Status::ok();
}

auto applyConfig(Config const& config, mts::config::AppConfig& app) -> Status {
    if (auto const status = readString(config, "provider.origin", app.provider.origin); !status)
        return status;
    if (auto const status = readString(config, "provider.path", app.provider.path); !status)
        return status;
    if (auto const status = readString(config, "provider.model", app.provider.model); !status)
        return status;
    if (auto const status = readString(config, "provider.apiKey", app.provider.apiKey); !status)
        return status;
    if (auto const status = readString(config, "provider.referer", app.provider.referer); !status)
        return status;
    if (auto const status = readString(config, "provider.title", app.provider.title); !status)
        return status;

    if (auto const status = readString(config, "server.origin", app.server.origin); !status)
        return status;
    if (auto const status = readString(config, "server.graphqlPath", app.server.graphqlPath); !status)
        return status;
    if (auto const status = readString(config, "server.schemaPath", app.server.schemaPath); !status)
        return status;

    if (auto const status = readUInt32(config, "runtime.workers", app.runtime.workers); !status)
        return status;

    if (auto const status = readUInt32(config, "budgets.maxTurns", app.budgets.maxTurns); !status)
        return status;
    if (auto const status = readUInt64(config, "budgets.maxInputTokens", app.budgets.maxInputTokens); !status)
        return status;
    if (auto const status = readUInt64(config, "budgets.maxOutputTokens", app.budgets.maxOutputTokens); !status)
        return status;
    if (auto const status = readDouble(config, "budgets.maxCost", app.budgets.maxCost); !status)
        return status;

    if (auto const status = readBool(config, "compaction.enabled", app.compaction.enabled); !status)
        return status;
    if (auto const status = readUInt32(config, "compaction.maxMessages", app.compaction.maxMessages); !status)
        return status;
    if (auto const status = readUInt64(config, "compaction.maxBytes", app.compaction.maxBytes); !status)
        return status;

    if (auto const status = readString(config, "naming.model", app.naming.model); !status)
        return status;

    if (auto const status = readString(config, "cache.tilesDir", app.cache.tilesDir); !status)
        return status;

    if (auto const status = readString(config, "vision.model", app.vision.model); !status)
        return status;
    if (auto const status = readUInt32(config, "vision.maxPixels", app.vision.maxPixels); !status)
        return status;
    if (auto const status = readUInt32(config, "vision.timeoutMs", app.vision.timeoutMs); !status)
        return status;

    return Status::ok();
}

}

export namespace mts::config {

auto defaultConfig() -> AppConfig {
    return AppConfig{};
}

auto parseConfig(std::string_view jsonText) -> mts::agent::Result<AppConfig> {
    auto const parsed = mts::agent::ConfigLoader::fromJsonString(jsonText);
    if (!parsed)
        return parsed.status();

    AppConfig app;
    if (auto const status = detail::applyConfig(parsed.value(), app); !status)
        return status;
    return app;
}

auto loadConfig(std::filesystem::path const& path) -> mts::agent::Result<AppConfig> {
    auto const parsed = mts::agent::ConfigLoader::fromJsonFile(path);
    if (!parsed)
        return parsed.status();

    AppConfig app;
    if (auto const status = detail::applyConfig(parsed.value(), app); !status)
        return status;
    return app;
}

auto loadConfigOrDefaults(std::filesystem::path const& path, std::string& error) -> AppConfig {
    error.clear();

    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError))
        return defaultConfig();

    auto const loaded = loadConfig(path);
    if (!loaded) {
        error = loaded.status().message();
        return defaultConfig();
    }

    return loaded.value();
}

auto configPath() -> std::filesystem::path {
    if (char const* env = std::getenv("MINDTOIS_CONFIG"); env != nullptr && *env != '\0')
        return std::filesystem::path(env);
    return std::filesystem::path("config/mindtosis.json");
}

}
