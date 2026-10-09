export module config;

import std;
import agent;
import foundation;

export namespace mts::config {

enum class ConfigErrorKind : foundation::u8 {
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

static_assert(foundation::ErrorKind<ConfigErrorKind>);

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

    [[nodiscard]] auto readString(std::string_view key, std::string& target) const -> foundation::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return foundation::Status::ok();
        }

        auto const text = value->asString();
        if (!text) {
            return foundation::Status::err(mts::agent::toFoundation(text.status()).error());
        }

        target = text.value();

        return foundation::Status::ok();
    }

    [[nodiscard]] auto readBool(std::string_view key, bool& target) const -> foundation::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return foundation::Status::ok();
        }

        auto const flag = value->asBool();
        if (!flag) {
            return foundation::Status::err(mts::agent::toFoundation(flag.status()).error());
        }

        target = flag.value();

        return foundation::Status::ok();
    }

    [[nodiscard]] auto readInt64(std::string_view key, std::int64_t& target) const -> foundation::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return foundation::Status::ok();
        }

        auto const number = value->asInt64();
        if (!number) {
            return foundation::Status::err(mts::agent::toFoundation(number.status()).error());
        }

        target = number.value();

        return foundation::Status::ok();
    }

    [[nodiscard]] auto readUInt32(std::string_view key, std::uint32_t& target) const -> foundation::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return foundation::Status::ok();
        }

        auto const number = value->asInt64();
        if (!number) {
            return foundation::Status::err(mts::agent::toFoundation(number.status()).error());
        }

        std::int64_t const raw = number.value();
        if (raw < 0 || static_cast<std::uint64_t>(raw) > std::numeric_limits<std::uint32_t>::max()) {
            return foundation::Status::err(ConfigErrorKind::OutOfRange, std::string(key) + " is out of range");
        }

        target = static_cast<std::uint32_t>(raw);

        return foundation::Status::ok();
    }

    [[nodiscard]] auto readUInt64(std::string_view key, std::uint64_t& target) const -> foundation::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return foundation::Status::ok();
        }

        auto const number = value->asInt64();
        if (!number) {
            return foundation::Status::err(mts::agent::toFoundation(number.status()).error());
        }

        std::int64_t const raw = number.value();
        if (raw < 0) {
            return foundation::Status::err(ConfigErrorKind::OutOfRange, std::string(key) + " is out of range");
        }

        target = static_cast<std::uint64_t>(raw);

        return foundation::Status::ok();
    }

    [[nodiscard]] auto readDouble(std::string_view key, double& target) const -> foundation::Status {
        auto const value = this->entry(key);
        if (!value.has_value()) {
            return foundation::Status::ok();
        }

        auto const number = value->asDouble();
        if (!number) {
            return foundation::Status::err(mts::agent::toFoundation(number.status()).error());
        }

        target = number.value();

        return foundation::Status::ok();
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

class Config {
public:
    Config() = default;
    explicit Config(mts::agent::Config config)
        : m_config(std::move(config)) {}

    [[nodiscard]] static auto fromJson(std::string_view jsonText) -> foundation::Result<Config> {
        auto const parsed = mts::agent::ConfigLoader::fromJsonString(jsonText);
        if (!parsed) {
            return foundation::Result<Config>::err(mts::agent::toFoundation(parsed.status()).error());
        }

        return Config{parsed.value()};
    }

    [[nodiscard]] static auto fromFile(std::filesystem::path const& path) -> foundation::Result<Config> {
        auto const parsed = mts::agent::ConfigLoader::fromJsonFile(path);
        if (!parsed) {
            return foundation::Result<Config>::err(mts::agent::toFoundation(parsed.status()).error());
        }

        return Config{parsed.value()};
    }

    auto merge(Config const& other) -> foundation::Status {
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
