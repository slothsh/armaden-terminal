#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <spdlog/formatter.h>

export module logging;

import std;
import magic_enum;

class LogFormatter final : public spdlog::formatter {
public:
    ~LogFormatter() = default;

    auto format(spdlog::details::log_msg const& message, spdlog::memory_buf_t &destination) -> void override {
        auto const local_time = std::chrono::zoned_time{
            std::chrono::current_zone(),
            std::chrono::floor<std::chrono::seconds>(message.time)
        };
        auto const milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.time.time_since_epoch()
        ) % std::chrono::seconds{1};

        auto source_file = message.source.filename != nullptr
            ? std::string_view{message.source.filename}
            : std::string_view{"unknown"};
        if (auto const separator = source_file.find_last_of("/\\\\"); separator != std::string_view::npos) {
            source_file.remove_prefix(separator + 1);
        }

        auto payload = std::string_view{message.payload.data(), message.payload.size()};
        auto module_scope = std::string_view{"unknown"};
        if (payload.starts_with('[')) {
            if (auto const separator = payload.find("] - "); separator != std::string_view::npos) {
                module_scope = payload.substr(1, separator - 1);
                payload.remove_prefix(separator + 4);
            }
        }

        auto const level = spdlog::level::to_string_view(message.level);
        auto const formatted = std::format(
            "{:%Y-%m-%d %H:%M:%S}.{:03} {:%Z} [{}][{}:{}][{}] {}\n",
            local_time,
            milliseconds.count(),
            local_time,
            std::string_view{level.data(), level.size()},
            source_file,
            message.source.line,
            module_scope,
            payload
        );

        destination.append(formatted.data(), formatted.data() + formatted.size());
    }

    auto clone() const -> std::unique_ptr<spdlog::formatter> override {
        return std::make_unique<LogFormatter>();
    }

};

export auto initialize_logging() -> void {
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_formatter(std::make_unique<LogFormatter>());
}
