export module harness:naming;

import std;
import persistence;

namespace mts::harness::detail {

inline constexpr std::string_view kNamingSystemPrompt =
    "You name chat sessions. Reply with only a short title of at most six words. "
    "Do not use quotes, markdown, or trailing punctuation.";

inline auto collapseWhitespace(std::string_view text) -> std::string {
    std::string out;
    out.reserve(text.size());
    bool pendingSpace = false;

    for (char const ch : text) {
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            pendingSpace = !out.empty();
            continue;
        }

        if (pendingSpace) {
            out.push_back(' ');
            pendingSpace = false;
        }

        out.push_back(ch);
    }

    return out;
}

inline auto truncateOnWord(std::string text, std::size_t limit) -> std::string {
    if (text.size() <= limit) {
        return text;
    }

    auto cut = text.rfind(' ', limit);
    if (cut == std::string::npos || cut < limit / 2) {
        cut = limit;
    }

    text.resize(cut);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.pop_back();
    }
    text += "\u2026";

    return text;
}

inline auto sanitizeSessionTitle(std::string text) -> std::string {
    auto title = collapseWhitespace(text);
    if (!title.empty() && (title.front() == '"' || title.front() == '\'')) {
        char const quote = title.front();
        title.erase(title.begin());
        if (!title.empty() && title.back() == quote) {
            title.pop_back();
        }
    }

    while (!title.empty()) {
        char const last = title.back();
        if (last == '.' || last == ',' || last == '!' || last == '?' || last == ':' || last == ';' || last == '"' || last == '\'') {
            title.pop_back();
        } else {
            break;
        }
    }

    return truncateOnWord(collapseWhitespace(title), 64);
}

inline auto localSessionTitle(std::string_view prompt) -> std::string {
    return truncateOnWord(collapseWhitespace(prompt), 48);
}

inline auto sessionTranscript(std::vector<mts::persistence::Message> const& messages) -> std::string {
    std::string transcript;

    for (auto const& message : messages) {
        std::string_view role;
        switch (message.kind) {
            case mts::persistence::MessageKind::UserText:
                role = "User";
                break;

            case mts::persistence::MessageKind::AssistantText:
                role = "Assistant";
                break;

            case mts::persistence::MessageKind::Reasoning:
                continue;

            case mts::persistence::MessageKind::ToolCall:
                role = "Tool call";
                break;

            case mts::persistence::MessageKind::ToolResult:
                role = "Tool result";
                break;

            case mts::persistence::MessageKind::Error:
                role = "Error";
                break;
        }

        auto text = collapseWhitespace(message.text);
        if (text.empty()) {
            continue;
        }

        transcript += role;
        transcript += ": ";
        transcript += text;
        transcript += '\n';
        if (transcript.size() > 4000) {
            break;
        }
    }

    return transcript;
}

inline auto namingRequest(std::string_view transcript) -> std::string {
    return std::string{"Conversation:\n"} + std::string{transcript} + "\nWrite a short title for this conversation.";
}

} // END OF NAMESPACE `mts::harness::detail`
