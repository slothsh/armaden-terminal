#include <catch2/catch_test_macros.hpp>

import std;
import agent;
import events;
import harness;
import orchestration;
import persistence;
import provider;
import runtime;

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

TEST_CASE("harness publishes streamed and final assistant events in order") {
    constexpr std::string_view response = "The response appears exactly once.";
    mts::runtime::ThreadPool pool{4};
    mts::events::EventBus bus{&pool};
    mts::persistence::SessionStoreHandle store{mts::persistence::InMemorySessionStore{}};
    std::vector<mts::agent::BaseMessage> responses;
    responses.push_back(mts::agent::BaseMessage::ai(
        {},
        {mts::agent::ToolCall{"call-1", "echo", mts::agent::json{{"text", "tool result"}}}}
    ));
    responses.push_back(mts::agent::BaseMessage::ai(std::string(response)));
    mts::model::ProviderHandle provider{mts::model::FakeModelProvider{std::move(responses)}};
    mts::harness::ServerConfig server;
    mts::harness::AgentHarness agentHarness{provider, nullptr, nullptr, store, pool, bus, server};
    auto const created = store.createSession("stream order");
    REQUIRE(created.isOk());
    std::string const session = created.value().id;

    std::mutex eventsMutex;
    std::vector<std::string> eventOrder;
    std::string streamedText;
    int assistantMessageCount = 0;
    [[maybe_unused]] auto const messageSubscription = bus.subscribe<mts::events::MessageAdded>(
        [&](mts::events::MessageAdded const& event) {
            if (event.session != session) {
                return;
            }
            if (event.kind == mts::events::MessageKind::UserText) {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                std::lock_guard const lock(eventsMutex);
                eventOrder.emplace_back("user");
            } else if (event.kind == mts::events::MessageKind::AssistantText) {
                std::lock_guard const lock(eventsMutex);
                ++assistantMessageCount;
                eventOrder.emplace_back("assistant");
            }
        }
    );
    [[maybe_unused]] auto const runStartedSubscription = bus.subscribe<mts::events::RunStarted>(
        [&](mts::events::RunStarted const& event) {
            if (event.session == session) {
                std::lock_guard const lock(eventsMutex);
                eventOrder.emplace_back("started");
            }
        }
    );
    [[maybe_unused]] auto const tokenSubscription = bus.subscribe<mts::events::TokenDelta>(
        [&](mts::events::TokenDelta const& event) {
            if (event.session == session) {
                std::lock_guard const lock(eventsMutex);
                streamedText += event.delta;
                eventOrder.emplace_back("delta");
            }
        }
    );
    [[maybe_unused]] auto const toolStartedSubscription = bus.subscribe<mts::events::ToolCallStarted>(
        [&](mts::events::ToolCallStarted const& event) {
            if (event.session == session) {
                std::lock_guard const lock(eventsMutex);
                eventOrder.emplace_back("tool-started");
            }
        }
    );
    [[maybe_unused]] auto const toolFinishedSubscription = bus.subscribe<mts::events::ToolCallFinished>(
        [&](mts::events::ToolCallFinished const& event) {
            if (event.session == session) {
                std::lock_guard const lock(eventsMutex);
                eventOrder.emplace_back("tool-finished");
            }
        }
    );
    [[maybe_unused]] auto const runFinishedSubscription = bus.subscribe<mts::events::RunFinished>(
        [&](mts::events::RunFinished const& event) {
            if (event.session == session) {
                std::lock_guard const lock(eventsMutex);
                eventOrder.emplace_back("finished");
            }
        }
    );

    REQUIRE(agentHarness.submitUserMessage(session, "test prompt"));
    mts::runtime::Cancellation cancellation;
    REQUIRE(agentHarness.runBlocking(session, cancellation));

    std::lock_guard const lock(eventsMutex);
    REQUIRE(eventOrder.size() >= 7);
    REQUIRE(eventOrder.front() == "user");
    REQUIRE(eventOrder[1] == "started");
    REQUIRE(eventOrder[eventOrder.size() - 4] == "tool-started");
    REQUIRE(eventOrder[eventOrder.size() - 3] == "tool-finished");
    REQUIRE(eventOrder[eventOrder.size() - 2] == "assistant");
    REQUIRE(eventOrder.back() == "finished");
    REQUIRE(assistantMessageCount == 1);
    REQUIRE(streamedText == response);
    auto const assistant = std::ranges::find(eventOrder, "assistant");
    REQUIRE(assistant != eventOrder.end());
    auto const lastDelta = std::ranges::find_last(eventOrder, "delta");
    REQUIRE(lastDelta.begin() != eventOrder.end());
    REQUIRE(assistant > lastDelta.begin());
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
