export module interaction:layers;

import std;
import :model;
import :registry;

export namespace mts::interaction {

enum class LayerKind : u8 {
    Popup,
    Dialog,
    Pane,
    ContextMenu,
};

enum class LayerChangeKind : u8 {
    Pushed,
    Popped,
};

struct LayerDescriptor {
    std::string name;
    LayerKind kind = LayerKind::Popup;
    Domain domain;
    TargetId owner;
    TargetId previousFocus;
    TargetId initialFocus;
    bool dismissOnEscape = true;
    bool dismissOnOutsideClick = true;
    bool focusTrap = false;
};

struct LayerEntry {
    u64 id = 0;
    LayerDescriptor descriptor;
};

struct LayerChange {
    LayerChangeKind kind;
    LayerEntry layer;
    std::size_t depth;
};

class LayerStack {
public:
    using Handler = std::function<void(LayerChange const&)>;

    LayerStack() = default;

    LayerStack(LayerStack const&) = delete;
    auto operator=(LayerStack const&) -> LayerStack& = delete;
    LayerStack(LayerStack&&) = delete;
    auto operator=(LayerStack&&) -> LayerStack& = delete;

    [[nodiscard]] auto subscribe(Handler handler) -> std::size_t {
        std::size_t const token = m_nextSubscription++;
        m_handlers.push_back(Observer{token, std::move(handler)});

        return token;
    }

    auto unsubscribe(std::size_t token) -> void {
        std::erase_if(m_handlers, [token](Observer const& observer) {
            return observer.token == token;
        });
    }

    [[nodiscard]] auto push(LayerDescriptor descriptor) -> u64 {
        u64 const id = m_nextId++;
        LayerEntry entry{id, std::move(descriptor)};
        m_layers.push_back(entry);
        this->notify(LayerChange{LayerChangeKind::Pushed, entry, m_layers.size()});

        return id;
    }

    [[nodiscard]] auto pop() -> std::optional<LayerEntry> {
        if (m_layers.empty()) {
            return std::nullopt;
        }

        LayerEntry entry = std::move(m_layers.back());
        m_layers.pop_back();
        this->notify(LayerChange{LayerChangeKind::Popped, entry, m_layers.size()});

        return entry;
    }

    auto remove(u64 id) -> bool {
        auto const found = std::find_if(m_layers.begin(), m_layers.end(), [id](LayerEntry const& entry) {
            return entry.id == id;
        });
        if (found == m_layers.end()) {
            return false;
        }

        LayerEntry entry = std::move(*found);
        m_layers.erase(found);
        this->notify(LayerChange{LayerChangeKind::Popped, entry, m_layers.size()});

        return true;
    }

    auto popTo(u64 id) -> bool {
        auto const found = std::find_if(m_layers.begin(), m_layers.end(), [id](LayerEntry const& entry) {
            return entry.id == id;
        });
        if (found == m_layers.end()) {
            return false;
        }

        std::size_t const keepCount = static_cast<std::size_t>(found - m_layers.begin()) + 1;
        while (m_layers.size() > keepCount) {
            LayerEntry entry = std::move(m_layers.back());
            m_layers.pop_back();
            this->notify(LayerChange{LayerChangeKind::Popped, entry, m_layers.size()});
        }

        return true;
    }

    auto clear() -> void {
        while (!m_layers.empty()) {
            [[maybe_unused]] auto const popped = this->pop();
        }
    }

    [[nodiscard]] auto top() const -> std::optional<LayerEntry> {
        if (m_layers.empty()) {
            return std::nullopt;
        }

        return m_layers.back();
    }

    [[nodiscard]] auto find(u64 id) const -> std::optional<LayerEntry> {
        auto const found = std::find_if(m_layers.begin(), m_layers.end(), [id](LayerEntry const& entry) {
            return entry.id == id;
        });
        if (found == m_layers.end()) {
            return std::nullopt;
        }

        return *found;
    }

    [[nodiscard]] auto depth() const noexcept -> std::size_t {
        return m_layers.size();
    }

    [[nodiscard]] auto empty() const noexcept -> bool {
        return m_layers.empty();
    }

    [[nodiscard]] auto topDomain() const -> std::optional<Domain> {
        auto const layer = this->top();
        if (!layer.has_value()) {
            return std::nullopt;
        }

        return layer->descriptor.domain;
    }

    [[nodiscard]] auto ownsInput(TargetId target, TargetRegistry const& registry) const -> bool {
        auto const layer = this->top();
        if (!layer.has_value() || !layer->descriptor.focusTrap) {
            return true;
        }

        auto const resolved = registry.find(target);
        if (!resolved.has_value()) {
            return false;
        }

        return resolved->domain.isWithin(layer->descriptor.domain);
    }

    [[nodiscard]] auto shouldDismissOnEscape() const -> bool {
        auto const layer = this->top();

        return layer.has_value() && layer->descriptor.dismissOnEscape;
    }

    [[nodiscard]] auto outsideClickDismisses(TargetId target, TargetRegistry const& registry) const -> bool {
        auto const layer = this->top();
        if (!layer.has_value() || !layer->descriptor.dismissOnOutsideClick) {
            return false;
        }

        auto const resolved = registry.find(target);
        if (!resolved.has_value()) {
            return true;
        }

        return !resolved->domain.isWithin(layer->descriptor.domain);
    }

    [[nodiscard]] auto layers() const -> std::span<LayerEntry const> {
        return m_layers;
    }

private:
    struct Observer {
        std::size_t token;
        Handler handler;
    };

    auto notify(LayerChange const& change) const -> void {
        for (Observer const& observer : m_handlers) {
            observer.handler(change);
        }
    }

    std::vector<LayerEntry> m_layers;
    std::vector<Observer> m_handlers;
    u64 m_nextId = 1;
    std::size_t m_nextSubscription = 1;
};

} // END OF NAMESPACE `mts::interaction`
