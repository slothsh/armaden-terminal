export module interaction:trace;

import std;
import :model;

export namespace mts::interaction {

enum class TraceKind : u8 {
    FocusChanged,
    CurrentChanged,
    HoverChanged,
    Pressed,
    Released,
    ModifiersChanged,
    Clicked,
    SelectionAdded,
    SelectionRemoved,
    SelectionReset,
};

struct TraceEntry {
    std::int64_t timeMs = 0;
    TraceKind kind = TraceKind::FocusChanged;
    TargetId target;
    Domain domain;
    std::string detail;
};

class TraceRing {
public:
    using Clock = std::function<std::int64_t()>;

    explicit TraceRing(std::size_t capacity = 256, Clock clock = nullptr)
        : m_capacity(capacity)
        , m_clock(std::move(clock)) {}

    TraceRing(TraceRing const&) = delete;
    auto operator=(TraceRing const&) -> TraceRing& = delete;
    TraceRing(TraceRing&&) = default;
    auto operator=(TraceRing&&) -> TraceRing& = default;

    auto record(TraceKind kind, TargetId target, Domain const& domain, std::string detail = {}) -> void {
        if (m_capacity == 0) {
            return;
        }

        TraceEntry entry;
        entry.timeMs = this->now();
        entry.kind = kind;
        entry.target = target;
        entry.domain = domain;
        entry.detail = std::move(detail);

        while (m_entries.size() >= m_capacity) {
            m_entries.pop_front();
            m_dropped += 1;
        }

        m_entries.push_back(std::move(entry));
    }

    [[nodiscard]] auto entries(std::size_t limit = 0) const -> std::vector<TraceEntry> {
        if (limit == 0 || limit >= m_entries.size()) {
            return {m_entries.begin(), m_entries.end()};
        }

        return {m_entries.end() - static_cast<std::ptrdiff_t>(limit), m_entries.end()};
    }

    [[nodiscard]] auto size() const -> std::size_t {
        return m_entries.size();
    }

    [[nodiscard]] auto capacity() const -> std::size_t {
        return m_capacity;
    }

    auto setCapacity(std::size_t capacity) -> void {
        m_capacity = capacity;
        while (m_entries.size() > m_capacity) {
            m_entries.pop_front();
            m_dropped += 1;
        }
    }

    [[nodiscard]] auto dropped() const -> std::size_t {
        return m_dropped;
    }

    auto clear() -> void {
        m_entries.clear();
    }

private:
    [[nodiscard]] auto now() const -> std::int64_t {
        if (m_clock != nullptr) {
            return m_clock();
        }

        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    std::size_t m_capacity;
    Clock m_clock;
    std::deque<TraceEntry> m_entries;
    std::size_t m_dropped = 0;
};

} // END OF NAMESPACE `mts::interaction`
