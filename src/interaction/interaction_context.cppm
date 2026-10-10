export module interaction:context;

import std;
import :model;
import :registry;

export namespace mts::interaction {

enum class PointerButton : u8 {
    None,
    Primary,
    Secondary,
    Middle,
};

enum class ContextChangeKind : u8 {
    FocusChanged,
    CurrentChanged,
    HoverChanged,
    Pressed,
    Released,
    ModifiersChanged,
    Clicked,
};

struct ContextChange {
    ContextChangeKind kind;
    TargetId target;
    PointerButton button = PointerButton::None;
    Point position;
};

class ContextStore {
public:
    using Handler = std::function<void(ContextChange const&)>;
    using Clock = std::function<std::int64_t()>;

    explicit ContextStore(TargetRegistry const& registry, Clock clock = nullptr)
        : m_registry(registry)
        , m_clock(std::move(clock)) {}

    ContextStore(ContextStore const&) = delete;
    auto operator=(ContextStore const&) -> ContextStore& = delete;
    ContextStore(ContextStore&&) = delete;
    auto operator=(ContextStore&&) -> ContextStore& = delete;

    [[nodiscard]] auto subscribe(Handler handler) -> std::size_t {
        std::size_t const id = m_nextSubscription++;
        m_handlers.push_back(Entry{id, std::move(handler)});

        return id;
    }

    auto unsubscribe(std::size_t id) -> void {
        std::erase_if(m_handlers, [id](Entry const& entry) {
            return entry.id == id;
        });
    }

    auto setFocused(TargetId target) -> void {
        if (m_focused == target) {
            return;
        }

        m_focused = target;
        this->notify(ContextChangeKind::FocusChanged, target, PointerButton::None, this->pointerPosition());
    }

    auto setCurrent(TargetId target) -> void {
        if (m_current == target) {
            return;
        }

        m_current = target;
        this->notify(ContextChangeKind::CurrentChanged, target, PointerButton::None, this->pointerPosition());
    }

    auto setLastClicked(TargetId target) -> void {
        m_lastClicked = target;
    }

    auto setHovered(TargetId target) -> void {
        if (m_hovered == target) {
            return;
        }

        m_hovered = target;
        this->notify(ContextChangeKind::HoverChanged, target, PointerButton::None, this->pointerPosition());
    }

    auto recordPress(PointerButton button, Point position, Modifiers modifiers) -> void {
        m_pointer.x = position.x;
        m_pointer.y = position.y;
        m_pointer.modifiers = modifiers;
        this->applyButton(button, true);

        if (this->isRepeatClick(button, position)) {
            m_clickCount += 1;
        } else {
            m_clickCount = 1;
        }

        m_lastPressTime = this->now();
        m_lastPressPosition = position;
        this->notify(ContextChangeKind::Pressed, m_current, button, position);
    }

    auto recordRelease(PointerButton button, Point position, Modifiers modifiers) -> void {
        m_pointer.x = position.x;
        m_pointer.y = position.y;
        m_pointer.modifiers = modifiers;
        this->applyButton(button, false);
        this->notify(ContextChangeKind::Released, m_current, button, position);
    }

    auto recordMove(Point position, Modifiers modifiers) -> void {
        m_pointer.x = position.x;
        m_pointer.y = position.y;
        m_pointer.modifiers = modifiers;
    }

    auto recordModifiers(Modifiers modifiers) -> void {
        if (m_pointer.modifiers == modifiers) {
            return;
        }

        m_pointer.modifiers = modifiers;
        this->notify(ContextChangeKind::ModifiersChanged, TargetId::invalid(), PointerButton::None, this->pointerPosition());
    }

    auto recordClick(TargetId target, PointerButton button, Point position) -> void {
        this->setCurrent(target);
        m_lastClicked = target;
        this->notify(ContextChangeKind::Clicked, target, button, position);
    }

    [[nodiscard]] auto focused() const -> std::optional<TargetRef> {
        return m_registry.find(m_focused);
    }

    [[nodiscard]] auto current() const -> std::optional<TargetRef> {
        return m_registry.find(m_current);
    }

    [[nodiscard]] auto hovered() const -> std::optional<TargetRef> {
        return m_registry.find(m_hovered);
    }

    [[nodiscard]] auto lastClicked() const -> std::optional<TargetRef> {
        return m_registry.find(m_lastClicked);
    }

    [[nodiscard]] auto focusedId() const noexcept -> TargetId {
        return m_focused;
    }

    [[nodiscard]] auto currentId() const noexcept -> TargetId {
        return m_current;
    }

    [[nodiscard]] auto hoveredId() const noexcept -> TargetId {
        return m_hovered;
    }

    [[nodiscard]] auto lastClickedId() const noexcept -> TargetId {
        return m_lastClicked;
    }

    [[nodiscard]] auto pointer() const noexcept -> PointerState {
        return m_pointer;
    }

    [[nodiscard]] auto modifiers() const noexcept -> Modifiers {
        return m_pointer.modifiers;
    }

    [[nodiscard]] auto lastPressedButton() const noexcept -> PointerButton {
        return m_lastPressedButton;
    }

    [[nodiscard]] auto clickCount() const noexcept -> u32 {
        return m_clickCount;
    }

    [[nodiscard]] auto isWithin(TargetId target, Domain const& domain) const -> bool {
        auto const resolved = m_registry.find(target);
        if (!resolved.has_value()) {
            return false;
        }

        return resolved->domain.isWithin(domain);
    }

    [[nodiscard]] auto domainIsActive(Domain const& domain) const -> bool {
        return this->isWithin(m_focused, domain) || this->isWithin(m_current, domain);
    }

    auto clearAll() -> void {
        m_focused = TargetId::invalid();
        m_current = TargetId::invalid();
        m_hovered = TargetId::invalid();
        m_lastClicked = TargetId::invalid();
        m_lastPressedButton = PointerButton::None;
        m_clickCount = 0;
        m_pointer = PointerState{};
    }

private:
    struct Entry {
        std::size_t id;
        Handler handler;
    };

    [[nodiscard]] auto pointerPosition() const -> Point {
        return Point{m_pointer.x, m_pointer.y};
    }

    [[nodiscard]] auto now() const -> std::int64_t {
        if (m_clock != nullptr) {
            return m_clock();
        }

        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    auto applyButton(PointerButton button, bool pressed) -> void {
        switch (button) {
            case PointerButton::Primary: {
                m_pointer.primaryPressed = pressed;
                break;
            }

            case PointerButton::Secondary: {
                m_pointer.secondaryPressed = pressed;
                break;
            }

            case PointerButton::Middle: {
                m_pointer.middlePressed = pressed;
                break;
            }

            case PointerButton::None: {
                break;
            }
        }

        if (pressed) {
            m_lastPressedButton = button;
        }
    }

    [[nodiscard]] auto isRepeatClick(PointerButton button, Point position) const -> bool {
        constexpr std::int64_t kRepeatIntervalMs = 500;
        constexpr double kRepeatDistance = 4.0;

        if (button != m_lastPressedButton || m_lastPressedButton == PointerButton::None) {
            return false;
        }

        std::int64_t const delta = this->now() - m_lastPressTime;
        if (delta > kRepeatIntervalMs) {
            return false;
        }

        double const dx = position.x - m_lastPressPosition.x;
        double const dy = position.y - m_lastPressPosition.y;

        return dx * dx + dy * dy < kRepeatDistance * kRepeatDistance;
    }

    auto notify(ContextChangeKind kind, TargetId target, PointerButton button, Point position) const -> void {
        ContextChange const change{kind, target, button, position};
        for (Entry const& entry : m_handlers) {
            entry.handler(change);
        }
    }

    TargetRegistry const& m_registry;
    Clock m_clock;
    TargetId m_focused;
    TargetId m_current;
    TargetId m_hovered;
    TargetId m_lastClicked;
    PointerState m_pointer;
    PointerButton m_lastPressedButton = PointerButton::None;
    u32 m_clickCount = 0;
    std::int64_t m_lastPressTime = 0;
    Point m_lastPressPosition;
    std::vector<Entry> m_handlers;
    std::size_t m_nextSubscription = 1;
};

} // END OF NAMESPACE `mts::interaction`
