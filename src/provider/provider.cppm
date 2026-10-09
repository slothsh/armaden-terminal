export module provider;

import std;
import agent;
import core;
import config;

export namespace mts::model {

using Message = mts::agent::BaseMessage;
using Chunk = mts::agent::AIMessageChunk;
using ToolBinding = mts::agent::ChatModelToolBinding;
using ChunkHandler = std::function<void(Chunk const&)>;
using Status = mts::core::Status;

template <class Type>
using Result = mts::core::Result<Type>;

struct ProviderConfig {
    std::string origin = "https://openrouter.ai";
    std::string path = "/api/v1/chat/completions";
    std::string model;
    std::string apiKey;
    std::string referer;
    std::string title;
};

struct VisionConfig {
    std::string model;
    std::uint32_t maxPixels = 1024;
    std::uint32_t timeoutMs = 30000;
};

[[nodiscard]] auto applyConfig(mts::config::ConfigView const& view, ProviderConfig& target) -> Status;
[[nodiscard]] auto applyConfig(mts::config::ConfigView const& view, VisionConfig& target) -> Status;

enum class ProviderErrorKind : core::u8 {
    Unknown,
    InvalidArgument,
    NotFound,
    Unavailable,
    Internal,
    Cancelled,
    ResourceExhausted,
    OutOfRange,
    DataLoss,
    PermissionDenied,
};

static_assert(core::ErrorKind<ProviderErrorKind>);

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
    [[nodiscard]] static auto create(ProviderConfig const& config) -> Result<OpenRouterProvider>;

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

static_assert(ModelProvider<OpenRouterProvider>);
static_assert(ModelProvider<FakeModelProvider>);
static_assert(ModelProvider<ProviderHandle>);

[[nodiscard]] auto makeOpenRouterProvider(ProviderConfig const& config) -> Result<OpenRouterProvider>;
[[nodiscard]] auto makeProvider(ProviderConfig const& config) -> Result<ProviderHandle>;
[[nodiscard]] auto makeVisionProvider(ProviderConfig const& provider, VisionConfig const& vision) -> Result<ProviderHandle>;

} // END OF NAMESPACE `mts::model`

namespace mts::model::detail {

template <class Provider>
auto streamWithCancellation(
    Provider& provider,
    std::vector<Message> const& messages,
    ChunkHandler handler,
    std::stop_token token
) -> Result<Message> {
    auto const wrapped = [&handler, token](Chunk const& chunk) -> mts::agent::Status {
        if (token.stop_requested()) {
            return mts::agent::Status::cancelled();
        }

        handler(chunk);

        return mts::agent::Status::ok();
    };

    return mts::agent::toFoundation(provider.stream(messages, wrapped));
}

} // END OF NAMESPACE `mts::model::detail`

export namespace mts::model {

OpenRouterProvider::OpenRouterProvider(std::shared_ptr<mts::agent::BaseChatModel> model, std::string modelName)
    : m_model(std::move(model))
    , m_modelName(std::move(modelName)) {}

auto OpenRouterProvider::invoke(std::vector<Message> const& messages) -> Result<Message> {
    return mts::agent::toFoundation(m_model->invoke(messages));
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
        return mts::agent::toFoundation(bound.status());
    }

    m_model = bound.value();

    return Status::ok();
}

auto OpenRouterProvider::create(ProviderConfig const& config) -> Result<OpenRouterProvider> {
    auto const clientConfig = mts::agent::HttpClientConfig::fromOrigin(config.origin);
    if (!clientConfig) {
        return Result<OpenRouterProvider>::err(mts::agent::toFoundation(clientConfig.status()).error());
    }

    auto const factory = mts::agent::defaultHttpClientFactory();
    if (!factory) {
        return Result<OpenRouterProvider>::err(ProviderErrorKind::Internal, "http client factory unavailable");
    }

    auto const client = factory->create(clientConfig.value(), nullptr);
    if (!client) {
        return Result<OpenRouterProvider>::err(mts::agent::toFoundation(client.status()).error());
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
    return mts::agent::toFoundation(m_model->invoke(messages));
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

auto applyConfig(mts::config::ConfigView const& view, ProviderConfig& target) -> Status {
    auto const section = view.section("provider");
    if (auto const status = section.readString("origin", target.origin); !status) {
        return status;
    }

    if (auto const status = section.readString("path", target.path); !status) {
        return status;
    }

    if (auto const status = section.readString("model", target.model); !status) {
        return status;
    }

    if (auto const status = section.readString("apiKey", target.apiKey); !status) {
        return status;
    }

    if (auto const status = section.readString("referer", target.referer); !status) {
        return status;
    }

    if (auto const status = section.readString("title", target.title); !status) {
        return status;
    }

    return Status::ok();
}

auto applyConfig(mts::config::ConfigView const& view, VisionConfig& target) -> Status {
    auto const section = view.section("vision");
    if (auto const status = section.readString("model", target.model); !status) {
        return status;
    }

    if (auto const status = section.readUInt32("maxPixels", target.maxPixels); !status) {
        return status;
    }

    if (auto const status = section.readUInt32("timeoutMs", target.timeoutMs); !status) {
        return status;
    }

    return Status::ok();
}

auto makeOpenRouterProvider(ProviderConfig const& config) -> Result<OpenRouterProvider> {
    return OpenRouterProvider::create(config);
}

auto makeProvider(ProviderConfig const& config) -> Result<ProviderHandle> {
    if (!config.model.empty() && !config.apiKey.empty()) {
        auto provider = makeOpenRouterProvider(config);
        if (!provider) {
            return Result<ProviderHandle>::err(provider.error());
        }

        return ProviderHandle(std::move(provider.value()));
    }

    return ProviderHandle(FakeModelProvider{});
}

auto makeVisionProvider(ProviderConfig const& provider, VisionConfig const& vision) -> Result<ProviderHandle> {
    if (vision.model.empty()) {
        return Result<ProviderHandle>::err(
            ProviderErrorKind::InvalidArgument,
            "vision model is not configured; set vision.model in the config to enable map image description"
        );
    }

    if (provider.apiKey.empty()) {
        return Result<ProviderHandle>::err(
            ProviderErrorKind::InvalidArgument,
            "provider.apiKey is not configured; the vision model cannot be used without credentials"
        );
    }

    auto providerConfig = provider;
    providerConfig.model = vision.model;
    auto handle = makeOpenRouterProvider(providerConfig);
    if (!handle) {
        return Result<ProviderHandle>::err(handle.error());
    }

    return ProviderHandle(std::move(handle.value()));
}

} // END OF NAMESPACE `mts::model`
