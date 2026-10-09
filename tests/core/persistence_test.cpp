#include <catch2/catch_test_macros.hpp>

import std;
import persistence;

static_assert(mts::persistence::SessionRepository<mts::persistence::InMemorySessionStore>);
static_assert(mts::persistence::ConversationStore<mts::persistence::InMemorySessionStore>);
static_assert(mts::persistence::SessionRepository<mts::persistence::LanggraphSessionStore>);
static_assert(mts::persistence::ConversationStore<mts::persistence::LanggraphSessionStore>);
static_assert(mts::persistence::SessionRepository<mts::persistence::SessionStoreHandle>);
static_assert(mts::persistence::ConversationStore<mts::persistence::SessionStoreHandle>);

TEST_CASE("in-memory store persists sessions, drafts and messages", "[core][persistence]") {
    mts::persistence::SessionStoreHandle store{mts::persistence::InMemorySessionStore{}};

    auto const session = store.createSession("probe");
    REQUIRE(session.isOk());

    REQUIRE(store.saveDraft(session.value().id, "draft").isOk());
    REQUIRE(store.getSession(session.value().id).value().draft == "draft");

    mts::persistence::Message message;
    message.session = session.value().id;
    message.kind = mts::persistence::MessageKind::UserText;
    message.text = "hi";
    REQUIRE(store.appendMessage(message).isOk());

    auto const messages = store.loadMessages(session.value().id);
    REQUIRE(messages.isOk());
    REQUIRE(messages.value().size() == 1);
    REQUIRE(messages.value().front().text == "hi");
}

TEST_CASE("in-memory store lists, renames and deletes sessions", "[core][persistence]") {
    mts::persistence::SessionStoreHandle store{mts::persistence::InMemorySessionStore{}};

    auto const session = store.createSession("old");
    REQUIRE(session.isOk());
    REQUIRE(store.renameSession(session.value().id, "new").isOk());

    auto const sessions = store.listSessions();
    REQUIRE(sessions.isOk());
    REQUIRE(sessions.value().size() == 1);
    REQUIRE(sessions.value().front().title == "new");

    REQUIRE(store.deleteSession(session.value().id).isOk());
    REQUIRE(store.listSessions().value().empty());
}
