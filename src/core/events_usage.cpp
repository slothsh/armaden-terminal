import std;
import events;

namespace mts::core {

auto eventBusProbe() -> std::size_t {
    mts::events::EventBus bus;
    std::size_t const id = bus.subscribe<mts::events::RunStarted>([](mts::events::RunStarted const&) {});
    bus.publish(mts::events::RunStarted{.session = "probe"});
    bus.unsubscribe<mts::events::RunStarted>(id);

    return id;
}

} // END OF NAMESPACE `mts::core`
