export module harness:messages;

import std;
import agent;
import events;
import persistence;

namespace mts::harness::detail {

using SessionId = mts::persistence::SessionId;

using BaseMessage = mts::agent::BaseMessage;

inline auto storedKind(BaseMessage const& message) -> mts::persistence::MessageKind {
    switch (message.type_) {
        case mts::agent::MessageType::Human:
            return mts::persistence::MessageKind::UserText;

        case mts::agent::MessageType::System:
            return mts::persistence::MessageKind::Reasoning;

        case mts::agent::MessageType::Tool:
            return mts::persistence::MessageKind::ToolResult;

        case mts::agent::MessageType::AI:
            return message.toolCalls_.empty()
                ? mts::persistence::MessageKind::AssistantText
                : mts::persistence::MessageKind::ToolCall;
    }

    return mts::persistence::MessageKind::AssistantText;
}

inline auto storedFromBaseMessage(SessionId const& session, BaseMessage const& message) -> mts::persistence::Message {
    mts::persistence::Message stored;
    stored.session = session;
    stored.kind = storedKind(message);
    stored.text = message.content_;
    stored.payload = mts::agent::baseMessageToJson(message).dump();

    return stored;
}

inline auto baseMessageFromStored(mts::persistence::Message const& stored) -> BaseMessage {
    if (!stored.payload.empty()) {
        auto const parsed = mts::agent::json::parse(stored.payload, nullptr, false);
        if (!parsed.is_discarded()) {
            auto const message = mts::agent::baseMessageFromJson(parsed);
            if (message) {
                return message.value();
            }
        }
    }

    switch (stored.kind) {
        case mts::persistence::MessageKind::UserText:
            return BaseMessage::human(stored.text);

        case mts::persistence::MessageKind::Reasoning:
            return BaseMessage::system(stored.text);

        case mts::persistence::MessageKind::ToolResult:
            return BaseMessage::tool({}, {}, stored.text);

        case mts::persistence::MessageKind::AssistantText:
            return BaseMessage::ai(stored.text);

        case mts::persistence::MessageKind::ToolCall:
            return BaseMessage::ai(stored.text);

        case mts::persistence::MessageKind::Error:
            return BaseMessage::system(stored.text);
    }

    return BaseMessage::human(stored.text);
}

inline auto emitMessage(
    mts::events::EventBus& bus,
    SessionId const& session,
    BaseMessage const& message
) -> void {
    switch (message.type_) {
        case mts::agent::MessageType::Human:
            bus.publish(mts::events::MessageAdded{session, mts::events::MessageKind::UserText, message.content_});
            break;

        case mts::agent::MessageType::System:
            bus.publish(mts::events::MessageAdded{session, mts::events::MessageKind::Reasoning, message.content_});
            break;

        case mts::agent::MessageType::Tool:
            bus.publish(mts::events::ToolCallFinished{session, message.toolCallId_, message.name_, message.content_, true});
            break;

        case mts::agent::MessageType::AI:
            if (message.toolCalls_.empty()) {
                bus.publish(mts::events::MessageAdded{session, mts::events::MessageKind::AssistantText, message.content_});
            } else {
                for (auto const& call : message.toolCalls_) {
                    bus.publish(mts::events::ToolCallStarted{session, call.id_, call.name_, call.args_.dump()});
                }
            }
            break;
    }
}

inline auto failureMessage(mts::agent::RunResult const& result) -> std::string {
    for (auto const& event : result.events_) {
        if (event.type_ != mts::agent::RuntimeEventType::RunFailed
            && event.type_ != mts::agent::RuntimeEventType::NodeFailed
            && event.type_ != mts::agent::RuntimeEventType::ToolCallFailed) {
            continue;
        }

        if (!event.message_.empty()) {
            return event.message_;
        }

        return std::string(mts::agent::runtimeEventTypeName(event.type_));
    }

    return "run did not complete";
}

} // END OF NAMESPACE `mts::harness::detail`
