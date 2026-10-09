export module foundation;

import std;
import magic_enum;

export namespace mts::foundation {

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;
using isize = std::ptrdiff_t;

template <typename T>
using SharedPtr = std::shared_ptr<T>;

template <typename T>
using UniquePtr = std::unique_ptr<T>;

template <typename T>
using WeakPtr = std::weak_ptr<T>;

template <typename Kind>
concept ErrorKind = std::is_enum_v<Kind> && requires(Kind kind) {
    { magic_enum::enum_name(kind) } -> std::convertible_to<std::string_view>;
    { magic_enum::enum_integer(kind) } -> std::convertible_to<i64>;
};

template <typename T>
concept UserError = requires(T const& value, std::ostream& stream) {
    requires ErrorKind<std::remove_cvref_t<decltype(value.kind())>>;
    { value.message() } -> std::convertible_to<std::string_view>;
    { stream << value } -> std::same_as<std::ostream&>;
};

struct ErrorContext {
    std::string key;
    std::string value;
};

class Error {
public:
    template <ErrorKind Kind>
    Error(Kind kind, std::string message);

    template <typename T>
    requires UserError<T>
    explicit Error(T value);

    [[nodiscard]] auto kindName() const noexcept -> std::string_view {
        return this->m_kindName;
    }

    [[nodiscard]] auto kindValue() const noexcept -> i64 {
        return this->m_kindValue;
    }

    template <ErrorKind Kind>
    [[nodiscard]] auto kind() const noexcept -> Kind {
        return static_cast<Kind>(this->m_kindValue);
    }

    [[nodiscard]] auto message() const noexcept -> std::string_view {
        return this->m_payload->message();
    }

    [[nodiscard]] auto context() const noexcept -> std::span<ErrorContext const> {
        return this->m_context;
    }

    [[nodiscard]] auto format() const -> std::string;

    [[nodiscard]] auto with(std::string key, std::string value) const -> Error {
        Error copy = *this;
        copy.m_context.push_back(ErrorContext{std::move(key), std::move(value)});

        return copy;
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        [[nodiscard]] virtual auto message() const noexcept -> std::string_view = 0;
        virtual auto write(std::ostream& stream) const -> void = 0;
        [[nodiscard]] virtual auto clone() const -> SharedPtr<Concept const> = 0;
    };

    struct MessageModel final : Concept {
        std::string messageValue;

        explicit MessageModel(std::string message)
            : messageValue(std::move(message)) {}

        [[nodiscard]] auto message() const noexcept -> std::string_view override {
            return this->messageValue;
        }

        auto write(std::ostream& stream) const -> void override {
            stream << this->messageValue;
        }

        [[nodiscard]] auto clone() const -> SharedPtr<Concept const> override {
            return std::make_shared<MessageModel>(this->messageValue);
        }
    };

    template <typename T>
    struct Model final : Concept {
        T value;

        explicit Model(T input)
            : value(std::move(input)) {}

        [[nodiscard]] auto message() const noexcept -> std::string_view override {
            return this->value.message();
        }

        auto write(std::ostream& stream) const -> void override {
            stream << this->value;
        }

        [[nodiscard]] auto clone() const -> SharedPtr<Concept const> override {
            return std::make_shared<Model<T>>(this->value);
        }
    };

    std::string_view m_kindName;
    i64 m_kindValue = 0;
    SharedPtr<Concept const> m_payload;
    std::vector<ErrorContext> m_context;
};

template <ErrorKind Kind>
Error::Error(Kind kind, std::string message)
    : m_kindName(magic_enum::enum_name(kind))
    , m_kindValue(static_cast<i64>(magic_enum::enum_integer(kind)))
    , m_payload(std::make_shared<MessageModel>(std::move(message))) {}

template <typename T>
requires UserError<T>
Error::Error(T value)
    : m_kindName(magic_enum::enum_name(value.kind()))
    , m_kindValue(static_cast<i64>(magic_enum::enum_integer(value.kind())))
    , m_payload(std::make_shared<Model<T>>(std::move(value))) {}

auto Error::format() const -> std::string {
    std::ostringstream stream;
    if (this->m_kindName.empty()) {
        stream << "unknown";
    } else {
        stream << this->m_kindName;
    }
    stream << ": ";
    this->m_payload->write(stream);

    if (!this->m_context.empty()) {
        stream << " (";
        for (usize index = 0; index < this->m_context.size(); ++index) {
            if (index > 0) {
                stream << ", ";
            }

            stream << this->m_context[index].key << "=" << this->m_context[index].value;
        }
        stream << ")";
    }

    return stream.str();
}

[[nodiscard]] auto to_string(Error const& error) -> std::string {
    return error.format();
}

inline auto operator<<(std::ostream& stream, Error const& error) -> std::ostream& {
    return stream << error.format();
}

template <typename T>
class Result {
public:
    using value_type = T;
    using error_type = Error;

    Result(T value)
        : m_value(std::move(value)) {}

    Result(Error error)
        : m_value(std::unexpected(std::move(error))) {}

    [[nodiscard]] static auto ok(T value) -> Result {
        return Result(std::move(value));
    }

    [[nodiscard]] static auto err(Error error) -> Result {
        return Result(std::move(error));
    }

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return this->m_value.has_value();
    }

    [[nodiscard]] auto isOk() const noexcept -> bool {
        return this->m_value.has_value();
    }

    [[nodiscard]] auto isErr() const noexcept -> bool {
        return !this->m_value.has_value();
    }

    [[nodiscard]] auto error() const -> Error const& {
        return this->m_value.error();
    }

    [[nodiscard]] auto value() & -> T& {
        return this->m_value.value();
    }

    [[nodiscard]] auto value() const& -> T const& {
        return this->m_value.value();
    }

    [[nodiscard]] auto value() && -> T&& {
        return std::move(this->m_value.value());
    }

    [[nodiscard]] auto operator*() & -> T& {
        return this->m_value.value();
    }

    [[nodiscard]] auto operator*() const& -> T const& {
        return this->m_value.value();
    }

    [[nodiscard]] auto operator*() && -> T&& {
        return std::move(this->m_value.value());
    }

    [[nodiscard]] auto operator->() -> T* {
        return std::addressof(this->m_value.value());
    }

    [[nodiscard]] auto operator->() const -> T const* {
        return std::addressof(this->m_value.value());
    }

    [[nodiscard]] auto expected() & -> std::expected<T, Error>& {
        return this->m_value;
    }

    [[nodiscard]] auto expected() const& -> std::expected<T, Error> const& {
        return this->m_value;
    }

    [[nodiscard]] auto expected() && -> std::expected<T, Error>&& {
        return std::move(this->m_value);
    }

private:
    std::expected<T, Error> m_value;
};

template <>
class Result<void> {
public:
    using value_type = void;
    using error_type = Error;

    Result() = default;

    Result(Error error)
        : m_value(std::unexpected(std::move(error))) {}

    [[nodiscard]] static auto ok() -> Result {
        return Result{};
    }

    [[nodiscard]] static auto err(Error error) -> Result {
        return Result(std::move(error));
    }

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return this->m_value.has_value();
    }

    [[nodiscard]] auto isOk() const noexcept -> bool {
        return this->m_value.has_value();
    }

    [[nodiscard]] auto isErr() const noexcept -> bool {
        return !this->m_value.has_value();
    }

    [[nodiscard]] auto error() const -> Error const& {
        return this->m_value.error();
    }

    auto value() const -> void {
        this->m_value.value();
    }

    [[nodiscard]] auto expected() & -> std::expected<void, Error>& {
        return this->m_value;
    }

    [[nodiscard]] auto expected() const& -> std::expected<void, Error> const& {
        return this->m_value;
    }

private:
    std::expected<void, Error> m_value{};
};

using Status = Result<void>;

} // END OF NAMESPACE `mts::foundation`

export namespace std {

template <>
struct formatter<mts::foundation::Error> {
    constexpr auto parse(std::format_parse_context& context) -> std::format_parse_context::iterator {
        return context.begin();
    }

    auto format(mts::foundation::Error const& error, std::format_context& context) const -> std::format_context::iterator {
        return std::format_to(context.out(), "{}", error.format());
    }
};

} // END OF NAMESPACE `std`
