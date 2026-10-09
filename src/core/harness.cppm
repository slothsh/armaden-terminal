export module harness;

import std;
import agent;
import foundation;
import config;
import events;
import persistence;
import provider;
import runtime;

export import :tools;
export import :config;
import :messages;
import :naming;

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
        ServerConfig const& server,
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
    auto scheduleSessionNaming(SessionId session) -> void;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // END OF NAMESPACE `mts::harness`

namespace mts::harness {

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
    ServerConfig const& server,
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
        return mts::agent::fromFoundation(status);
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
                return mts::agent::fromFoundation(streamed.error());
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

    auto const status = graph.addConditionalEdges(
        std::string("model"),
        [](mts::agent::State const& state, mts::agent::Runtime&) -> mts::agent::Result<mts::agent::NodeId> {
            if (mts::agent::toolsCondition(state)) {
                return std::string("tools");
            }

            return std::string(mts::agent::END);
        },
        {std::string("tools"), std::string(mts::agent::END)}
    );
    if (!status) {
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
        bus.publish(mts::events::RunFailed{session, std::string(stored.error().message())});
        return mts::agent::fromFoundation(stored.error());
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

    mts::agent::CancellationSource agentCancellation;
    std::stop_callback cancellationBridge(cancellation.token(), [&agentCancellation] {
        agentCancellation.cancel();
    });

    mts::agent::RunOptions options;
    options.threadId_ = session;
    options.metadata_ = mts::agent::json::object();
    options.configurable_ = mts::agent::json::object();
    options.reducers_.set("messages", mts::agent::ReducerKind::AddMessages);
    options.cancellationToken_ = agentCancellation.token();
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
            bus.publish(mts::events::RunFailed{session, std::string(status.error().message())});
            return mts::agent::fromFoundation(status);
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
        return mts::agent::fromFoundation(stored.error());
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
            return mts::agent::fromFoundation(response.error());
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
        this->scheduleSessionNaming(session);
    });
}

auto AgentHarness::scheduleSessionNaming(SessionId session) -> void {
    auto const sessionInfo = m_impl->store->getSession(session);
    if (!sessionInfo || sessionInfo.value().title != mts::persistence::kDefaultSessionTitle) {
        return;
    }

    m_impl->pool->schedule([this, session = std::move(session)] {
        mts::runtime::Cancellation cancellation;
        auto const title = this->suggestSessionTitle(session, cancellation);
        if (!title) {
            return;
        }

        auto const current = m_impl->store->getSession(session);
        if (!current || current.value().title != mts::persistence::kDefaultSessionTitle) {
            return;
        }

        if (auto const status = m_impl->store->renameSession(session, title.value()); !status) {
            return;
        }

        m_impl->bus->publish(mts::events::SessionUpdated{session, title.value()});
    });
}

} // END OF NAMESPACE `mts::harness`
