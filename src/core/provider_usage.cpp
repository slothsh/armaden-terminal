import std;
import agent;
import provider;

namespace mts::core {

static_assert(mts::model::ModelProvider<mts::model::OpenRouterProvider>);
static_assert(mts::model::ModelProvider<mts::model::FakeModelProvider>);

auto providerProbe() -> bool {
    auto provider = mts::model::ProviderHandle(mts::model::FakeModelProvider{});
    auto const result = provider.invoke({mts::agent::BaseMessage::human("hi")});

    return result.isOk();
}

} // END OF NAMESPACE `mts::core`
