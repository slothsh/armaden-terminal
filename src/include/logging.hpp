#pragma once

#include <cstddef>
#include <source_location>
#include <type_traits>
#include <utility>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

namespace mts::log {

namespace detail {

template<typename... Args>
struct log_message {
    spdlog::format_string_t<Args...> format;
    std::source_location location;

    template<std::size_t N>
    consteval log_message(
        char const (&format_string)[N],
        std::source_location source = std::source_location::current()
    ) : format(format_string), location(source) {}
};

template<typename M, typename... Args>
static auto emit(
    std::source_location location,
    spdlog::level::level_enum level,
    spdlog::format_string_t<Args...> format,
    Args&&... args
) -> void {
    auto const message = spdlog::fmt_lib::format(format, std::forward<Args>(args)...);

    spdlog::default_logger_raw()->log(
        spdlog::source_loc{
            location.file_name(),
            static_cast<int>(location.line()),
            location.function_name()
        },
        level,
        "[{}::{}] - {}",
        M::LIBRARY_NAME.data(),
        M::MODULE_NAME.data(),
        message
    );
}

} // END OF NAMESPACE `detail`

template<typename Scope = ModuleScope, typename... Args>
static auto info(
    detail::log_message<std::type_identity_t<Args>...> message,
    Args&&... args
) -> void {
    detail::emit<Scope>(
        message.location,
        spdlog::level::level_enum::info,
        message.format,
        std::forward<Args>(args)...
    );
}

template<typename Scope = ModuleScope, typename... Args>
static auto debug(
    detail::log_message<std::type_identity_t<Args>...> message,
    Args&&... args
) -> void {
    detail::emit<Scope>(
        message.location,
        spdlog::level::level_enum::debug,
        message.format,
        std::forward<Args>(args)...
    );
}

template<typename Scope = ModuleScope, typename... Args>
static auto warn(
    detail::log_message<std::type_identity_t<Args>...> message,
    Args&&... args
) -> void {
    detail::emit<Scope>(
        message.location,
        spdlog::level::level_enum::warn,
        message.format,
        std::forward<Args>(args)...
    );
}

template<typename Scope = ModuleScope, typename... Args>
static auto error(
    detail::log_message<std::type_identity_t<Args>...> message,
    Args&&... args
) -> void {
    detail::emit<Scope>(
        message.location,
        spdlog::level::level_enum::err,
        message.format,
        std::forward<Args>(args)...
    );
}

template<typename Scope = ModuleScope, typename... Args>
static auto trace(
    detail::log_message<std::type_identity_t<Args>...> message,
    Args&&... args
) -> void {
    detail::emit<Scope>(
        message.location,
        spdlog::level::level_enum::trace,
        message.format,
        std::forward<Args>(args)...
    );
}

template<typename Scope = ModuleScope, typename... Args>
static auto critical(
    detail::log_message<std::type_identity_t<Args>...> message,
    Args&&... args
) -> void {
    detail::emit<Scope>(
        message.location,
        spdlog::level::level_enum::critical,
        message.format,
        std::forward<Args>(args)...
    );
}

} // END OF NAMESPACE `mts::log`
