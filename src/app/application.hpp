#pragma once

#include <memory>
#include <string>

class ChatController;
class MapController;
class InteractionController;
class MarkdownImageService;
class QGuiApplication;
class QQmlApplicationEngine;

namespace mts::events {
class EventBus;
}

namespace mts::harness {
class AgentHarness;
}

namespace mts::model {
class ProviderHandle;
}

namespace mts::orchestration {
class ConversationService;
}

namespace mts::persistence {
class SessionStoreHandle;
}

namespace mts::runtime {
class ThreadPool;
}

namespace mts::app {

class Application {
public:
    Application(int argc, char** argv);
    ~Application();

    Application(Application const&) = delete;
    auto operator=(Application const&) -> Application& = delete;

    auto run() -> int;

private:
    int m_argc;
    char** m_argv;
    std::unique_ptr<QGuiApplication> m_application;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    std::unique_ptr<MarkdownImageService> m_markdownImageService;
    std::unique_ptr<mts::runtime::ThreadPool> m_pool;
    std::unique_ptr<mts::events::EventBus> m_bus;
    std::unique_ptr<mts::persistence::SessionStoreHandle> m_store;
    std::unique_ptr<mts::model::ProviderHandle> m_provider;
    std::unique_ptr<mts::model::ProviderHandle> m_namingProvider;
    std::unique_ptr<mts::model::ProviderHandle> m_visionProvider;
    std::unique_ptr<MapController> m_mapController;
    std::unique_ptr<mts::harness::AgentHarness> m_harness;
    std::unique_ptr<mts::orchestration::ConversationService> m_conversationService;
    ChatController* m_chat = nullptr;
    InteractionController* m_interaction = nullptr;
    std::string m_initialStatus;
};

} // END OF NAMESPACE `mts::app`
