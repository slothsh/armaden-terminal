export module provider;

import std;
import agent;
import config;

export namespace mts::model {

using Message = mts::agent::BaseMessage;
using Chunk = mts::agent::AIMessageChunk;
using ToolBinding = mts::agent::ChatModelToolBinding;
using ChunkHandler = std::function<void(Chunk const&)>;
using Status = mts::agent::Status;

template <class Type>
using Result = mts::agent::Result<Type>;

template <class Provider>
concept ModelProvider = requires(
    Provider& provider,
    std::vector<Message> const& messages,
    ChunkHandler handler,
    ToolBinding const& binding,
    std::stop_token token
) {
    { provider.invoke(messages) } -> std::same_as<Result<Message>>;
    { provider.stream(messages, handler, token) } -> std::same_as<Result<Message>>;
    { provider.bindTools(binding) } -> std::same_as<Status>;
};

class OpenRouterProvider {
public:
    [[nodiscard]] static auto create(mts::config::ProviderConfig const& config) -> Result<OpenRouterProvider>;

    auto invoke(std::vector<Message> const& messages) -> Result<Message>;
    auto stream(
        std::vector<Message> const& messages,
        ChunkHandler handler,
        std::stop_token token
    ) -> Result<Message>;
    auto bindTools(ToolBinding const& binding) -> Status;

private:
    OpenRouterProvider(std::shared_ptr<mts::agent::BaseChatModel> model, std::string modelName);

    std::shared_ptr<mts::agent::BaseChatModel> m_model;
    std::string m_modelName;
};

class FakeModelProvider {
public:
    FakeModelProvider();
    explicit FakeModelProvider(std::vector<Message> responses);

    auto invoke(std::vector<Message> const& messages) -> Result<Message>;
    auto stream(
        std::vector<Message> const& messages,
        ChunkHandler handler,
        std::stop_token token
    ) -> Result<Message>;
    auto bindTools(ToolBinding const& binding) -> Status;

private:
    std::shared_ptr<mts::agent::BaseChatModel> m_model;
};

class ProviderHandle {
public:
    explicit ProviderHandle(OpenRouterProvider provider);
    explicit ProviderHandle(FakeModelProvider provider);

    auto invoke(std::vector<Message> const& messages) -> Result<Message>;
    auto stream(
        std::vector<Message> const& messages,
        ChunkHandler handler,
        std::stop_token token
    ) -> Result<Message>;
    auto bindTools(ToolBinding const& binding) -> Status;

private:
    std::variant<OpenRouterProvider, FakeModelProvider> m_provider;
};

[[nodiscard]] auto makeOpenRouterProvider(mts::config::ProviderConfig const& config) -> Result<OpenRouterProvider>;
[[nodiscard]] auto makeProvider(mts::config::AppConfig const& config) -> Result<ProviderHandle>;
[[nodiscard]] auto makeVisionProvider(mts::config::AppConfig const& config) -> Result<ProviderHandle>;

} // END OF NAMESPACE `mts::model`

namespace mts::model::detail {

template <class Provider>
auto streamWithCancellation(
    Provider& provider,
    std::vector<Message> const& messages,
    ChunkHandler handler,
    std::stop_token token
) -> Result<Message> {
    auto const wrapped = [&handler, token](Chunk const& chunk) -> Status {
        if (token.stop_requested()) {
            return Status::cancelled();
        }

        handler(chunk);

        return Status::ok();
    };

    return provider.stream(messages, wrapped);
}

} // END OF NAMESPACE `mts::model::detail`

export namespace mts::model {

OpenRouterProvider::OpenRouterProvider(std::shared_ptr<mts::agent::BaseChatModel> model, std::string modelName)
    : m_model(std::move(model))
    , m_modelName(std::move(modelName)) {}

auto OpenRouterProvider::invoke(std::vector<Message> const& messages) -> Result<Message> {
    return m_model->invoke(messages);
}

auto OpenRouterProvider::stream(
    std::vector<Message> const& messages,
    ChunkHandler handler,
    std::stop_token token
) -> Result<Message> {
    return detail::streamWithCancellation(*m_model, messages, std::move(handler), token);
}

auto OpenRouterProvider::bindTools(ToolBinding const& binding) -> Status {
    auto const bound = m_model->bindTools(binding);
    if (!bound) {
        return bound.status();
    }

    m_model = bound.value();

    return Status::ok();
}

auto OpenRouterProvider::create(mts::config::ProviderConfig const& config) -> Result<OpenRouterProvider> {
    auto const clientConfig = mts::agent::HttpClientConfig::fromOrigin(config.origin);
    if (!clientConfig) {
        return clientConfig.status();
    }

    auto const factory = mts::agent::defaultHttpClientFactory();
    if (!factory) {
        return Status::internal("http client factory unavailable");
    }

    auto const client = factory->create(clientConfig.value(), nullptr);
    if (!client) {
        return client.status();
    }

    auto options = mts::agent::ProviderChatModelOptions::openAICompatible(client.value(), config.model, config.apiKey);
    options.path_ = config.path;
    options.extraRequestFields_ = mts::agent::json::object();
    if (!config.referer.empty()) {
        options.headers_["HTTP-Referer"] = config.referer;
    }

    if (!config.title.empty()) {
        options.headers_["X-Title"] = config.title;
    }

    auto model = std::make_shared<mts::agent::ProviderChatModel>(std::move(options));

    return OpenRouterProvider(std::move(model), config.model);
}

FakeModelProvider::FakeModelProvider()
    : m_model(std::make_shared<mts::agent::FakeChatModel>()) {}

FakeModelProvider::FakeModelProvider(std::vector<Message> responses)
    : m_model(std::make_shared<mts::agent::FakeChatModel>(std::move(responses))) {}

auto FakeModelProvider::invoke(std::vector<Message> const& messages) -> Result<Message> {
    return m_model->invoke(messages);
}

auto FakeModelProvider::stream(
    std::vector<Message> const& messages,
    ChunkHandler handler,
    std::stop_token token
) -> Result<Message> {
    return detail::streamWithCancellation(*m_model, messages, std::move(handler), token);
}

auto FakeModelProvider::bindTools(ToolBinding const&) -> Status {
    return Status::ok();
}

ProviderHandle::ProviderHandle(OpenRouterProvider provider)
    : m_provider(std::move(provider)) {}

ProviderHandle::ProviderHandle(FakeModelProvider provider)
    : m_provider(std::move(provider)) {}

auto ProviderHandle::invoke(std::vector<Message> const& messages) -> Result<Message> {
    return std::visit([&messages](auto& provider) {
        return provider.invoke(messages);
    }, m_provider);
}

auto ProviderHandle::stream(
    std::vector<Message> const& messages,
    ChunkHandler handler,
    std::stop_token token
) -> Result<Message> {
    return std::visit([&messages, &handler, token](auto& provider) {
        return provider.stream(messages, handler, token);
    }, m_provider);
}

auto ProviderHandle::bindTools(ToolBinding const& binding) -> Status {
    return std::visit([&binding](auto& provider) {
        return provider.bindTools(binding);
    }, m_provider);
}

auto makeOpenRouterProvider(mts::config::ProviderConfig const& config) -> Result<OpenRouterProvider> {
    return OpenRouterProvider::create(config);
}

auto makeProvider(mts::config::AppConfig const& config) -> Result<ProviderHandle> {
    if (!config.provider.model.empty() && !config.provider.apiKey.empty()) {
        auto provider = makeOpenRouterProvider(config.provider);
        if (!provider) {
            return provider.status();
        }

        return ProviderHandle(std::move(provider.value()));
    }

    return ProviderHandle(FakeModelProvider{});
}

auto makeVisionProvider(mts::config::AppConfig const& config) -> Result<ProviderHandle> {
    if (config.vision.model.empty()) {
        return mts::agent::Result<ProviderHandle>::fromStatus(
            mts::agent::Status::invalidArgument(
                "vision model is not configured; set vision.model in the config to enable map image description"));
    }

    if (config.provider.apiKey.empty()) {
        return mts::agent::Result<ProviderHandle>::fromStatus(
            mts::agent::Status::invalidArgument(
                "provider.apiKey is not configured; the vision model cannot be used without credentials"));
    }

    auto providerConfig = config.provider;
    providerConfig.model = config.vision.model;
    auto provider = makeOpenRouterProvider(providerConfig);
    if (!provider) {
        return provider.status();
    }

    return ProviderHandle(std::move(provider.value()));
}

} // END OF NAMESPACE `mts::model`
