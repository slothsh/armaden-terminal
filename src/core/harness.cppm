export module harness;

import std;
import agent;
import config;
import events;
import persistence;
import provider;
import runtime;

export namespace mts::harness {

using SessionId = mts::persistence::SessionId;
using Status = mts::agent::Status;

struct MapToolRegion {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct MapToolSnapshot {
    std::string snapshotId;
    int width = 0;
    int height = 0;
    int level = 0;
    std::vector<std::string> layers;
    MapToolRegion region;
    std::string unitLabel;
    std::string pngBase64;
};

struct MapTools {
    std::function<mts::agent::Result<mts::agent::json>()> info;
    std::function<mts::agent::Result<MapToolSnapshot>(
        std::optional<MapToolRegion> const& region,
        std::optional<int> level,
        std::vector<std::string> const& layers
    )> snapshot;
    int visionTimeoutMs = 30000;
};

[[nodiscard]] auto makeGetMapInfoTool(MapTools const& tools) -> mts::agent::Tool;
[[nodiscard]] auto makeDescribeMapTool(
    MapTools const& tools,
    mts::model::ProviderHandle* visionProvider
) -> mts::agent::Tool;

class AgentHarness {
public:
    AgentHarness(
        mts::model::ProviderHandle& provider,
        mts::model::ProviderHandle* namingProvider,
        mts::model::ProviderHandle* visionProvider,
        mts::persistence::SessionStoreHandle& store,
        mts::runtime::ThreadPool& pool,
        mts::events::EventBus& bus,
        mts::config::ServerConfig const& server,
        MapTools mapTools = {}
    );
    ~AgentHarness();

    AgentHarness(AgentHarness const&) = delete;
    auto operator=(AgentHarness const&) -> AgentHarness& = delete;
    AgentHarness(AgentHarness&&) noexcept;
    auto operator=(AgentHarness&&) noexcept -> AgentHarness&;

    auto submitUserMessage(SessionId const& session, std::string text) -> Status;
    auto runBlocking(SessionId const& session, mts::runtime::Cancellation& cancellation) -> Status;
    auto suggestSessionTitle(
        SessionId const& session,
        mts::runtime::Cancellation& cancellation
    ) -> mts::agent::Result<std::string>;
    auto run(SessionId session, std::shared_ptr<mts::runtime::Cancellation> cancellation) -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // END OF NAMESPACE `mts::harness`

namespace mts::harness::detail {

using mts::agent::BaseMessage;

inline auto makeEchoTool() -> mts::agent::Tool {
    mts::agent::Tool tool;
    tool.name_ = "echo";
    tool.description_ = "Echo the provided text back to the caller.";
    tool.inputSchema_ = mts::agent::JsonSchema::object()
                            .property("text", mts::agent::JsonSchema::string(), true)
                            .additionalProperties(false);
    tool.callable_ = [](mts::agent::json const& input) -> mts::agent::Result<mts::agent::json> {
        return mts::agent::json{{"text", input.value("text", std::string{})}};
    };

    return tool;
}

inline constexpr std::string_view kGraphqlSchema =
    "type Query { hello: String! echo(message: String!): String! players: [Player!]! }\n"
    "type Player { id: String! name: String! position: Position! }\n"
    "type Position { x: Float! y: Float! z: Float! }\n"
    "type Mutation { broadcastChat(message: String!): ChatBroadcastResult! smite(playerId: Int!, instigatorId: Int, radiusKm: Float): SmiteResult! }\n"
    "type ChatBroadcastResult { sent: Boolean! message: String! }\n"
    "type SmiteResult { smitten: Boolean! playerId: Int! instigatorId: Int! }";

class GameGraphqlClient {
public:
    GameGraphqlClient() = default;

    GameGraphqlClient(std::shared_ptr<mts::agent::IHttpClient> client, mts::config::ServerConfig config)
        : m_client(std::move(client))
        , m_config(std::move(config)) {}

    auto execute(std::string const& query, mts::agent::json const& variables) const -> mts::agent::Result<mts::agent::json> {
        return this->postJson(m_config.graphqlPath, mts::agent::json{{"query", query}, {"variables", variables}});
    }

    auto fetchSchema() const -> mts::agent::Result<mts::agent::json> {
        return this->postJson(m_config.schemaPath, mts::agent::json::object());
    }

private:
    auto postJson(std::string const& path, mts::agent::json const& body) const -> mts::agent::Result<mts::agent::json> {
        if (m_client == nullptr) {
            return mts::agent::Status::unavailable("game server client is not configured");
        }

        auto request = mts::agent::HttpRequest::post(path, body.dump());

        auto const response = m_client->send(std::move(request), mts::agent::HttpRequestOptions{});
        if (!response) {
            return response.status();
        }

        if (!response.value().isSuccessful()) {
            return mts::agent::Status::internal(
                "game server returned HTTP " + std::to_string(response.value().statusCode_));
        }

        auto const parsed = mts::agent::json::parse(response.value().body_, nullptr, false);
        if (!parsed.is_discarded()) {
            return parsed;
        }

        return mts::agent::json(response.value().body_);
    }

    std::shared_ptr<mts::agent::IHttpClient> m_client;
    mts::config::ServerConfig m_config;
};

inline auto makeGameGraphqlClient(mts::config::ServerConfig const& config) -> GameGraphqlClient {
    auto const clientConfig = mts::agent::HttpClientConfig::fromOrigin(config.origin);
    if (!clientConfig) {
        return {};
    }

    auto const factory = mts::agent::defaultHttpClientFactory();
    if (!factory) {
        return {};
    }

    auto const client = factory->create(clientConfig.value(), nullptr);
    if (!client) {
        return {};
    }

    return GameGraphqlClient(client.value(), config);
}

inline auto makeCommandGraphqlTool(GameGraphqlClient& client) -> mts::agent::Tool {
    mts::agent::Tool tool;
    tool.name_ = "command_graphql";
    tool.description_ =
        "Run a GraphQL operation against the game server. "
        "Write the GraphQL document yourself based on what the user is asking for, "
        "pass it in `query`, and pass any operation variables as a JSON object in `variables`. "
        "Use a query for reads and a mutation for actions. "
        "Call getSchema for the authoritative, current schema; the summary below is only a hint.\n"
        + std::string{kGraphqlSchema};
    tool.inputSchema_ = mts::agent::JsonSchema::object()
                            .property("query", mts::agent::JsonSchema::string(), true)
                            .property("variables", mts::agent::JsonSchema::object(), false)
                            .additionalProperties(false);
    tool.callable_ = [&client](mts::agent::json const& input) -> mts::agent::Result<mts::agent::json> {
        auto const variables = input.contains("variables") && input["variables"].is_object()
            ? input["variables"]
            : mts::agent::json::object();

        return client.execute(input.value("query", std::string{}), variables);
    };

    return tool;
}

inline auto makeGetSchemaTool(GameGraphqlClient& client) -> mts::agent::Tool {
    mts::agent::Tool tool;
    tool.name_ = "getSchema";
    tool.description_ =
        "Fetch the exact, current schema of the game server's GraphQL API. "
        "Call this whenever you are unsure which queries, mutations, fields or arguments exist "
        "before writing a command_graphql operation. Returns the raw schema as JSON context.";
    tool.inputSchema_ = mts::agent::JsonSchema::object().additionalProperties(false);
    tool.callable_ = [&client](mts::agent::json const&) -> mts::agent::Result<mts::agent::json> {
        return client.fetchSchema();
    };

    return tool;
}

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

export namespace mts::harness {

[[nodiscard]] inline auto makeGetMapInfoTool(MapTools const& tools) -> mts::agent::Tool {
    mts::agent::Tool tool;
    tool.name_ = "get_map_info";
    tool.description_ =
        "Return metadata for the configured map: name, world bounds and unit label, meters per pixel, "
        "available detail levels (minLevel/maxLevel), tile size, and the layers (id, name, visibility, opacity, blend). "
        "Call this before describe_map so coordinates and layer ids are correct.";
    tool.inputSchema_ = mts::agent::JsonSchema::object().additionalProperties(false);
    tool.riskLevel_ = mts::agent::ToolRiskLevel::ReadOnly;
    tool.callable_ = [info = tools.info](mts::agent::json const&) -> mts::agent::Result<mts::agent::json> {
        if (!info) {
            return mts::agent::Result<mts::agent::json>::fromStatus(
                mts::agent::Status::unavailable("map information is not available"));
        }

        return info();
    };

    return tool;
}

[[nodiscard]] inline auto makeDescribeMapTool(
    MapTools const& tools,
    mts::model::ProviderHandle* visionProvider
) -> mts::agent::Tool {
    mts::agent::Tool tool;
    tool.name_ = "describe_map";
    tool.description_ =
        "Render a region of the configured map and have a vision model describe or extract information from it. "
        "Coordinates are in the map's configured units - call get_map_info first to learn the unit label, world bounds and layer ids. "
        "Omitting x/y/width/height renders the WHOLE map (downscaled). "
        "Pass a specific instruction describing exactly what to extract, e.g. 'count the yellow markers and list their numbers'. "
        "Returns the model's description plus an image reference (id) that the UI can display.";
    tool.inputSchema_ = mts::agent::JsonSchema::object()
        .property("x", mts::agent::JsonSchema::number(), false)
        .property("y", mts::agent::JsonSchema::number(), false)
        .property("width", mts::agent::JsonSchema::number(), false)
        .property("height", mts::agent::JsonSchema::number(), false)
        .property("level", mts::agent::JsonSchema::integer(), false)
        .property("layers", mts::agent::JsonSchema::array().items(mts::agent::JsonSchema::string()), false)
        .property("instruction", mts::agent::JsonSchema::string(), true)
        .additionalProperties(false);
    tool.riskLevel_ = mts::agent::ToolRiskLevel::ReadOnly;
    tool.callable_ = [tools, visionProvider](mts::agent::json const& input) -> mts::agent::Result<mts::agent::json> {
        if (!input.contains("instruction") || !input.at("instruction").is_string()) {
            return mts::agent::Result<mts::agent::json>::fromStatus(
                mts::agent::Status::invalidArgument("describe_map requires a string 'instruction'"));
        }

        std::string const instruction = input.at("instruction").get<std::string>();
        if (instruction.empty()) {
            return mts::agent::Result<mts::agent::json>::fromStatus(
                mts::agent::Status::invalidArgument("describe_map 'instruction' must not be empty"));
        }

        std::optional<MapToolRegion> region;
        bool const hasAnyCoordinate = input.contains("x") || input.contains("y")
            || input.contains("width") || input.contains("height");
        if (hasAnyCoordinate) {
            if (!(input.contains("x") && input.contains("y")
                  && input.contains("width") && input.contains("height"))) {
                return mts::agent::Result<mts::agent::json>::fromStatus(
                    mts::agent::Status::invalidArgument(
                        "describe_map requires all of x, y, width and height, or none of them for the whole map"));
            }

            for (char const* key : {"x", "y", "width", "height"}) {
                if (!input.at(key).is_number()) {
                    return mts::agent::Result<mts::agent::json>::fromStatus(
                        mts::agent::Status::invalidArgument(std::string("describe_map '") + key + "' must be a number"));
                }
            }

            MapToolRegion candidate;
            candidate.x = input.at("x").get<double>();
            candidate.y = input.at("y").get<double>();
            candidate.width = input.at("width").get<double>();
            candidate.height = input.at("height").get<double>();
            if (!(candidate.width > 0.0) || !(candidate.height > 0.0)) {
                return mts::agent::Result<mts::agent::json>::fromStatus(
                    mts::agent::Status::invalidArgument("describe_map width and height must be positive"));
            }

            region = candidate;
        }

        std::optional<int> level;
        if (input.contains("level")) {
            if (!input.at("level").is_number_integer()) {
                return mts::agent::Result<mts::agent::json>::fromStatus(
                    mts::agent::Status::invalidArgument("describe_map 'level' must be an integer"));
            }

            level = input.at("level").get<int>();
        }

        std::vector<std::string> layers;
        if (input.contains("layers")) {
            if (!input.at("layers").is_array()) {
                return mts::agent::Result<mts::agent::json>::fromStatus(
                    mts::agent::Status::invalidArgument("describe_map 'layers' must be an array of layer ids"));
            }

            for (auto const& value : input.at("layers")) {
                if (!value.is_string()) {
                    return mts::agent::Result<mts::agent::json>::fromStatus(
                        mts::agent::Status::invalidArgument("describe_map 'layers' must contain only strings"));
                }

                layers.push_back(value.get<std::string>());
            }
        }

        if (visionProvider == nullptr) {
            return mts::agent::Result<mts::agent::json>::fromStatus(
                mts::agent::Status::invalidArgument(
                    "vision model is not configured; set vision.model in the config to enable map image description"));
        }

        if (!tools.snapshot) {
            return mts::agent::Result<mts::agent::json>::fromStatus(
                mts::agent::Status::unavailable("map imagery is not available"));
        }

        auto const snapshot = tools.snapshot(region, level, layers);
        if (!snapshot) {
            return snapshot.status();
        }

        mts::agent::json regionJson = mts::agent::json::object();
        if (region.has_value()) {
            regionJson = mts::agent::json{
                {"x", region->x},
                {"y", region->y},
                {"width", region->width},
                {"height", region->height},
                {"unit", snapshot.value().unitLabel},
            };
        }

        std::string prompt = "Map image";
        if (region.has_value()) {
            prompt += " for region x=" + std::to_string(region->x)
                + " y=" + std::to_string(region->y)
                + " width=" + std::to_string(region->width)
                + " height=" + std::to_string(region->height)
                + " (map units, " + snapshot.value().unitLabel + ")";
        } else {
            prompt += " showing the whole map";
        }
        prompt += ". Detail level " + std::to_string(snapshot.value().level) + ".";
        prompt += "\nInstruction: " + instruction;

        auto message = mts::agent::BaseMessage::human(prompt);
        message.contentBlocks_ = mts::agent::json::array({
            mts::agent::json{{"type", "text"}, {"text", prompt}},
            mts::agent::json{{"type", "image"}, {"base64", snapshot.value().pngBase64}, {"mime_type", "image/png"}},
        });

        std::stop_source stopSource;
        std::jthread timer([&stopSource, timeoutMs = tools.visionTimeoutMs](std::stop_token token) {
            auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
            while (std::chrono::steady_clock::now() < deadline) {
                if (token.stop_requested()) {
                    return;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            if (!token.stop_requested()) {
                stopSource.request_stop();
            }
        });

        auto const response = visionProvider->stream(
            std::vector<mts::agent::BaseMessage>{message},
            [](mts::model::Chunk const&) {},
            stopSource.get_token());
        timer.request_stop();
        if (!response) {
            return response.status();
        }

        mts::agent::json result = mts::agent::json::object();
        result["description"] = response.value().content_;

        mts::agent::json image = mts::agent::json::object();
        image["id"] = snapshot.value().snapshotId;
        image["width"] = snapshot.value().width;
        image["height"] = snapshot.value().height;
        image["level"] = snapshot.value().level;
        image["layers"] = snapshot.value().layers;
        image["region"] = regionJson;
        result["image"] = std::move(image);

        return result;
    };

    return tool;
}

struct AgentHarness::Impl {
    mts::model::ProviderHandle* provider = nullptr;
    mts::model::ProviderHandle* namingProvider = nullptr;
    mts::model::ProviderHandle* visionProvider = nullptr;
    mts::persistence::SessionStoreHandle* store = nullptr;
    mts::runtime::ThreadPool* pool = nullptr;
    mts::events::EventBus* bus = nullptr;
    MapTools mapTools;
    std::shared_ptr<mts::agent::BaseCheckpointSaver> checkpointer;
    detail::GameGraphqlClient gameClient;
    std::shared_ptr<mts::agent::ToolRegistry> registry;
};

AgentHarness::AgentHarness(
    mts::model::ProviderHandle& provider,
    mts::model::ProviderHandle* namingProvider,
    mts::model::ProviderHandle* visionProvider,
    mts::persistence::SessionStoreHandle& store,
    mts::runtime::ThreadPool& pool,
    mts::events::EventBus& bus,
    mts::config::ServerConfig const& server,
    MapTools mapTools
)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->provider = &provider;
    m_impl->namingProvider = namingProvider;
    m_impl->visionProvider = visionProvider;
    m_impl->store = &store;
    m_impl->pool = &pool;
    m_impl->bus = &bus;
    m_impl->mapTools = std::move(mapTools);
    m_impl->checkpointer = std::make_shared<mts::agent::InMemorySaver>();

    m_impl->gameClient = detail::makeGameGraphqlClient(server);

    m_impl->registry = std::make_shared<mts::agent::ToolRegistry>();
    static_cast<void>(m_impl->registry->add(detail::makeEchoTool()));
    static_cast<void>(m_impl->registry->add(detail::makeCommandGraphqlTool(m_impl->gameClient)));
    static_cast<void>(m_impl->registry->add(detail::makeGetSchemaTool(m_impl->gameClient)));
    if (m_impl->mapTools.info) {
        static_cast<void>(m_impl->registry->add(makeGetMapInfoTool(m_impl->mapTools)));
    }

    if (m_impl->mapTools.snapshot) {
        static_cast<void>(m_impl->registry->add(makeDescribeMapTool(m_impl->mapTools, m_impl->visionProvider)));
    }

    mts::agent::ChatModelToolBinding binding;
    for (auto const& spec : m_impl->registry->list()) {
        binding.tools_.push_back(mts::agent::ChatModelTool{spec.name_, spec.description_, spec.inputSchema_});
    }
    static_cast<void>(provider.bindTools(binding));
}

AgentHarness::~AgentHarness() = default;

AgentHarness::AgentHarness(AgentHarness&&) noexcept = default;

auto AgentHarness::operator=(AgentHarness&&) noexcept -> AgentHarness& = default;

auto AgentHarness::submitUserMessage(SessionId const& session, std::string text) -> Status {
    mts::persistence::Message message;
    message.session = session;
    message.kind = mts::persistence::MessageKind::UserText;
    message.text = text;

    if (auto const status = m_impl->store->appendMessage(message); !status) {
        return status;
    }

    m_impl->bus->publish(mts::events::MessageAdded{session, mts::events::MessageKind::UserText, text});

    return Status::ok();
}

auto AgentHarness::runBlocking(SessionId const& session, mts::runtime::Cancellation& cancellation) -> Status {
    auto& bus = *m_impl->bus;
    bus.publish(mts::events::RunStarted{session});

    auto const registry = m_impl->registry;

    auto* provider = m_impl->provider;
    mts::agent::NodeHandler const modelNode =
        [provider, &session, &bus, &cancellation](mts::agent::State const& state, mts::agent::Runtime&)
        -> mts::agent::Result<mts::agent::StateUpdate> {
            auto const messages = mts::agent::messagesFromStateJson(state.view());
            if (!messages) {
                return messages.status();
            }

            auto const streamed = provider->stream(
                messages.value(),
                [&bus, &session](mts::model::Chunk const& chunk) {
                    if (!chunk.text_.empty()) {
                        bus.publish(mts::events::TokenDelta{session, chunk.text_});
                    }
                },
                cancellation.token()
            );
            if (!streamed) {
                return streamed.status();
            }

            return mts::agent::StateUpdate::fromJsonValue({
                {"messages", mts::agent::messagesToJson({streamed.value()})},
            });
        };

    mts::agent::StateGraph graph;
    if (auto const status = graph.addNode(std::string("model"), modelNode); !status) {
        return status.status();
    }

    if (auto const status = graph.addNode(std::string("tools"), mts::agent::ToolNode(registry)); !status) {
        return status.status();
    }

    if (auto const status = graph.addEdge(std::string(mts::agent::START), std::string("model")); !status) {
        return status.status();
    }

    if (auto const status = graph.addConditionalEdges(
            std::string("model"),
            [](mts::agent::State const& state, mts::agent::Runtime&) -> mts::agent::Result<mts::agent::NodeId> {
                if (mts::agent::toolsCondition(state)) {
                    return std::string("tools");
                }

                return std::string(mts::agent::END);
            },
            {std::string("tools"), std::string(mts::agent::END)});
        !status) {
        return status.status();
    }

    if (auto const status = graph.addEdge(std::string("tools"), std::string("model")); !status) {
        return status.status();
    }

    auto const compiled = graph.compile();
    if (!compiled) {
        bus.publish(mts::events::RunFailed{session, compiled.status().message()});
        return compiled.status();
    }

    auto const stored = m_impl->store->loadMessages(session);
    if (!stored) {
        bus.publish(mts::events::RunFailed{session, stored.status().message()});
        return stored.status();
    }

    std::vector<mts::agent::BaseMessage> history;
    history.reserve(stored.value().size());
    for (auto const& message : stored.value()) {
        history.push_back(detail::baseMessageFromStored(message));
    }

    auto const input = mts::agent::State::fromJsonValue({
        {"messages", mts::agent::messagesToJson(history)},
    });
    if (!input) {
        bus.publish(mts::events::RunFailed{session, input.status().message()});
        return input.status();
    }

    mts::agent::RunOptions options;
    options.threadId_ = session;
    options.metadata_ = mts::agent::json::object();
    options.configurable_ = mts::agent::json::object();
    options.reducers_.set("messages", mts::agent::ReducerKind::AddMessages);
    options.cancellationToken_ = cancellation.agentToken();
    options.checkpointer_ = m_impl->checkpointer;
    options.collectEvents_ = true;
    options.eventTypes_ = {
        mts::agent::RuntimeEventType::RunFailed,
        mts::agent::RuntimeEventType::NodeFailed,
        mts::agent::RuntimeEventType::ToolCallFailed,
    };

    auto const result = compiled.value().stream(input.value(), options);
    if (!result) {
        bus.publish(mts::events::RunFailed{session, result.status().message()});
        return result.status();
    }

    auto const runStatus = result.value().status_;
    if (runStatus == mts::agent::RunStatus::Cancelled) {
        bus.publish(mts::events::RunFinished{session});
        return Status::cancelled("run cancelled");
    }

    if (runStatus != mts::agent::RunStatus::Completed) {
        std::string const message = detail::failureMessage(result.value());
        bus.publish(mts::events::RunFailed{session, message});
        return Status::internal(message);
    }

    auto const messages = mts::agent::messagesFromStateJson(result.value().state_.view());
    if (!messages) {
        bus.publish(mts::events::RunFailed{session, messages.status().message()});
        return messages.status();
    }

    auto const produced = messages.value();
    for (std::size_t index = history.size(); index < produced.size(); ++index) {
        auto const& message = produced[index];
        if (auto const status = m_impl->store->appendMessage(detail::storedFromBaseMessage(session, message)); !status) {
            bus.publish(mts::events::RunFailed{session, status.message()});
            return status;
        }

        detail::emitMessage(bus, session, message);
    }

    bus.publish(mts::events::RunFinished{session});

    return Status::ok();
}

auto AgentHarness::suggestSessionTitle(
    SessionId const& session,
    mts::runtime::Cancellation& cancellation
) -> mts::agent::Result<std::string> {
    if (cancellation.cancelled()) {
        return Status::cancelled("session naming cancelled");
    }

    auto const stored = m_impl->store->loadMessages(session);
    if (!stored) {
        return stored.status();
    }

    std::string firstPrompt;
    std::size_t userCount = 0;
    for (auto const& message : stored.value()) {
        if (message.kind != mts::persistence::MessageKind::UserText) {
            continue;
        }

        ++userCount;
        if (firstPrompt.empty()) {
            firstPrompt = message.text;
        }
    }

    if (userCount != 1 || firstPrompt.empty()) {
        return Status::invalidArgument("session naming requires a single first prompt");
    }

    if (m_impl->namingProvider != nullptr) {
        auto messages = std::vector<mts::agent::BaseMessage>{
            mts::agent::BaseMessage::system(std::string{detail::kNamingSystemPrompt}),
            mts::agent::BaseMessage::human(detail::namingRequest(detail::sessionTranscript(stored.value()))),
        };
        auto const response = m_impl->namingProvider->stream(
            messages,
            [](mts::model::Chunk const&) {},
            cancellation.token()
        );
        if (!response) {
            return response.status();
        }

        auto title = detail::sanitizeSessionTitle(response.value().content_);
        if (title.empty()) {
            return Status::invalidArgument("naming model returned an empty title");
        }

        return title;
    }

    auto title = detail::localSessionTitle(firstPrompt);
    if (title.empty()) {
        return Status::invalidArgument("could not derive a session title");
    }

    return title;
}

auto AgentHarness::run(SessionId session, std::shared_ptr<mts::runtime::Cancellation> cancellation) -> void {
    m_impl->pool->schedule([this, session = std::move(session), cancellation = std::move(cancellation)] {
        if (cancellation == nullptr) {
            return;
        }

        static_cast<void>(this->runBlocking(session, *cancellation));
    });
}

} // END OF NAMESPACE `mts::harness`
