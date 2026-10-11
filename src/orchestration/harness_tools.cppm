export module harness:tools;

import std;
import agent;
import core;
import config;
import :config;
import events;
import persistence;
import provider;
import runtime;

export namespace mts::harness {

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

} // END OF NAMESPACE `mts::harness`

namespace mts::harness::detail {

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

    GameGraphqlClient(std::shared_ptr<mts::agent::IHttpClient> client, mts::harness::ServerConfig config)
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
                "game server returned HTTP " + std::to_string(response.value().statusCode_)
            );
        }

        auto const parsed = mts::agent::json::parse(response.value().body_, nullptr, false);
        if (!parsed.is_discarded()) {
            return parsed;
        }

        return mts::agent::json(response.value().body_);
    }

    std::shared_ptr<mts::agent::IHttpClient> m_client;
    mts::harness::ServerConfig m_config;
};

inline auto makeGameGraphqlClient(mts::harness::ServerConfig const& config) -> GameGraphqlClient {
    auto const clientConfig = mts::agent::HttpClientConfig::fromOrigin(config.origin);
    if (!clientConfig) {
        return {};
    }

    auto const factory = mts::agent::defaultHttpClientFactory();
    if (!factory) {
        return {};
    }

    std::shared_ptr<mts::agent::IAuthorizationProvider> authorization;
    if (!config.bearerToken.empty()) {
        authorization = std::make_shared<mts::agent::BearerTokenAuthorizationProvider>(config.bearerToken);
    }

    auto const client = factory->create(clientConfig.value(), std::move(authorization));
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
                mts::agent::Status::unavailable("map information is not available")
            );
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
                mts::agent::Status::invalidArgument("describe_map requires a string 'instruction'")
            );
        }

        std::string const instruction = input.at("instruction").get<std::string>();
        if (instruction.empty()) {
            return mts::agent::Result<mts::agent::json>::fromStatus(
                mts::agent::Status::invalidArgument("describe_map 'instruction' must not be empty")
            );
        }

        std::optional<MapToolRegion> region;
        bool const hasAnyCoordinate = input.contains("x") || input.contains("y")
            || input.contains("width") || input.contains("height");
        if (hasAnyCoordinate) {
            if (!(input.contains("x") && input.contains("y")
                  && input.contains("width") && input.contains("height"))) {
                return mts::agent::Result<mts::agent::json>::fromStatus(
                    mts::agent::Status::invalidArgument(
                        "describe_map requires all of x, y, width and height, or none of them for the whole map"
                    )
                );
            }

            for (char const* key : {"x", "y", "width", "height"}) {
                if (!input.at(key).is_number()) {
                    return mts::agent::Result<mts::agent::json>::fromStatus(
                        mts::agent::Status::invalidArgument(std::string("describe_map '") + key + "' must be a number")
                    );
                }
            }

            MapToolRegion candidate;
            candidate.x = input.at("x").get<double>();
            candidate.y = input.at("y").get<double>();
            candidate.width = input.at("width").get<double>();
            candidate.height = input.at("height").get<double>();
            if (!(candidate.width > 0.0) || !(candidate.height > 0.0)) {
                return mts::agent::Result<mts::agent::json>::fromStatus(
                    mts::agent::Status::invalidArgument("describe_map width and height must be positive")
                );
            }

            region = candidate;
        }

        std::optional<int> level;
        if (input.contains("level")) {
            if (!input.at("level").is_number_integer()) {
                return mts::agent::Result<mts::agent::json>::fromStatus(
                    mts::agent::Status::invalidArgument("describe_map 'level' must be an integer")
                );
            }

            level = input.at("level").get<int>();
        }

        std::vector<std::string> layers;
        if (input.contains("layers")) {
            if (!input.at("layers").is_array()) {
                return mts::agent::Result<mts::agent::json>::fromStatus(
                    mts::agent::Status::invalidArgument("describe_map 'layers' must be an array of layer ids")
                );
            }

            for (auto const& value : input.at("layers")) {
                if (!value.is_string()) {
                    return mts::agent::Result<mts::agent::json>::fromStatus(
                        mts::agent::Status::invalidArgument("describe_map 'layers' must contain only strings")
                    );
                }

                layers.push_back(value.get<std::string>());
            }
        }

        if (visionProvider == nullptr) {
            return mts::agent::Result<mts::agent::json>::fromStatus(
                mts::agent::Status::invalidArgument(
                    "vision model is not configured; set vision.model in the config to enable map image description"
                )
            );
        }

        if (!tools.snapshot) {
            return mts::agent::Result<mts::agent::json>::fromStatus(
                mts::agent::Status::unavailable("map imagery is not available")
            );
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
            stopSource.get_token()
        );
        timer.request_stop();
        if (!response) {
            return mts::agent::fromFoundation(response.error());
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

} // END OF NAMESPACE `mts::harness`
