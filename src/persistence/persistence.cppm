export module persistence;

import std;
import agent;
import core;

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
    std::string draft;
    std::int64_t createdAt = 0;
    std::int64_t updatedAt = 0;
};

inline constexpr std::string_view kDefaultSessionTitle = "New session";

struct Message {
    std::string id;
    SessionId session;
    MessageKind kind = MessageKind::UserText;
    std::string text;
    std::string payload;
    std::uint64_t sequence = 0;
    std::int64_t timestamp = 0;
};

using Status = mts::core::Status;

template <class Type>
using Result = mts::core::Result<Type>;

enum class PersistenceErrorKind : mts::core::u8 {
    Unknown,
    InvalidArgument,
    NotFound,
    Unavailable,
    Internal,
    Cancelled,
    ResourceExhausted,
    OutOfRange,
    DataLoss,
    PermissionDenied,
};

static_assert(mts::core::ErrorKind<PersistenceErrorKind>);

template <class Repository>
concept SessionRepository = requires(
    Repository& repository,
    SessionId const& id,
    std::string title,
    std::string draft
) {
    { repository.createSession(title) } -> std::same_as<Result<SessionInfo>>;
    { repository.listSessions() } -> std::same_as<Result<std::vector<SessionInfo>>>;
    { repository.getSession(id) } -> std::same_as<Result<SessionInfo>>;
    { repository.renameSession(id, title) } -> std::same_as<Status>;
    { repository.saveDraft(id, draft) } -> std::same_as<Status>;
    { repository.deleteSession(id) } -> std::same_as<Status>;
};

template <class Store>
concept ConversationStore = requires(
    Store& store,
    SessionId const& id,
    Message message
) {
    { store.appendMessage(message) } -> std::same_as<Status>;
    { store.loadMessages(id) } -> std::same_as<Result<std::vector<Message>>>;
    { store.clearMessages(id) } -> std::same_as<Status>;
};

class InMemorySessionStore {
public:
    InMemorySessionStore();
    ~InMemorySessionStore();

    InMemorySessionStore(InMemorySessionStore const&) = delete;
    auto operator=(InMemorySessionStore const&) -> InMemorySessionStore& = delete;
    InMemorySessionStore(InMemorySessionStore&&) noexcept;
    auto operator=(InMemorySessionStore&&) noexcept -> InMemorySessionStore&;

    auto createSession(std::string title) -> Result<SessionInfo>;
    auto listSessions() -> Result<std::vector<SessionInfo>>;
    auto getSession(SessionId const& id) -> Result<SessionInfo>;
    auto renameSession(SessionId const& id, std::string title) -> Status;
    auto saveDraft(SessionId const& id, std::string draft) -> Status;
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
    auto operator=(LanggraphSessionStore const&) -> LanggraphSessionStore& = delete;
    LanggraphSessionStore(LanggraphSessionStore&&) noexcept;
    auto operator=(LanggraphSessionStore&&) noexcept -> LanggraphSessionStore&;

    auto createSession(std::string title) -> Result<SessionInfo>;
    auto listSessions() -> Result<std::vector<SessionInfo>>;
    auto getSession(SessionId const& id) -> Result<SessionInfo>;
    auto renameSession(SessionId const& id, std::string title) -> Status;
    auto saveDraft(SessionId const& id, std::string draft) -> Status;
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
    auto saveDraft(SessionId const& id, std::string draft) -> Status;
    auto deleteSession(SessionId const& id) -> Status;

    auto appendMessage(Message message) -> Status;
    auto loadMessages(SessionId const& id) -> Result<std::vector<Message>>;
    auto clearMessages(SessionId const& id) -> Status;

private:
    std::variant<InMemorySessionStore, LanggraphSessionStore> m_store;
};

static_assert(SessionRepository<InMemorySessionStore>);
static_assert(ConversationStore<InMemorySessionStore>);
static_assert(SessionRepository<LanggraphSessionStore>);
static_assert(ConversationStore<LanggraphSessionStore>);
static_assert(SessionRepository<SessionStoreHandle>);
static_assert(ConversationStore<SessionStoreHandle>);

[[nodiscard]] auto makeSessionStore(std::filesystem::path const& databasePath) -> Result<SessionStoreHandle>;

} // END OF NAMESPACE `mts::persistence`

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
        case MessageKind::UserText:
            return "user";

        case MessageKind::AssistantText:
            return "assistant";

        case MessageKind::Reasoning:
            return "reasoning";

        case MessageKind::ToolCall:
            return "tool_call";

        case MessageKind::ToolResult:
            return "tool_result";

        case MessageKind::Error:
            return "error";
    }

    return "user";
}

inline auto kindFromName(std::string_view name) -> MessageKind {
    if (name == "assistant") {
        return MessageKind::AssistantText;
    }

    if (name == "reasoning") {
        return MessageKind::Reasoning;
    }

    if (name == "tool_call") {
        return MessageKind::ToolCall;
    }

    if (name == "tool_result") {
        return MessageKind::ToolResult;
    }

    if (name == "error") {
        return MessageKind::Error;
    }

    return MessageKind::UserText;
}

inline auto toJson(SessionInfo const& session) -> mts::agent::json {
    return mts::agent::json{
        {"id", session.id},
        {"title", session.title},
        {"draft", session.draft},
        {"createdAt", session.createdAt},
        {"updatedAt", session.updatedAt},
    };
}

inline auto sessionFromJson(mts::agent::json const& value) -> Result<SessionInfo> {
    if (!value.is_object()) {
        return Result<SessionInfo>::err(PersistenceErrorKind::DataLoss, "session record is not an object");
    }

    SessionInfo session;
    session.id = value.value("id", std::string{});
    session.title = value.value("title", std::string{});
    session.draft = value.value("draft", std::string{});
    session.createdAt = value.value("createdAt", std::int64_t{0});
    session.updatedAt = value.value("updatedAt", std::int64_t{0});
    if (session.id.empty()) {
        return Result<SessionInfo>::err(PersistenceErrorKind::DataLoss, "session record has no id");
    }

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
    if (!value.is_object()) {
        return Result<Message>::err(PersistenceErrorKind::DataLoss, "message record is not an object");
    }

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

} // END OF NAMESPACE `mts::persistence::detail`

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

auto InMemorySessionStore::operator=(InMemorySessionStore&&) noexcept -> InMemorySessionStore& = default;

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
    for (auto const& [id, session] : m_impl->sessions) {
        sessions.push_back(session);
    }

    std::sort(sessions.begin(), sessions.end(), [](SessionInfo const& left, SessionInfo const& right) {
        return left.updatedAt > right.updatedAt;
    });

    return sessions;
}

auto InMemorySessionStore::getSession(SessionId const& id) -> Result<SessionInfo> {
    std::lock_guard const lock(m_impl->mutex);

    auto const it = m_impl->sessions.find(id);
    if (it == m_impl->sessions.end()) {
        return Result<SessionInfo>::err(PersistenceErrorKind::NotFound, "session not found");
    }

    return it->second;
}

auto InMemorySessionStore::renameSession(SessionId const& id, std::string title) -> Status {
    std::lock_guard const lock(m_impl->mutex);

    auto const it = m_impl->sessions.find(id);
    if (it == m_impl->sessions.end()) {
        return Status::err(PersistenceErrorKind::NotFound, "session not found");
    }

    it->second.title = std::move(title);
    it->second.updatedAt = detail::nowMillis();

    return Status::ok();
}

auto InMemorySessionStore::saveDraft(SessionId const& id, std::string draft) -> Status {
    std::lock_guard const lock(m_impl->mutex);

    auto const it = m_impl->sessions.find(id);
    if (it == m_impl->sessions.end()) {
        return Status::err(PersistenceErrorKind::NotFound, "session not found");
    }

    it->second.draft = std::move(draft);

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

    if (!m_impl->sessions.contains(message.session)) {
        return Status::err(PersistenceErrorKind::NotFound, "session not found");
    }

    if (message.id.empty()) {
        message.id = detail::generateId();
    }

    if (message.sequence == 0) {
        message.sequence = ++m_impl->nextSequence;
    }

    if (message.timestamp == 0) {
        message.timestamp = detail::nowMillis();
    }

    m_impl->messages[message.session].push_back(std::move(message));
    m_impl->sessions[message.session].updatedAt = detail::nowMillis();

    return Status::ok();
}

auto InMemorySessionStore::loadMessages(SessionId const& id) -> Result<std::vector<Message>> {
    std::lock_guard const lock(m_impl->mutex);

    auto const it = m_impl->messages.find(id);
    if (it == m_impl->messages.end()) {
        return std::vector<Message>{};
    }

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

auto LanggraphSessionStore::operator=(LanggraphSessionStore&&) noexcept -> LanggraphSessionStore& = default;

auto LanggraphSessionStore::open(std::filesystem::path const& path) -> Result<LanggraphSessionStore> {
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }

    auto storage = std::make_shared<mts::agent::SQLiteStorage>(path.string());
    if (auto const status = storage->open(); !status) {
        return Result<LanggraphSessionStore>::err(mts::agent::toFoundation(status.status()).error());
    }

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
        mts::agent::StorageKey{std::string(detail::sessionScope), session.id},
        value
    );
    if (!stored) {
        return Result<SessionInfo>::err(mts::agent::toFoundation(stored.status()).error());
    }

    return session;
}

auto LanggraphSessionStore::listSessions() -> Result<std::vector<SessionInfo>> {
    mts::agent::StorageListOptions options;
    options.scope_ = std::string(detail::sessionScope);
    options.limit_ = 1'000;

    auto const listed = m_impl->storage->list(options);
    if (!listed) {
        return Result<std::vector<SessionInfo>>::err(mts::agent::toFoundation(listed.status()).error());
    }

    std::vector<SessionInfo> sessions;
    sessions.reserve(listed.value().items_.size());
    for (auto const& item : listed.value().items_) {
        auto const parsed = detail::sessionFromJson(mts::agent::json::parse(item.value_, nullptr, false));
        if (!parsed) {
            return parsed.error();
        }

        sessions.push_back(parsed.value());
    }

    std::sort(sessions.begin(), sessions.end(), [](SessionInfo const& left, SessionInfo const& right) {
        return left.updatedAt > right.updatedAt;
    });

    return sessions;
}

auto LanggraphSessionStore::getSession(SessionId const& id) -> Result<SessionInfo> {
    auto const item = m_impl->storage->get(mts::agent::StorageKey{std::string(detail::sessionScope), id});
    if (!item) {
        return Result<SessionInfo>::err(mts::agent::toFoundation(item.status()).error());
    }

    if (!item.value().has_value()) {
        return Result<SessionInfo>::err(PersistenceErrorKind::NotFound, "session not found");
    }

    auto const parsed = detail::sessionFromJson(mts::agent::json::parse(item.value()->value_, nullptr, false));
    if (!parsed) {
        return parsed.error();
    }

    return parsed.value();
}

auto LanggraphSessionStore::renameSession(SessionId const& id, std::string title) -> Status {
    auto const session = getSession(id);
    if (!session) {
        return session.error();
    }

    SessionInfo updated = session.value();
    updated.title = std::move(title);
    updated.updatedAt = detail::nowMillis();

    auto const value = detail::toJson(updated).dump();
    auto const stored = m_impl->storage->put(mts::agent::StorageKey{std::string(detail::sessionScope), id}, value);

    return stored.isOk() ? Status::ok() : mts::agent::toFoundation(stored.status());
}

auto LanggraphSessionStore::saveDraft(SessionId const& id, std::string draft) -> Status {
    auto const session = getSession(id);
    if (!session) {
        return session.error();
    }

    SessionInfo updated = session.value();
    updated.draft = std::move(draft);

    auto const value = detail::toJson(updated).dump();
    auto const stored = m_impl->storage->put(mts::agent::StorageKey{std::string(detail::sessionScope), id}, value);

    return stored.isOk() ? Status::ok() : mts::agent::toFoundation(stored.status());
}

auto LanggraphSessionStore::deleteSession(SessionId const& id) -> Status {
    if (auto const status = clearMessages(id); !status) {
        return status;
    }

    auto const removed = m_impl->storage->remove(mts::agent::StorageKey{std::string(detail::sessionScope), id});

    return removed.isOk() ? Status::ok() : mts::agent::toFoundation(removed.status());
}

auto LanggraphSessionStore::appendMessage(Message message) -> Status {
    auto const session = getSession(message.session);
    if (!session) {
        return session.error();
    }

    if (message.id.empty()) {
        message.id = detail::generateId();
    }

    if (message.timestamp == 0) {
        message.timestamp = detail::nowMillis();
    }

    if (message.sequence == 0) {
        auto const existing = loadMessages(message.session);
        if (!existing) {
            return existing.error();
        }

        std::uint64_t next = 1;
        for (auto const& stored : existing.value()) {
            next = std::max(next, stored.sequence + 1);
        }
        message.sequence = next;
    }

    auto const value = detail::toJson(message).dump();
    auto const stored = m_impl->storage->put(
        mts::agent::StorageKey{std::string(detail::messageScope), detail::messageKey(message.session, message.sequence)},
        value
    );
    if (!stored) {
        return mts::agent::toFoundation(stored.status());
    }

    SessionInfo updated = session.value();
    updated.updatedAt = detail::nowMillis();
    auto const touched = m_impl->storage->put(
        mts::agent::StorageKey{std::string(detail::sessionScope), session.value().id},
        detail::toJson(updated).dump()
    );

    return touched.isOk() ? Status::ok() : mts::agent::toFoundation(touched.status());
}

auto LanggraphSessionStore::loadMessages(SessionId const& id) -> Result<std::vector<Message>> {
    mts::agent::StorageListOptions options;
    options.scope_ = std::string(detail::messageScope);
    options.keyPrefix_ = id + "/";
    options.limit_ = 1'000;

    auto const listed = m_impl->storage->list(options);
    if (!listed) {
        return Result<std::vector<Message>>::err(mts::agent::toFoundation(listed.status()).error());
    }

    std::vector<Message> messages;
    messages.reserve(listed.value().items_.size());
    for (auto const& item : listed.value().items_) {
        auto const parsed = detail::messageFromJson(mts::agent::json::parse(item.value_, nullptr, false));
        if (!parsed) {
            return parsed.error();
        }

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
    if (!listed) {
        return mts::agent::toFoundation(listed.status());
    }

    for (auto const& item : listed.value().items_) {
        auto const removed = m_impl->storage->remove(item.key_);
        if (!removed) {
            return mts::agent::toFoundation(removed.status());
        }
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

auto SessionStoreHandle::saveDraft(SessionId const& id, std::string draft) -> Status {
    return std::visit([&id, &draft](auto& store) {
        return store.saveDraft(id, std::move(draft));
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

auto makeSessionStore(std::filesystem::path const& path) -> Result<SessionStoreHandle> {
    auto store = LanggraphSessionStore::open(path);
    if (!store) {
        return store.error();
    }

    return SessionStoreHandle(std::move(store.value()));
}

} // END OF NAMESPACE `mts::persistence`
