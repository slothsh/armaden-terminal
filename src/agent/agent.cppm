#include <langgraph_cpp/langgraph.hpp>
#include <nlohmann/json.hpp>
#include "foundation/storage/i_storage.hpp"
#include "foundation/storage/sqlite_storage.hpp"
#include "langgraph/checkpoint/saver.hpp"
#include "langgraph/checkpoint/in_memory_saver.hpp"
#include "langgraph/checkpoint/storage_saver.hpp"
#include "langgraph/store/base_store.hpp"
#include "langgraph/store/in_memory_store.hpp"
#include "langgraph/store/storage_store.hpp"
#include "langgraph/graph/compiled_state_graph.hpp"
#include "foundation/network/i_http_client.hpp"
#include "foundation/network/http_client_factory.hpp"
#include "foundation/network/http_client_types.hpp"
#include "foundation/network/i_authorization_provider.hpp"
#include "foundation/config/config_loader.hpp"
#include "foundation/event/i_event_sink.hpp"

export module agent;

import std;
import core;
import magic_enum;

export namespace mts::agent {

using ::nlohmann::json;

using ::lgc::Status;
using ::lgc::StatusCode;
using ::lgc::Result;
using ::lgc::BaseMessage;
using ::lgc::ToolCall;
using ::lgc::AIMessageChunk;
using ::lgc::AIMessageChunkHandler;
using ::lgc::ChatModelTool;
using ::lgc::ChatModelToolBinding;
using ::lgc::messagesToJson;
using ::lgc::messagesFromJson;
using ::lgc::messagesFromStateJson;
using ::lgc::baseMessageToJson;
using ::lgc::baseMessageFromJson;
using ::lgc::MessageType;
using ::lgc::BaseChatModel;
using ::lgc::FakeChatModel;
using ::lgc::ProviderChatModel;
using ::lgc::ProviderChatModelOptions;
using ::lgc::ChatProviderKind;
using ::lgc::CancellationToken;
using ::lgc::CancellationSource;
using ::lgc::BaseCheckpointSaver;
using ::lgc::InMemorySaver;
using ::lgc::StorageSaver;
using ::lgc::BaseStore;
using ::lgc::InMemoryStore;
using ::lgc::StorageStore;
using ::lgc::StateGraph;
using ::lgc::CompiledStateGraph;
using ::lgc::State;
using ::lgc::StateUpdate;
using ::lgc::NodeId;
using ::lgc::NodeHandler;
using ::lgc::RunOptions;
using ::lgc::RunResult;
using ::lgc::RunStatus;
using ::lgc::RuntimeEvent;
using ::lgc::RuntimeEventType;
using ::lgc::runtimeEventTypeName;
using ::lgc::Runtime;
using ::lgc::ReducerKind;
using ::lgc::START;
using ::lgc::END;
using ::lgc::JsonSchema;
using ::lgc::Tool;
using ::lgc::ToolRegistry;
using ::lgc::ToolNode;
using ::lgc::toolsCondition;
using ::lgc::FunctionTool;
using ::lgc::ToolPolicy;
using ::lgc::ToolSpec;
using ::lgc::ToolResult;
using ::lgc::ToolRiskLevel;
using ::lgc::ToolRuntime;
using ::lgc::IHttpClient;
using ::lgc::HttpClientConfig;
using ::lgc::HttpRequest;
using ::lgc::HttpResponse;
using ::lgc::HttpRequestOptions;
using ::lgc::HttpMethod;
using ::lgc::HttpHeaders;
using ::lgc::HttpResult;
using ::lgc::DefaultHttpClientFactory;
using ::lgc::HttpClientFactoryOptions;
using ::lgc::IHttpClientFactory;
using ::lgc::IAuthorizationProvider;
using ::lgc::defaultHttpClientFactory;
using ::lgc::ConfigLoader;
using ::lgc::Config;
using ::lgc::ConfigValue;
using ::lgc::IEventSink;
using ::lgc::IStorage;
using ::lgc::StorageKey;
using ::lgc::StorageItem;
using ::lgc::StoragePutOptions;
using ::lgc::StoragePutMode;
using ::lgc::StorageListOptions;
using ::lgc::StorageListResult;
using ::lgc::StorageLimits;
using ::lgc::SQLiteStorage;
using ::lgc::SQLiteStorageOptions;

enum class AgentErrorKind : core::u8 {
    Unknown,
    Cancelled,
    InvalidArgument,
    DeadlineExceeded,
    NotFound,
    AlreadyExists,
    PermissionDenied,
    ResourceExhausted,
    FailedPrecondition,
    Aborted,
    OutOfRange,
    Unimplemented,
    Internal,
    Unavailable,
    DataLoss,
    Unauthenticated,
};

static_assert(core::ErrorKind<AgentErrorKind>);

[[nodiscard]] constexpr auto agentErrorKind(lgc::StatusCode code) -> AgentErrorKind {
    switch (code) {
        case lgc::StatusCode::Ok:
            return AgentErrorKind::Unknown;

        case lgc::StatusCode::Cancelled:
            return AgentErrorKind::Cancelled;

        case lgc::StatusCode::Unknown:
            return AgentErrorKind::Unknown;

        case lgc::StatusCode::InvalidArgument:
            return AgentErrorKind::InvalidArgument;

        case lgc::StatusCode::DeadlineExceeded:
            return AgentErrorKind::DeadlineExceeded;

        case lgc::StatusCode::NotFound:
            return AgentErrorKind::NotFound;

        case lgc::StatusCode::AlreadyExists:
            return AgentErrorKind::AlreadyExists;

        case lgc::StatusCode::PermissionDenied:
            return AgentErrorKind::PermissionDenied;

        case lgc::StatusCode::ResourceExhausted:
            return AgentErrorKind::ResourceExhausted;

        case lgc::StatusCode::FailedPrecondition:
            return AgentErrorKind::FailedPrecondition;

        case lgc::StatusCode::Aborted:
            return AgentErrorKind::Aborted;

        case lgc::StatusCode::OutOfRange:
            return AgentErrorKind::OutOfRange;

        case lgc::StatusCode::Unimplemented:
            return AgentErrorKind::Unimplemented;

        case lgc::StatusCode::Internal:
            return AgentErrorKind::Internal;

        case lgc::StatusCode::Unavailable:
            return AgentErrorKind::Unavailable;

        case lgc::StatusCode::DataLoss:
            return AgentErrorKind::DataLoss;

        case lgc::StatusCode::Unauthenticated:
            return AgentErrorKind::Unauthenticated;
    }

    return AgentErrorKind::Unknown;
}

[[nodiscard]] constexpr auto toLgcStatusCode(AgentErrorKind kind) -> lgc::StatusCode {
    switch (kind) {
        case AgentErrorKind::Unknown:
            return lgc::StatusCode::Unknown;

        case AgentErrorKind::Cancelled:
            return lgc::StatusCode::Cancelled;

        case AgentErrorKind::InvalidArgument:
            return lgc::StatusCode::InvalidArgument;

        case AgentErrorKind::DeadlineExceeded:
            return lgc::StatusCode::DeadlineExceeded;

        case AgentErrorKind::NotFound:
            return lgc::StatusCode::NotFound;

        case AgentErrorKind::AlreadyExists:
            return lgc::StatusCode::AlreadyExists;

        case AgentErrorKind::PermissionDenied:
            return lgc::StatusCode::PermissionDenied;

        case AgentErrorKind::ResourceExhausted:
            return lgc::StatusCode::ResourceExhausted;

        case AgentErrorKind::FailedPrecondition:
            return lgc::StatusCode::FailedPrecondition;

        case AgentErrorKind::Aborted:
            return lgc::StatusCode::Aborted;

        case AgentErrorKind::OutOfRange:
            return lgc::StatusCode::OutOfRange;

        case AgentErrorKind::Unimplemented:
            return lgc::StatusCode::Unimplemented;

        case AgentErrorKind::Internal:
            return lgc::StatusCode::Internal;

        case AgentErrorKind::Unavailable:
            return lgc::StatusCode::Unavailable;

        case AgentErrorKind::DataLoss:
            return lgc::StatusCode::DataLoss;

        case AgentErrorKind::Unauthenticated:
            return lgc::StatusCode::Unauthenticated;
    }

    return lgc::StatusCode::Unknown;
}

[[nodiscard]] inline auto toFoundation(lgc::Status const& status) -> core::Status {
    if (status.isOk()) {
        return core::Status::ok();
    }

    std::string message(status.message());
    if (message.empty()) {
        message = std::string(status.codeName());
    }

    return core::Status::err(
        core::Error(agentErrorKind(status.code()), std::move(message))
            .with("lgc.status", std::string(status.codeName()))
    );
}

template <typename T>
requires (!std::is_void_v<T>)
[[nodiscard]] inline auto toFoundation(lgc::Result<T> const& result) -> core::Result<T> {
    if (result.isOk()) {
        return core::Result<T>::ok(result.value());
    }

    return core::Result<T>::err(toFoundation(result.status()).error());
}

[[nodiscard]] inline auto toFoundation(lgc::Result<void> const& result) -> core::Status {
    return toFoundation(result.status());
}

[[nodiscard]] inline auto fromFoundation(core::Status const& status) -> lgc::Status {
    if (status.isOk()) {
        return lgc::Status::ok();
    }

    core::Error const& error = status.error();
    auto const kind = magic_enum::enum_cast<AgentErrorKind>(error.kindName());
    lgc::StatusCode const code = kind.has_value() ? toLgcStatusCode(*kind) : lgc::StatusCode::Internal;

    return lgc::Status(code, std::string(error.message()));
}

} // END OF NAMESPACE `mts::agent`
