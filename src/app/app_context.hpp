#pragma once

#include <string>

namespace mts::config {
struct AppConfig;
}

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

class MapController;

namespace mts::app {

struct AppContext {
    mts::runtime::ThreadPool& pool;
    mts::events::EventBus& bus;
    mts::persistence::SessionStoreHandle* store = nullptr;
    mts::model::ProviderHandle* provider = nullptr;
    mts::model::ProviderHandle* namingProvider = nullptr;
    mts::model::ProviderHandle* visionProvider = nullptr;
    mts::harness::AgentHarness* harness = nullptr;
    MapController& mapController;
    mts::orchestration::ConversationService& conversation;
    std::string initialStatus;
};

} // END OF NAMESPACE `mts::app`
