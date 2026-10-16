export module config;

import std;
import agent;
import core;

export namespace mts::config {

enum class ConfigErrorKind : core::u8 {
    Unknown,
    InvalidArgument,
    NotFound,
    Unavailable,
    Internal,
    Cancelled,
    ResourceExhausted,
    OutOfRange,
    DataLoss,
    PermissionDenied,
};

static_assert(core::ErrorKind<ConfigErrorKind>);

class ConfigView {
public:
    ConfigView() = default;
    explicit ConfigView(mts::agent::Config const& config, std::string prefix = {})
        : m_config(&config)
        , m_prefix(std::move(prefix)) {}

    [[nodiscard]] auto section(std::string_view key) const -> ConfigView {
        if (this->m_config == nullptr) {
            return ConfigView{};
        }

        std::string prefix = this->m_prefix;
        prefix += key;
        prefix += '.';

        return ConfigView{*this->m_config, std::move(prefix)};
    }

    [[nodiscard]] auto readString(std::string_view key, std::string& target) const -> core::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return core::Status::ok();
        }

        auto const text = value->asString();
        if (!text) {
            return core::Status::err(mts::agent::toFoundation(text.status()).error());
        }

        target = text.value();

        return core::Status::ok();
    }

    [[nodiscard]] auto readBool(std::string_view key, bool& target) const -> core::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return core::Status::ok();
        }

        auto const flag = value->asBool();
        if (!flag) {
            return core::Status::err(mts::agent::toFoundation(flag.status()).error());
        }

        target = flag.value();

        return core::Status::ok();
    }

    [[nodiscard]] auto readInt64(std::string_view key, std::int64_t& target) const -> core::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return core::Status::ok();
        }

        auto const number = value->asInt64();
        if (!number) {
            return core::Status::err(mts::agent::toFoundation(number.status()).error());
        }

        target = number.value();

        return core::Status::ok();
    }

    [[nodiscard]] auto readUInt32(std::string_view key, std::uint32_t& target) const -> core::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return core::Status::ok();
        }

        auto const number = value->asInt64();
        if (!number) {
            return core::Status::err(mts::agent::toFoundation(number.status()).error());
        }

        std::int64_t const raw = number.value();
        if (raw < 0 || static_cast<std::uint64_t>(raw) > std::numeric_limits<std::uint32_t>::max()) {
            return core::Status::err(ConfigErrorKind::OutOfRange, std::string(key) + " is out of range");
        }

        target = static_cast<std::uint32_t>(raw);

        return core::Status::ok();
    }

    [[nodiscard]] auto readUInt64(std::string_view key, std::uint64_t& target) const -> core::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return core::Status::ok();
        }

        auto const number = value->asInt64();
        if (!number) {
            return core::Status::err(mts::agent::toFoundation(number.status()).error());
        }

        std::int64_t const raw = number.value();
        if (raw < 0) {
            return core::Status::err(ConfigErrorKind::OutOfRange, std::string(key) + " is out of range");
        }

        target = static_cast<std::uint64_t>(raw);

        return core::Status::ok();
    }

    [[nodiscard]] auto readDouble(std::string_view key, double& target) const -> core::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return core::Status::ok();
        }

        auto const number = value->asDouble();
        if (!number) {
            return core::Status::err(mts::agent::toFoundation(number.status()).error());
        }

        target = number.value();

        return core::Status::ok();
    }

    [[nodiscard]] auto contains(std::string_view key) const -> bool {
        return this->entry(key).has_value();
    }

    [[nodiscard]] auto valid() const -> bool {
        return this->m_config != nullptr;
    }

private:
    [[nodiscard]] auto fullKey(std::string_view key) const -> std::string {
        std::string full = this->m_prefix;
        full += key;

        return full;
    }

    [[nodiscard]] auto entry(std::string_view key) const -> std::optional<mts::agent::ConfigValue> {
        if (this->m_config == nullptr) {
            return std::nullopt;
        }

        return this->m_config->get(this->fullKey(key));
    }

    mts::agent::Config const* m_config = nullptr;
    std::string m_prefix;
};

struct RuntimeConfig {
    std::uint32_t workers = 4;
};

struct MarkdownImageConfig {
    bool allowHttp = true;
    bool allowHttps = true;
    bool allowJpeg = true;
    bool allowPng = true;
    bool allowGif = true;
    std::uint32_t maxConcurrentRequests = 4;
    std::uint32_t maxQueuedRequests = 64;
    std::uint32_t timeoutMilliseconds = 15000;
    std::uint32_t maxRedirects = 5;
    std::uint32_t maxGifFrames = 300;
    std::uint64_t maxResponseBytes = 10 * 1024 * 1024;
    std::uint64_t maxDecodedPixels = 40 * 1024 * 1024;
    std::uint64_t maxCacheBytes = 128 * 1024 * 1024;
};

[[nodiscard]] inline auto applyConfig(ConfigView const& view, RuntimeConfig& target) -> core::Status {
    return view.section("runtime").readUInt32("workers", target.workers);
}

[[nodiscard]] inline auto applyConfig(ConfigView const& view, MarkdownImageConfig& target) -> core::Status {
    ConfigView const images = view.section("markdown").section("images");
    core::Status status = images.readBool("allowHttp", target.allowHttp);
    if (!status) {
        return status;
    }
    status = images.readBool("allowHttps", target.allowHttps);
    if (!status) {
        return status;
    }
    status = images.readBool("allowJpeg", target.allowJpeg);
    if (!status) {
        return status;
    }
    status = images.readBool("allowPng", target.allowPng);
    if (!status) {
        return status;
    }
    status = images.readBool("allowGif", target.allowGif);
    if (!status) {
        return status;
    }
    status = images.readUInt32("maxConcurrentRequests", target.maxConcurrentRequests);
    if (!status) {
        return status;
    }
    status = images.readUInt32("maxQueuedRequests", target.maxQueuedRequests);
    if (!status) {
        return status;
    }
    status = images.readUInt32("timeoutMilliseconds", target.timeoutMilliseconds);
    if (!status) {
        return status;
    }
    status = images.readUInt32("maxRedirects", target.maxRedirects);
    if (!status) {
        return status;
    }
    status = images.readUInt32("maxGifFrames", target.maxGifFrames);
    if (!status) {
        return status;
    }
    status = images.readUInt64("maxResponseBytes", target.maxResponseBytes);
    if (!status) {
        return status;
    }
    status = images.readUInt64("maxDecodedPixels", target.maxDecodedPixels);
    if (!status) {
        return status;
    }
    status = images.readUInt64("maxCacheBytes", target.maxCacheBytes);
    if (!status) {
        return status;
    }

    if (target.maxConcurrentRequests == 0 || target.maxConcurrentRequests > 32
        || target.maxQueuedRequests == 0 || target.maxQueuedRequests > 1024
        || target.timeoutMilliseconds < 1000 || target.timeoutMilliseconds > 300000
        || target.maxRedirects > 10 || target.maxGifFrames == 0 || target.maxGifFrames > 10000
        || target.maxResponseBytes == 0 || target.maxResponseBytes > 100 * 1024 * 1024
        || target.maxDecodedPixels == 0 || target.maxDecodedPixels > 200 * 1024 * 1024
        || target.maxCacheBytes > 2ULL * 1024 * 1024 * 1024) {
        return core::Status::err(ConfigErrorKind::OutOfRange, "markdown.images settings are out of range");
    }

    return core::Status::ok();
}

class Config {
public:
    Config() = default;
    explicit Config(mts::agent::Config config)
        : m_config(std::move(config)) {}

    [[nodiscard]] static auto fromJson(std::string_view jsonText) -> core::Result<Config> {
        auto const parsed = mts::agent::ConfigLoader::fromJsonString(jsonText);
        if (!parsed) {
            return core::Result<Config>::err(mts::agent::toFoundation(parsed.status()).error());
        }

        return Config{parsed.value()};
    }

    [[nodiscard]] static auto fromFile(std::filesystem::path const& path) -> core::Result<Config> {
        auto const parsed = mts::agent::ConfigLoader::fromJsonFile(path);
        if (!parsed) {
            return core::Result<Config>::err(mts::agent::toFoundation(parsed.status()).error());
        }

        return Config{parsed.value()};
    }

    auto merge(Config const& other) -> core::Status {
        return mts::agent::toFoundation(this->m_config.merge(other.m_config));
    }

    [[nodiscard]] auto view() const -> ConfigView {
        return ConfigView{this->m_config};
    }

    [[nodiscard]] auto raw() const -> mts::agent::Config const& {
        return this->m_config;
    }

private:
    mts::agent::Config m_config;
};

[[nodiscard]] auto loadConfigOrDefaults(std::filesystem::path const& path, std::string& error) -> Config {
    error.clear();

    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError)) {
        return Config{};
    }

    auto const loaded = Config::fromFile(path);
    if (!loaded) {
        error = std::string(loaded.error().message());

        return Config{};
    }

    return loaded.value();
}

} // END OF NAMESPACE `mts::config`
