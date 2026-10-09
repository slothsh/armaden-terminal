export module events;

import std;
import runtime;

export namespace mts::events {

using SessionId = std::string;

enum class MessageKind {
    UserText,
    AssistantText,
    Reasoning,
    Error,
};

struct SessionCreated {
    SessionId session;
    std::string title;
};

struct SessionUpdated {
    SessionId session;
    std::string title;
};

struct RunStarted {
    SessionId session;
};

struct RunFinished {
    SessionId session;
};

struct RunFailed {
    SessionId session;
    std::string message;
};

struct MessageAdded {
    SessionId session;
    MessageKind kind;
    std::string text;
};

struct TokenDelta {
    SessionId session;
    std::string delta;
};

struct ToolCallStarted {
    SessionId session;
    std::string callId;
    std::string name;
    std::string arguments;
};

struct ToolCallFinished {
    SessionId session;
    std::string callId;
    std::string name;
    std::string result;
    bool ok;
};

struct BudgetExceeded {
    SessionId session;
    std::string detail;
};

struct CompactionApplied {
    SessionId session;
    std::size_t removed;
    std::string detail;
};

using EventTypes = std::tuple<
    SessionCreated,
    SessionUpdated,
    RunStarted,
    RunFinished,
    RunFailed,
    MessageAdded,
    TokenDelta,
    ToolCallStarted,
    ToolCallFinished,
    BudgetExceeded,
    CompactionApplied
>;

template <class Event>
class Signal {
public:
    using Handler = std::function<void(Event const&)>;

    [[nodiscard]] auto subscribe(Handler handler) -> std::size_t {
        std::lock_guard const lock(m_mutex);
        std::size_t const id = m_nextId++;
        m_entries.push_back(Entry{id, std::move(handler)});

        return id;
    }

    auto unsubscribe(std::size_t id) -> void {
        std::lock_guard const lock(m_mutex);
        std::erase_if(m_entries, [id](Entry const& entry) {
            return entry.id == id;
        });
    }

    auto publish(Event const& event) const -> void {
        std::vector<Handler> handlers;
        {
            std::lock_guard const lock(m_mutex);
            handlers.reserve(m_entries.size());
            for (Entry const& entry : m_entries) {
                handlers.push_back(entry.handler);
            }
        }

        for (Handler const& handler : handlers) {
            handler(event);
        }
    }

    [[nodiscard]] auto size() const -> std::size_t {
        std::lock_guard const lock(m_mutex);

        return m_entries.size();
    }

private:
    struct Entry {
        std::size_t id;
        Handler handler;
    };

    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
    std::size_t m_nextId = 0;
};

class EventBus {
public:
    explicit EventBus(runtime::ThreadPool* pool = nullptr)
        : m_pool(pool) {}

    EventBus(EventBus const&) = delete;
    auto operator=(EventBus const&) -> EventBus& = delete;
    EventBus(EventBus&&) = delete;
    auto operator=(EventBus&&) -> EventBus& = delete;

    template <class Event>
    [[nodiscard]] auto subscribe(typename Signal<Event>::Handler handler) -> std::size_t {
        return std::get<Signal<Event>>(m_signals).subscribe(std::move(handler));
    }

    template <class Event>
    auto unsubscribe(std::size_t id) -> void {
        std::get<Signal<Event>>(m_signals).unsubscribe(id);
    }

    template <class Event>
    auto publish(Event event) -> void {
        if (m_pool == nullptr) {
            std::get<Signal<Event>>(m_signals).publish(event);
            return;
        }

        m_pool->schedule([this, event = std::move(event)]() mutable {
            std::get<Signal<Event>>(m_signals).publish(event);
        });
    }

private:
    runtime::ThreadPool* m_pool;
    std::tuple<
        Signal<SessionCreated>,
        Signal<SessionUpdated>,
        Signal<RunStarted>,
        Signal<RunFinished>,
        Signal<RunFailed>,
        Signal<MessageAdded>,
        Signal<TokenDelta>,
        Signal<ToolCallStarted>,
        Signal<ToolCallFinished>,
        Signal<BudgetExceeded>,
        Signal<CompactionApplied>
    > m_signals;
};

} // END OF NAMESPACE `mts::events`
