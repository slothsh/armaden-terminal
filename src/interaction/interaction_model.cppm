export module interaction:model;

import std;

export namespace mts::interaction {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

struct TargetId {
    u32 index = 0;
    u32 generation = 0;

    [[nodiscard]] static constexpr auto invalid() noexcept -> TargetId {
        return TargetId{};
    }

    [[nodiscard]] constexpr auto valid() const noexcept -> bool {
        return this->index != 0;
    }

    [[nodiscard]] constexpr auto packed() const noexcept -> u64 {
        return (static_cast<u64>(this->generation) << 32) | static_cast<u64>(this->index);
    }

    [[nodiscard]] static constexpr auto fromPacked(u64 value) noexcept -> TargetId {
        return TargetId{
            static_cast<u32>(value & 0xffffffffu),
            static_cast<u32>(value >> 32),
        };
    }

    [[nodiscard]] constexpr auto operator==(TargetId const&) const noexcept -> bool = default;
};

class Domain {
public:
    Domain() = default;

    explicit Domain(std::string path)
        : m_path(std::move(path)) {}

    [[nodiscard]] auto path() const noexcept -> std::string_view {
        return this->m_path;
    }

    [[nodiscard]] auto empty() const noexcept -> bool {
        return this->m_path.empty();
    }

    [[nodiscard]] auto child(std::string_view segment) const -> Domain {
        std::string result = this->m_path;
        if (!result.empty() && !segment.empty()) {
            result.push_back('.');
        }

        result.append(segment);

        return Domain(std::move(result));
    }

    [[nodiscard]] auto isWithin(Domain const& ancestor) const -> bool {
        if (ancestor.m_path.empty() || this->m_path == ancestor.m_path) {
            return true;
        }

        if (this->m_path.size() <= ancestor.m_path.size()) {
            return false;
        }

        return this->m_path.compare(0, ancestor.m_path.size(), ancestor.m_path) == 0
            && this->m_path.at(ancestor.m_path.size()) == '.';
    }

    [[nodiscard]] auto operator==(Domain const&) const noexcept -> bool = default;

private:
    std::string m_path;
};

class Role {
public:
    Role() = default;

    explicit Role(std::string name)
        : m_name(std::move(name)) {}

    [[nodiscard]] auto name() const noexcept -> std::string_view {
        return this->m_name;
    }

    [[nodiscard]] auto empty() const noexcept -> bool {
        return this->m_name.empty();
    }

    [[nodiscard]] auto operator==(Role const&) const noexcept -> bool = default;

private:
    std::string m_name;
};

enum class SelectionMode : std::uint8_t {
    None,
    Single,
    Multiple,
};

struct Capabilities {
    bool focusable = false;
    bool selectable = false;
    bool activatable = false;
    bool contextMenuable = false;
    bool navigable = false;
    bool editable = false;
    bool draggable = false;
    bool scrollable = false;
    bool hitTestable = false;
    SelectionMode selection = SelectionMode::None;

    [[nodiscard]] auto operator==(Capabilities const&) const noexcept -> bool = default;
};

struct Modifiers {
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool meta = false;

    [[nodiscard]] auto operator==(Modifiers const&) const noexcept -> bool = default;
};

struct PointerState {
    double x = 0.0;
    double y = 0.0;
    bool primaryPressed = false;
    bool secondaryPressed = false;
    bool middlePressed = false;
    Modifiers modifiers;

    [[nodiscard]] auto operator==(PointerState const&) const noexcept -> bool = default;
};

struct Point {
    double x = 0.0;
    double y = 0.0;

    [[nodiscard]] auto operator==(Point const&) const noexcept -> bool = default;
};

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    [[nodiscard]] auto empty() const noexcept -> bool {
        return this->width <= 0.0 || this->height <= 0.0;
    }

    [[nodiscard]] auto contains(Point const& point) const noexcept -> bool {
        return point.x >= this->x
            && point.y >= this->y
            && point.x < this->x + this->width
            && point.y < this->y + this->height;
    }

    [[nodiscard]] auto operator==(Rect const&) const noexcept -> bool = default;
};

struct TargetDescriptor {
    Domain domain;
    Role role;
    Capabilities capabilities;
    Rect geometry;
    std::string label;
    std::string group;
    int order = 0;
};

struct TargetRef {
    TargetId id;
    Domain domain;
    Role role;
    Capabilities capabilities;
    Rect geometry;
    std::string label;
    std::string group;
    int order = 0;

    [[nodiscard]] auto valid() const noexcept -> bool {
        return this->id.valid();
    }
};

} // END OF NAMESPACE `mts::interaction`
