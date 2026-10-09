#include <catch2/catch_test_macros.hpp>

import std;
import persistence;
import orchestration;

TEST_CASE("conversation service manages session lifecycle without Qt") {
    mts::persistence::SessionStoreHandle store{mts::persistence::InMemorySessionStore{}};
    mts::orchestration::ConversationService service{&store, nullptr};

    auto const id = service.createSession();
    REQUIRE_FALSE(id.empty());
    REQUIRE(service.activeSession() == id);
    REQUIRE(service.draft().empty());

    service.saveDraft("hello");
    REQUIRE(service.draft() == "hello");

    auto const sessions = service.listSessions();
    REQUIRE(sessions.size() == 1);
    REQUIRE(sessions.front().id == id);
    REQUIRE(sessions.front().title == mts::persistence::kDefaultSessionTitle);
    REQUIRE_FALSE(sessions.front().running);

    REQUIRE(service.renameSession(id, "Renamed"));
    auto const renamed = service.listSessions();
    REQUIRE(renamed.front().title == "Renamed");
}

TEST_CASE("conversation service tracks runs and streaming blocks") {
    mts::persistence::SessionStoreHandle store{mts::persistence::InMemorySessionStore{}};
    mts::orchestration::ConversationService service{&store, nullptr};

    auto const id = service.createSession();
    REQUIRE_FALSE(id.empty());

    service.runStarted(id);
    REQUIRE(service.hasInFlight());
    REQUIRE(service.running());
    REQUIRE(service.listSessions().front().running);

    service.tokenDelta(id, "Hi");
    service.tokenDelta(id, " there");

    auto const* pending = service.pendingRun(id);
    REQUIRE(pending != nullptr);
    REQUIRE(pending->blocks.size() == 1);
    REQUIRE(pending->streamingIndex == 0);
    REQUIRE(pending->blocks.front().kind == mts::orchestration::BlockKind::AssistantText);
    REQUIRE(pending->blocks.front().text == "Hi there");

    service.runFinished(id);
    REQUIRE_FALSE(service.hasInFlight());
    REQUIRE_FALSE(service.running());
    REQUIRE(service.pendingRun(id) == nullptr);
}

TEST_CASE("conversation service deletes sessions and picks the next active one") {
    mts::persistence::SessionStoreHandle store{mts::persistence::InMemorySessionStore{}};
    mts::orchestration::ConversationService service{&store, nullptr};

    auto const first = service.createSession();
    auto const second = service.createSession();
    REQUIRE_FALSE(first.empty());
    REQUIRE_FALSE(second.empty());
    REQUIRE(first != second);

    REQUIRE(service.deleteSession(second));
    REQUIRE(service.activeSession() == first);
    REQUIRE(service.listSessions().size() == 1);

    REQUIRE(service.deleteSession(first));
    REQUIRE(service.activeSession().empty());
    REQUIRE(service.listSessions().empty());
}
