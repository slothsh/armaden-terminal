export module persistence;

import std;
import agent;

export namespace mts::persistence {

using SessionId = std::string;

enum class MessageKind {
    UserText,
    AssistantText,
    Reasoning,
    ToolCall,
    ToolResult,
    Error,
};

struct SessionInfo {
    SessionId id;
    std::string title;
    std::int64_t createdAt = 0;
    std::int64_t updatedAt = 0;
};

struct Message {
    std::string id;
    SessionId session;
    MessageKind kind = MessageKind::UserText;
    std::string text;
    std::string payload;
    std::uint64_t sequence = 0;
    std::int64_t timestamp = 0;
};

using Status = mts::agent::Status;

template <class Type>
using Result = mts::agent::Result<Type>;

template <class Repository>
concept SessionRepository = requires(Repository& repository, SessionId const& id, std::string title) {
    { repository.createSession(title) } -> std::same_as<Result<SessionInfo>>;
    { repository.listSessions() } -> std::same_as<Result<std::vector<SessionInfo>>>;
    { repository.getSession(id) } -> std::same_as<Result<SessionInfo>>;
    { repository.renameSession(id, title) } -> std::same_as<Status>;
    { repository.deleteSession(id) } -> std::same_as<Status>;
};

template <class Store>
concept ConversationStore = requires(Store& store, SessionId const& id, Message message) {
    { store.appendMessage(message) } -> std::same_as<Status>;
    { store.loadMessages(id) } -> std::same_as<Result<std::vector<Message>>>;
    { store.clearMessages(id) } -> std::same_as<Status>;
};

class InMemorySessionStore {
public:
    InMemorySessionStore();
    ~InMemorySessionStore();

    InMemorySessionStore(InMemorySessionStore const&) = delete;
    InMemorySessionStore& operator=(InMemorySessionStore const&) = delete;
    InMemorySessionStore(InMemorySessionStore&&) noexcept;
    InMemorySessionStore& operator=(InMemorySessionStore&&) noexcept;

    auto createSession(std::string title) -> Result<SessionInfo>;
    auto listSessions() -> Result<std::vector<SessionInfo>>;
    auto getSession(SessionId const& id) -> Result<SessionInfo>;
    auto renameSession(SessionId const& id, std::string title) -> Status;
    auto deleteSession(SessionId const& id) -> Status;

    auto appendMessage(Message message) -> Status;
    auto loadMessages(SessionId const& id) -> Result<std::vector<Message>>;
    auto clearMessages(SessionId const& id) -> Status;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

class LanggraphSessionStore {
public:
    [[nodiscard]] static auto open(std::filesystem::path const& databasePath) -> Result<LanggraphSessionStore>;

    ~LanggraphSessionStore();

    LanggraphSessionStore(LanggraphSessionStore const&) = delete;
    LanggraphSessionStore& operator=(LanggraphSessionStore const&) = delete;
    LanggraphSessionStore(LanggraphSessionStore&&) noexcept;
    LanggraphSessionStore& operator=(LanggraphSessionStore&&) noexcept;

    auto createSession(std::string title) -> Result<SessionInfo>;
    auto listSessions() -> Result<std::vector<SessionInfo>>;
    auto getSession(SessionId const& id) -> Result<SessionInfo>;
    auto renameSession(SessionId const& id, std::string title) -> Status;
    auto deleteSession(SessionId const& id) -> Status;

    auto appendMessage(Message message) -> Status;
    auto loadMessages(SessionId const& id) -> Result<std::vector<Message>>;
    auto clearMessages(SessionId const& id) -> Status;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    explicit LanggraphSessionStore(std::unique_ptr<Impl> impl);
};

class SessionStoreHandle {
public:
    explicit SessionStoreHandle(InMemorySessionStore store);
    explicit SessionStoreHandle(LanggraphSessionStore store);

    auto createSession(std::string title) -> Result<SessionInfo>;
    auto listSessions() -> Result<std::vector<SessionInfo>>;
    auto getSession(SessionId const& id) -> Result<SessionInfo>;
    auto renameSession(SessionId const& id, std::string title) -> Status;
    auto deleteSession(SessionId const& id) -> Status;

    auto appendMessage(Message message) -> Status;
    auto loadMessages(SessionId const& id) -> Result<std::vector<Message>>;
    auto clearMessages(SessionId const& id) -> Status;

private:
    std::variant<InMemorySessionStore, LanggraphSessionStore> m_store;
};

[[nodiscard]] auto databasePath() -> std::filesystem::path;
[[nodiscard]] auto makeSessionStore(std::filesystem::path const& databasePath) -> Result<SessionStoreHandle>;

}

namespace mts::persistence::detail {

inline auto nowMillis() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline auto generateId() -> std::string {
    static std::atomic<std::uint64_t> counter{0};
    std::random_device device;
    std::uint64_t const random = (static_cast<std::uint64_t>(device()) << 32) ^ device();
    std::uint64_t const sequence = counter.fetch_add(1);
    std::ostringstream stream;
    stream << std::hex << random << '-' << sequence;
    return stream.str();
}

inline auto kindName(MessageKind kind) -> std::string_view {
    switch (kind) {
        case MessageKind::UserText: return "user";
        case MessageKind::AssistantText: return "assistant";
        case MessageKind::Reasoning: return "reasoning";
        case MessageKind::ToolCall: return "tool_call";
        case MessageKind::ToolResult: return "tool_result";
        case MessageKind::Error: return "error";
    }
    return "user";
}

inline auto kindFromName(std::string_view name) -> MessageKind {
    if (name == "assistant") return MessageKind::AssistantText;
    if (name == "reasoning") return MessageKind::Reasoning;
    if (name == "tool_call") return MessageKind::ToolCall;
    if (name == "tool_result") return MessageKind::ToolResult;
    if (name == "error") return MessageKind::Error;
    return MessageKind::UserText;
}

inline auto toJson(SessionInfo const& session) -> mts::agent::json {
    return mts::agent::json{
        {"id", session.id},
        {"title", session.title},
        {"createdAt", session.createdAt},
        {"updatedAt", session.updatedAt},
    };
}

inline auto sessionFromJson(mts::agent::json const& value) -> Result<SessionInfo> {
    if (!value.is_object())
        return Status::dataLoss("session record is not an object");
    SessionInfo session;
    session.id = value.value("id", std::string{});
    session.title = value.value("title", std::string{});
    session.createdAt = value.value("createdAt", std::int64_t{0});
    session.updatedAt = value.value("updatedAt", std::int64_t{0});
    if (session.id.empty())
        return Status::dataLoss("session record has no id");
    return session;
}

inline auto toJson(Message const& message) -> mts::agent::json {
    return mts::agent::json{
        {"id", message.id},
        {"session", message.session},
        {"kind", std::string(kindName(message.kind))},
        {"text", message.text},
        {"payload", message.payload},
        {"sequence", message.sequence},
        {"timestamp", message.timestamp},
    };
}

inline auto messageFromJson(mts::agent::json const& value) -> Result<Message> {
    if (!value.is_object())
        return Status::dataLoss("message record is not an object");
    Message message;
    message.id = value.value("id", std::string{});
    message.session = value.value("session", std::string{});
    message.kind = kindFromName(value.value("kind", std::string("user")));
    message.text = value.value("text", std::string{});
    message.payload = value.value("payload", std::string{});
    message.sequence = value.value("sequence", std::uint64_t{0});
    message.timestamp = value.value("timestamp", std::int64_t{0});
    return message;
}

inline auto messageKey(std::string_view session, std::uint64_t sequence) -> std::string {
    std::ostringstream stream;
    stream << session << '/' << std::setw(12) << std::setfill('0') << sequence;
    return stream.str();
}

inline constexpr std::string_view sessionScope = "mindtosis.sessions";
inline constexpr std::string_view messageScope = "mindtosis.messages";

}

namespace mts::persistence {

struct InMemorySessionStore::Impl {
    std::mutex mutex;
    std::map<SessionId, SessionInfo> sessions;
    std::map<SessionId, std::vector<Message>> messages;
    std::uint64_t nextSequence = 0;
};

InMemorySessionStore::InMemorySessionStore()
    : m_impl(std::make_unique<Impl>()) {}

InMemorySessionStore::~InMemorySessionStore() = default;

InMemorySessionStore::InMemorySessionStore(InMemorySessionStore&&) noexcept = default;

InMemorySessionStore& InMemorySessionStore::operator=(InMemorySessionStore&&) noexcept = default;

auto InMemorySessionStore::createSession(std::string title) -> Result<SessionInfo> {
    std::lock_guard const lock(m_impl->mutex);
    SessionInfo session;
    session.id = detail::generateId();
    session.title = std::move(title);
    session.createdAt = detail::nowMillis();
    session.updatedAt = session.createdAt;
    m_impl->sessions.emplace(session.id, session);
    return session;
}

auto InMemorySessionStore::listSessions() -> Result<std::vector<SessionInfo>> {
    std::lock_guard const lock(m_impl->mutex);
    std::vector<SessionInfo> sessions;
    sessions.reserve(m_impl->sessions.size());
    for (auto const& [id, session] : m_impl->sessions)
        sessions.push_back(session);
    std::sort(sessions.begin(), sessions.end(), [](SessionInfo const& left, SessionInfo const& right) {
        return left.updatedAt > right.updatedAt;
    });
    return sessions;
}

auto InMemorySessionStore::getSession(SessionId const& id) -> Result<SessionInfo> {
    std::lock_guard const lock(m_impl->mutex);
    auto const it = m_impl->sessions.find(id);
    if (it == m_impl->sessions.end())
        return Status::notFound("session not found");
    return it->second;
}

auto InMemorySessionStore::renameSession(SessionId const& id, std::string title) -> Status {
    std::lock_guard const lock(m_impl->mutex);
    auto const it = m_impl->sessions.find(id);
    if (it == m_impl->sessions.end())
        return Status::notFound("session not found");
    it->second.title = std::move(title);
    it->second.updatedAt = detail::nowMillis();
    return Status::ok();
}

auto InMemorySessionStore::deleteSession(SessionId const& id) -> Status {
    std::lock_guard const lock(m_impl->mutex);
    m_impl->sessions.erase(id);
    m_impl->messages.erase(id);
    return Status::ok();
}

auto InMemorySessionStore::appendMessage(Message message) -> Status {
    std::lock_guard const lock(m_impl->mutex);
    if (!m_impl->sessions.contains(message.session))
        return Status::notFound("session not found");
    if (message.id.empty())
        message.id = detail::generateId();
    if (message.sequence == 0)
        message.sequence = ++m_impl->nextSequence;
    if (message.timestamp == 0)
        message.timestamp = detail::nowMillis();
    m_impl->messages[message.session].push_back(std::move(message));
    m_impl->sessions[message.session].updatedAt = detail::nowMillis();
    return Status::ok();
}

auto InMemorySessionStore::loadMessages(SessionId const& id) -> Result<std::vector<Message>> {
    std::lock_guard const lock(m_impl->mutex);
    auto const it = m_impl->messages.find(id);
    if (it == m_impl->messages.end())
        return std::vector<Message>{};
    std::vector<Message> messages = it->second;
    std::sort(messages.begin(), messages.end(), [](Message const& left, Message const& right) {
        return left.sequence < right.sequence;
    });
    return messages;
}

auto InMemorySessionStore::clearMessages(SessionId const& id) -> Status {
    std::lock_guard const lock(m_impl->mutex);
    m_impl->messages.erase(id);
    return Status::ok();
}

struct LanggraphSessionStore::Impl {
    std::shared_ptr<mts::agent::IStorage> storage;
};

LanggraphSessionStore::LanggraphSessionStore(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl)) {}

LanggraphSessionStore::~LanggraphSessionStore() = default;

LanggraphSessionStore::LanggraphSessionStore(LanggraphSessionStore&&) noexcept = default;

LanggraphSessionStore& LanggraphSessionStore::operator=(LanggraphSessionStore&&) noexcept = default;

auto LanggraphSessionStore::open(std::filesystem::path const& path) -> Result<LanggraphSessionStore> {
    std::error_code error;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), error);

    auto storage = std::make_shared<mts::agent::SQLiteStorage>(path.string());
    if (auto const status = storage->open(); !status)
        return status.status();

    auto impl = std::make_unique<Impl>();
    impl->storage = std::move(storage);
    return LanggraphSessionStore(std::move(impl));
}

auto LanggraphSessionStore::createSession(std::string title) -> Result<SessionInfo> {
    SessionInfo session;
    session.id = detail::generateId();
    session.title = std::move(title);
    session.createdAt = detail::nowMillis();
    session.updatedAt = session.createdAt;

    auto const value = detail::toJson(session).dump();
    auto const stored = m_impl->storage->put(
        mts::agent::StorageKey{std::string(detail::sessionScope), session.id}, value);
    if (!stored)
        return stored.status();

    return session;
}

auto LanggraphSessionStore::listSessions() -> Result<std::vector<SessionInfo>> {
    mts::agent::StorageListOptions options;
    options.scope_ = std::string(detail::sessionScope);
    options.limit_ = 1'000;

    auto const listed = m_impl->storage->list(options);
    if (!listed)
        return listed.status();

    std::vector<SessionInfo> sessions;
    sessions.reserve(listed.value().items_.size());
    for (auto const& item : listed.value().items_) {
        auto const parsed = detail::sessionFromJson(mts::agent::json::parse(item.value_, nullptr, false));
        if (!parsed)
            return parsed.status();
        sessions.push_back(parsed.value());
    }

    std::sort(sessions.begin(), sessions.end(), [](SessionInfo const& left, SessionInfo const& right) {
        return left.updatedAt > right.updatedAt;
    });
    return sessions;
}

auto LanggraphSessionStore::getSession(SessionId const& id) -> Result<SessionInfo> {
    auto const item = m_impl->storage->get(mts::agent::StorageKey{std::string(detail::sessionScope), id});
    if (!item)
        return item.status();
    if (!item.value().has_value())
        return Status::notFound("session not found");

    auto const parsed = detail::sessionFromJson(mts::agent::json::parse(item.value()->value_, nullptr, false));
    if (!parsed)
        return parsed.status();
    return parsed.value();
}

auto LanggraphSessionStore::renameSession(SessionId const& id, std::string title) -> Status {
    auto const session = getSession(id);
    if (!session)
        return session.status();

    SessionInfo updated = session.value();
    updated.title = std::move(title);
    updated.updatedAt = detail::nowMillis();

    auto const value = detail::toJson(updated).dump();
    auto const stored = m_impl->storage->put(mts::agent::StorageKey{std::string(detail::sessionScope), id}, value);
    return stored.isOk() ? Status::ok() : stored.status();
}

auto LanggraphSessionStore::deleteSession(SessionId const& id) -> Status {
    if (auto const status = clearMessages(id); !status)
        return status;
    auto const removed = m_impl->storage->remove(mts::agent::StorageKey{std::string(detail::sessionScope), id});
    return removed.isOk() ? Status::ok() : removed.status();
}

auto LanggraphSessionStore::appendMessage(Message message) -> Status {
    auto const session = getSession(message.session);
    if (!session)
        return session.status();

    if (message.id.empty())
        message.id = detail::generateId();
    if (message.timestamp == 0)
        message.timestamp = detail::nowMillis();
    if (message.sequence == 0) {
        auto const existing = loadMessages(message.session);
        if (!existing)
            return existing.status();
        std::uint64_t next = 1;
        for (auto const& stored : existing.value())
            next = std::max(next, stored.sequence + 1);
        message.sequence = next;
    }

    auto const value = detail::toJson(message).dump();
    auto const stored = m_impl->storage->put(
        mts::agent::StorageKey{std::string(detail::messageScope), detail::messageKey(message.session, message.sequence)},
        value);
    if (!stored)
        return stored.status();

    SessionInfo updated = session.value();
    updated.updatedAt = detail::nowMillis();
    auto const touched = m_impl->storage->put(
        mts::agent::StorageKey{std::string(detail::sessionScope), session.value().id},
        detail::toJson(updated).dump()
    );
    return touched.isOk() ? Status::ok() : touched.status();
}

auto LanggraphSessionStore::loadMessages(SessionId const& id) -> Result<std::vector<Message>> {
    mts::agent::StorageListOptions options;
    options.scope_ = std::string(detail::messageScope);
    options.keyPrefix_ = id + "/";
    options.limit_ = 1'000;

    auto const listed = m_impl->storage->list(options);
    if (!listed)
        return listed.status();

    std::vector<Message> messages;
    messages.reserve(listed.value().items_.size());
    for (auto const& item : listed.value().items_) {
        auto const parsed = detail::messageFromJson(mts::agent::json::parse(item.value_, nullptr, false));
        if (!parsed)
            return parsed.status();
        messages.push_back(parsed.value());
    }

    std::sort(messages.begin(), messages.end(), [](Message const& left, Message const& right) {
        return left.sequence < right.sequence;
    });
    return messages;
}

auto LanggraphSessionStore::clearMessages(SessionId const& id) -> Status {
    mts::agent::StorageListOptions options;
    options.scope_ = std::string(detail::messageScope);
    options.keyPrefix_ = id + "/";
    options.limit_ = 1'000;

    auto const listed = m_impl->storage->list(options);
    if (!listed)
        return listed.status();

    for (auto const& item : listed.value().items_) {
        auto const removed = m_impl->storage->remove(item.key_);
        if (!removed)
            return removed.status();
    }
    return Status::ok();
}

SessionStoreHandle::SessionStoreHandle(InMemorySessionStore store)
    : m_store(std::move(store)) {}

SessionStoreHandle::SessionStoreHandle(LanggraphSessionStore store)
    : m_store(std::move(store)) {}

auto SessionStoreHandle::createSession(std::string title) -> Result<SessionInfo> {
    return std::visit([&title](auto& store) {
        return store.createSession(std::move(title));
    }, m_store);
}

auto SessionStoreHandle::listSessions() -> Result<std::vector<SessionInfo>> {
    return std::visit([](auto& store) {
        return store.listSessions();
    }, m_store);
}

auto SessionStoreHandle::getSession(SessionId const& id) -> Result<SessionInfo> {
    return std::visit([&id](auto& store) {
        return store.getSession(id);
    }, m_store);
}

auto SessionStoreHandle::renameSession(SessionId const& id, std::string title) -> Status {
    return std::visit([&id, &title](auto& store) {
        return store.renameSession(id, std::move(title));
    }, m_store);
}

auto SessionStoreHandle::deleteSession(SessionId const& id) -> Status {
    return std::visit([&id](auto& store) {
        return store.deleteSession(id);
    }, m_store);
}

auto SessionStoreHandle::appendMessage(Message message) -> Status {
    return std::visit([&message](auto& store) {
        return store.appendMessage(std::move(message));
    }, m_store);
}

auto SessionStoreHandle::loadMessages(SessionId const& id) -> Result<std::vector<Message>> {
    return std::visit([&id](auto& store) {
        return store.loadMessages(id);
    }, m_store);
}

auto SessionStoreHandle::clearMessages(SessionId const& id) -> Status {
    return std::visit([&id](auto& store) {
        return store.clearMessages(id);
    }, m_store);
}

auto databasePath() -> std::filesystem::path {
    if (char const* env = std::getenv("MINDTOIS_DB"); env != nullptr && *env != '\0')
        return std::filesystem::path(env);

    if (char const* dataHome = std::getenv("XDG_DATA_HOME"); dataHome != nullptr && *dataHome != '\0')
        return std::filesystem::path(dataHome) / "mindtosis" / "mindtosis.db";

    if (char const* home = std::getenv("HOME"); home != nullptr && *home != '\0')
        return std::filesystem::path(home) / ".local" / "share" / "mindtosis" / "mindtosis.db";

    return std::filesystem::path("mindtosis.db");
}

auto makeSessionStore(std::filesystem::path const& path) -> Result<SessionStoreHandle> {
    auto store = LanggraphSessionStore::open(path);
    if (!store)
        return store.status();
    return SessionStoreHandle(std::move(store.value()));
}

}
