export module orchestration;

import std;
import foundation;
import runtime;
import persistence;
import harness;

export namespace mts::orchestration {

enum class BlockKind {
    UserText,
    AssistantText,
    Reasoning,
    ToolCall,
    ToolResult,
    Error,
};

struct Block {
    BlockKind kind = BlockKind::AssistantText;
    std::string text;
    std::string payload;
};

struct SessionSummary {
    std::string id;
    std::string title;
    std::int64_t updatedAt = 0;
    bool running = false;
};

struct PendingRun {
    std::vector<Block> blocks;
    int streamingIndex = -1;
};

class ConversationService {
public:
    ConversationService(
        mts::persistence::SessionStoreHandle* store,
        mts::harness::AgentHarness* harness,
        std::string initialStatus = {}
    )
        : m_store(store)
        , m_harness(harness)
        , m_status(std::move(initialStatus)) {}

    ConversationService(ConversationService const&) = delete;
    auto operator=(ConversationService const&) -> ConversationService& = delete;

    auto shutdown() -> void {
        for (auto const& [id, cancellation] : m_cancellations) {
            if (cancellation != nullptr) {
                cancellation->cancel();
            }
        }
    }

    [[nodiscard]] auto activeSession() const -> std::string const& {
        return this->m_activeSession;
    }

    [[nodiscard]] auto draft() const -> std::string const& {
        return this->m_draft;
    }

    [[nodiscard]] auto status() const -> std::string const& {
        return this->m_status;
    }

    [[nodiscard]] auto running() const -> bool {
        return !this->m_activeSession.empty() && this->m_inFlight.contains(this->m_activeSession);
    }

    [[nodiscard]] auto hasInFlight() const -> bool {
        return !this->m_inFlight.empty();
    }

    [[nodiscard]] auto listSessions() -> std::vector<SessionSummary> {
        std::vector<SessionSummary> result;
        if (this->m_store == nullptr) {
            return result;
        }

        auto const sessions = this->m_store->listSessions();
        if (!sessions) {
            return result;
        }

        result.reserve(sessions.value().size());
        for (auto const& session : sessions.value()) {
            result.push_back(SessionSummary{
                session.id,
                session.title,
                session.updatedAt,
                this->m_inFlight.contains(session.id),
            });
        }

        return result;
    }

    [[nodiscard]] auto storedBlocks(std::string const& id) -> std::vector<Block> {
        std::vector<Block> result;
        if (this->m_store == nullptr) {
            return result;
        }

        auto const messages = this->m_store->loadMessages(id);
        if (!messages) {
            return result;
        }

        result.reserve(messages.value().size());
        for (auto const& message : messages.value()) {
            result.push_back(Block{toBlockKind(message.kind), message.text, message.payload});
        }

        return result;
    }

    [[nodiscard]] auto pendingRun(std::string const& id) const -> PendingRun const* {
        auto const it = this->m_pending.find(id);

        return it == this->m_pending.end() ? nullptr : &it->second;
    }

    auto createSession() -> std::string {
        if (this->m_store == nullptr) {
            return {};
        }

        auto const session = this->m_store->createSession(std::string(mts::persistence::kDefaultSessionTitle));
        if (!session) {
            this->setStatus(std::string(session.error().message()));
            return {};
        }

        this->m_activeSession = session.value().id;
        this->m_draft.clear();

        return session.value().id;
    }

    auto deleteSession(std::string const& id) -> bool {
        if (this->m_store == nullptr) {
            return false;
        }

        if (auto const status = this->m_store->deleteSession(id); !status) {
            this->setStatus(std::string(status.error().message()));
            return false;
        }

        if (auto it = this->m_cancellations.find(id); it != this->m_cancellations.end()) {
            if (it->second != nullptr) {
                it->second->cancel();
            }
            this->m_cancellations.erase(it);
        }

        this->m_inFlight.erase(id);
        this->m_pending.erase(id);

        if (this->m_activeSession == id) {
            this->m_activeSession.clear();
            this->m_draft.clear();

            auto const sessions = this->m_store->listSessions();
            if (sessions && !sessions.value().empty()) {
                this->selectSession(sessions.value().front().id);
            }
        }

        return true;
    }

    auto renameSession(std::string const& id, std::string const& title) -> bool {
        if (this->m_store == nullptr) {
            return false;
        }

        if (auto const status = this->m_store->renameSession(id, title); !status) {
            this->setStatus(std::string(status.error().message()));
            return false;
        }

        return true;
    }

    auto clearHistory() -> void {
        if (this->m_store != nullptr) {
            auto const sessions = this->m_store->listSessions();
            if (sessions) {
                for (auto const& session : sessions.value()) {
                    static_cast<void>(this->m_store->deleteSession(session.id));
                }
            }
        }

        for (auto const& [id, cancellation] : this->m_cancellations) {
            if (cancellation != nullptr) {
                cancellation->cancel();
            }
        }

        this->m_cancellations.clear();
        this->m_pending.clear();
        this->m_inFlight.clear();
        this->m_activeSession.clear();
        this->m_draft.clear();
    }

    auto selectSession(std::string const& id) -> void {
        this->m_activeSession = id;
        this->loadDraft(id);
    }

    auto saveDraft(std::string const& text) -> void {
        this->m_draft = text;
        if (this->m_store != nullptr && !this->m_activeSession.empty()) {
            static_cast<void>(this->m_store->saveDraft(this->m_activeSession, text));
        }
    }

    auto submit(std::string const& text) -> bool {
        if (this->m_harness == nullptr || this->m_inFlight.contains(this->m_activeSession)) {
            return false;
        }

        if (trimmed(text).empty()) {
            return false;
        }

        if (this->m_activeSession.empty()) {
            static_cast<void>(this->createSession());
        }

        if (this->m_activeSession.empty()) {
            return false;
        }

        if (auto const status = this->m_harness->submitUserMessage(this->m_activeSession, text); !status) {
            this->setStatus(std::string(status.message()));
            return false;
        }

        auto cancellation = std::make_shared<mts::runtime::Cancellation>();
        this->m_cancellations[this->m_activeSession] = cancellation;
        this->m_inFlight.insert(this->m_activeSession);
        this->saveDraft({});
        this->m_harness->run(this->m_activeSession, cancellation);

        return true;
    }

    auto cancel() -> void {
        auto const it = this->m_cancellations.find(this->m_activeSession);
        if (it != this->m_cancellations.end() && it->second != nullptr) {
            it->second->cancel();
        }
    }

    auto cancelAll() -> void {
        for (auto const& [id, cancellation] : this->m_cancellations) {
            if (cancellation != nullptr) {
                cancellation->cancel();
            }
        }
    }

    auto runStarted(std::string const& id) -> void {
        this->m_inFlight.insert(id);
    }

    auto runFinished(std::string const& id) -> void {
        this->m_inFlight.erase(id);
        this->m_cancellations.erase(id);
        this->m_pending.erase(id);
    }

    auto runFailed(std::string const& id) -> void {
        this->runFinished(id);
    }

    auto tokenDelta(std::string const& id, std::string const& delta) -> void {
        PendingRun& pending = this->m_pending[id];
        if (pending.streamingIndex < 0) {
            pending.blocks.push_back(Block{BlockKind::AssistantText, {}, {}});
            pending.streamingIndex = static_cast<int>(pending.blocks.size()) - 1;
        }

        pending.blocks[static_cast<std::size_t>(pending.streamingIndex)].text += delta;
    }

private:
    [[nodiscard]] static auto toBlockKind(mts::persistence::MessageKind kind) -> BlockKind {
        switch (kind) {
            case mts::persistence::MessageKind::UserText:
                return BlockKind::UserText;

            case mts::persistence::MessageKind::AssistantText:
                return BlockKind::AssistantText;

            case mts::persistence::MessageKind::Reasoning:
                return BlockKind::Reasoning;

            case mts::persistence::MessageKind::ToolCall:
                return BlockKind::ToolCall;

            case mts::persistence::MessageKind::ToolResult:
                return BlockKind::ToolResult;

            case mts::persistence::MessageKind::Error:
                return BlockKind::Error;
        }

        return BlockKind::AssistantText;
    }

    [[nodiscard]] static auto trimmed(std::string const& text) -> std::string {
        auto const first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return {};
        }

        auto const last = text.find_last_not_of(" \t\r\n");

        return text.substr(first, last - first + 1);
    }

    auto setStatus(std::string text) -> void {
        this->m_status = std::move(text);
    }

    auto loadDraft(std::string const& id) -> void {
        this->m_draft.clear();
        if (this->m_store == nullptr) {
            return;
        }

        auto const session = this->m_store->getSession(id);
        if (session) {
            this->m_draft = session.value().draft;
        }
    }

    mts::persistence::SessionStoreHandle* m_store;
    mts::harness::AgentHarness* m_harness;
    std::unordered_map<std::string, std::shared_ptr<mts::runtime::Cancellation>> m_cancellations;
    std::unordered_map<std::string, PendingRun> m_pending;
    std::unordered_set<std::string> m_inFlight;
    std::string m_activeSession;
    std::string m_draft;
    std::string m_status;
};

} // END OF NAMESPACE `mts::orchestration`
