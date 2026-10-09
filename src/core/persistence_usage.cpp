import std;
import persistence;

namespace mts::core {

static_assert(mts::persistence::SessionRepository<mts::persistence::InMemorySessionStore>);
static_assert(mts::persistence::ConversationStore<mts::persistence::InMemorySessionStore>);
static_assert(mts::persistence::SessionRepository<mts::persistence::LanggraphSessionStore>);
static_assert(mts::persistence::ConversationStore<mts::persistence::LanggraphSessionStore>);

auto persistenceProbe() -> bool {
    mts::persistence::SessionStoreHandle store(mts::persistence::InMemorySessionStore{});

    auto const session = store.createSession("probe");
    if (!session) {
        return false;
    }

    mts::persistence::Message message;
    message.session = session.value().id;
    message.kind = mts::persistence::MessageKind::UserText;
    message.text = "hi";

    if (auto const status = store.appendMessage(message); !status) {
        return false;
    }

    auto const messages = store.loadMessages(session.value().id);

    return messages.isOk() && messages.value().size() == 1;
}

} // END OF NAMESPACE `mts::core`
