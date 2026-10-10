export module interaction:navigation;

import std;
import :layers;
import :model;
import :registry;

export namespace mts::interaction {

enum class NavigationDirection : u8 {
    Up,
    Down,
    Left,
    Right,
};

enum class NavigationKey : u8 {
    Tab,
    Enter,
    Space,
    Escape,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    Menu,
    F10,
    A,
};

enum class NavigationCommand : u8 {
    FocusNext,
    FocusPrevious,
    FocusFirst,
    FocusLast,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    ExtendUp,
    ExtendDown,
    ExtendLeft,
    ExtendRight,
    Activate,
    SelectAll,
    ShowContextMenu,
    DismissLayer,
};

struct KeyStroke {
    NavigationKey key;
    Modifiers modifiers;

    [[nodiscard]] auto operator==(KeyStroke const&) const noexcept -> bool = default;
};

struct KeyBinding {
    KeyStroke stroke;
    Domain domain;
    std::optional<LayerKind> layer;
    NavigationCommand command;
    int priority = 0;
};

class KeyRouter {
public:
    KeyRouter() {
        this->installDefaults();
    }

    auto bind(KeyBinding binding) -> void {
        auto const found = std::find_if(m_bindings.begin(), m_bindings.end(), [&binding](KeyBinding const& existing) {
            return existing.stroke == binding.stroke
                && existing.domain == binding.domain
                && existing.layer == binding.layer;
        });
        if (found == m_bindings.end()) {
            m_bindings.push_back(std::move(binding));
        } else {
            *found = std::move(binding);
        }
    }

    auto unbind(KeyStroke stroke, Domain const& domain = Domain{}, std::optional<LayerKind> layer = std::nullopt) -> bool {
        auto const found = std::find_if(m_bindings.begin(), m_bindings.end(), [&stroke, &domain, layer](KeyBinding const& binding) {
            return binding.stroke == stroke && binding.domain == domain && binding.layer == layer;
        });
        if (found == m_bindings.end()) {
            return false;
        }

        m_bindings.erase(found);

        return true;
    }

    [[nodiscard]] auto resolve(
        KeyStroke stroke,
        Domain const& domain,
        std::optional<LayerKind> layer = std::nullopt
    ) const -> std::optional<KeyBinding> {
        KeyBinding const* best = nullptr;
        std::size_t bestDomainDepth = 0;
        int bestLayerPriority = 0;

        for (KeyBinding const& binding : m_bindings) {
            if (binding.stroke != stroke) {
                continue;
            }

            if (!binding.domain.empty() && !domain.isWithin(binding.domain)) {
                continue;
            }

            if (binding.layer.has_value() && binding.layer != layer) {
                continue;
            }

            std::size_t const domainDepth = binding.domain.empty()
                ? 0
                : static_cast<std::size_t>(std::count(
                    binding.domain.path().begin(),
                    binding.domain.path().end(),
                    '.'
                )) + 1;
            int const layerPriority = binding.layer.has_value() ? 1 : 0;
            if (best != nullptr
                && (domainDepth < bestDomainDepth
                    || (domainDepth == bestDomainDepth && layerPriority < bestLayerPriority)
                    || (domainDepth == bestDomainDepth && layerPriority == bestLayerPriority && binding.priority <= best->priority))) {
                continue;
            }

            best = &binding;
            bestDomainDepth = domainDepth;
            bestLayerPriority = layerPriority;
        }

        return best == nullptr ? std::nullopt : std::optional<KeyBinding>{*best};
    }

    [[nodiscard]] auto bindings() const noexcept -> std::span<KeyBinding const> {
        return m_bindings;
    }

private:
    auto installDefaults() -> void {
        this->bind(KeyBinding{KeyStroke{NavigationKey::Tab, {}}, {}, std::nullopt, NavigationCommand::FocusNext});

        Modifiers shift;
        shift.shift = true;
        this->bind(KeyBinding{KeyStroke{NavigationKey::Tab, shift}, {}, std::nullopt, NavigationCommand::FocusPrevious});

        this->bind(KeyBinding{KeyStroke{NavigationKey::ArrowUp, {}}, {}, std::nullopt, NavigationCommand::MoveUp});
        this->bind(KeyBinding{KeyStroke{NavigationKey::ArrowDown, {}}, {}, std::nullopt, NavigationCommand::MoveDown});
        this->bind(KeyBinding{KeyStroke{NavigationKey::ArrowLeft, {}}, {}, std::nullopt, NavigationCommand::MoveLeft});
        this->bind(KeyBinding{KeyStroke{NavigationKey::ArrowRight, {}}, {}, std::nullopt, NavigationCommand::MoveRight});
        this->bind(KeyBinding{KeyStroke{NavigationKey::ArrowUp, shift}, {}, std::nullopt, NavigationCommand::ExtendUp});
        this->bind(KeyBinding{KeyStroke{NavigationKey::ArrowDown, shift}, {}, std::nullopt, NavigationCommand::ExtendDown});
        this->bind(KeyBinding{KeyStroke{NavigationKey::ArrowLeft, shift}, {}, std::nullopt, NavigationCommand::ExtendLeft});
        this->bind(KeyBinding{KeyStroke{NavigationKey::ArrowRight, shift}, {}, std::nullopt, NavigationCommand::ExtendRight});

        Modifiers control;
        control.control = true;
        this->bind(KeyBinding{KeyStroke{NavigationKey::Enter, {}}, {}, std::nullopt, NavigationCommand::Activate});
        this->bind(KeyBinding{KeyStroke{NavigationKey::Enter, control}, {}, std::nullopt, NavigationCommand::Activate});
        this->bind(KeyBinding{KeyStroke{NavigationKey::Space, {}}, {}, std::nullopt, NavigationCommand::Activate});
        this->bind(KeyBinding{KeyStroke{NavigationKey::Escape, {}}, {}, std::nullopt, NavigationCommand::DismissLayer});
        this->bind(KeyBinding{KeyStroke{NavigationKey::Menu, {}}, {}, std::nullopt, NavigationCommand::ShowContextMenu});
        this->bind(KeyBinding{KeyStroke{NavigationKey::F10, shift}, {}, std::nullopt, NavigationCommand::ShowContextMenu});

        this->bind(KeyBinding{KeyStroke{NavigationKey::A, control}, {}, std::nullopt, NavigationCommand::SelectAll});
    }

    std::vector<KeyBinding> m_bindings;
};

class NavigationModel {
public:
    NavigationModel(TargetRegistry const& registry, LayerStack const& layers)
        : m_registry(registry)
        , m_layers(layers) {}

    NavigationModel(NavigationModel const&) = delete;
    auto operator=(NavigationModel const&) -> NavigationModel& = delete;
    NavigationModel(NavigationModel&&) = delete;
    auto operator=(NavigationModel&&) -> NavigationModel& = delete;

    auto remember(Domain const& domain, TargetId target) -> void {
        if (this->isFocusable(target, domain)) {
            m_focusMemory.insert_or_assign(std::string(domain.path()), target);
        }
    }

    [[nodiscard]] auto remembered(Domain const& domain) const -> std::optional<TargetId> {
        auto const found = m_focusMemory.find(std::string(domain.path()));
        if (found == m_focusMemory.end() || !this->isFocusable(found->second, domain)) {
            return std::nullopt;
        }

        return found->second;
    }

    auto forget(TargetId target) -> void {
        std::erase_if(m_focusMemory, [target](auto const& entry) {
            return entry.second == target;
        });
    }

    [[nodiscard]] auto focusRing(Domain const& domain, std::string_view group = {}) const -> std::vector<TargetId> {
        std::vector<TargetRef> candidates;
        for (TargetRef const& target : m_registry.targets()) {
            if (!target.domain.isWithin(domain)
                || !target.capabilities.focusable
                || !target.visible
                || !target.enabled
                || (!group.empty() && target.group != group)
                || !m_layers.ownsInput(target.id, m_registry)) {
                continue;
            }

            candidates.push_back(target);
        }

        std::stable_sort(candidates.begin(), candidates.end(), [](TargetRef const& left, TargetRef const& right) {
            return left.order < right.order;
        });

        std::vector<TargetId> result;
        result.reserve(candidates.size());
        for (TargetRef const& target : candidates) {
            result.push_back(target.id);
        }

        return result;
    }

    [[nodiscard]] auto focusFirst(Domain const& domain, std::string_view group = {}) const -> std::optional<TargetId> {
        std::vector<TargetId> const ring = this->focusRing(domain, group);

        return ring.empty() ? std::nullopt : std::optional<TargetId>{ring.front()};
    }

    [[nodiscard]] auto focusLast(Domain const& domain, std::string_view group = {}) const -> std::optional<TargetId> {
        std::vector<TargetId> const ring = this->focusRing(domain, group);

        return ring.empty() ? std::nullopt : std::optional<TargetId>{ring.back()};
    }

    [[nodiscard]] auto focusNext(
        Domain const& domain,
        TargetId current,
        bool wrap = true,
        std::string_view group = {}
    ) const -> std::optional<TargetId> {
        return this->focusLinear(domain, current, true, wrap, group);
    }

    [[nodiscard]] auto focusPrevious(
        Domain const& domain,
        TargetId current,
        bool wrap = true,
        std::string_view group = {}
    ) const -> std::optional<TargetId> {
        return this->focusLinear(domain, current, false, wrap, group);
    }

    [[nodiscard]] auto focusDirection(
        Domain const& domain,
        TargetId current,
        NavigationDirection direction,
        std::string_view group = {}
    ) const -> std::optional<TargetId> {
        auto const origin = m_registry.find(current);
        if (!origin.has_value()
            || !origin->domain.isWithin(domain)
            || !origin->capabilities.focusable
            || !origin->visible
            || !origin->enabled) {
            return std::nullopt;
        }

        Point const originCenter{
            origin->geometry.x + origin->geometry.width * 0.5,
            origin->geometry.y + origin->geometry.height * 0.5,
        };
        TargetId best;
        int bestOrder = std::numeric_limits<int>::max();
        double bestAlignment = std::numeric_limits<double>::max();
        double bestScore = std::numeric_limits<double>::max();

        for (TargetId const candidateId : this->focusRing(domain, group)) {
            if (candidateId == current) {
                continue;
            }

            auto const candidate = m_registry.find(candidateId);
            if (!candidate.has_value()) {
                continue;
            }

            Point const center{
                candidate->geometry.x + candidate->geometry.width * 0.5,
                candidate->geometry.y + candidate->geometry.height * 0.5,
            };
            double const dx = center.x - originCenter.x;
            double const dy = center.y - originCenter.y;
            double primary = 0.0;
            double secondary = 0.0;
            bool inDirection = false;

            switch (direction) {
                case NavigationDirection::Up: {
                    primary = -dy;
                    secondary = dx;
                    inDirection = dy < 0.0;
                    break;
                }

                case NavigationDirection::Down: {
                    primary = dy;
                    secondary = dx;
                    inDirection = dy > 0.0;
                    break;
                }

                case NavigationDirection::Left: {
                    primary = -dx;
                    secondary = dy;
                    inDirection = dx < 0.0;
                    break;
                }

                case NavigationDirection::Right: {
                    primary = dx;
                    secondary = dy;
                    inDirection = dx > 0.0;
                    break;
                }
            }

            if (!inDirection) {
                continue;
            }

            double const alignment = std::abs(secondary) / primary;
            double const score = primary * primary + secondary * secondary;
            if (alignment < bestAlignment
                || (alignment == bestAlignment && score < bestScore)
                || (alignment == bestAlignment && score == bestScore && candidate->order < bestOrder)) {
                bestAlignment = alignment;
                bestScore = score;
                bestOrder = candidate->order;
                best = candidateId;
            }
        }

        return best.valid() ? std::optional<TargetId>{best} : std::nullopt;
    }

private:
    [[nodiscard]] auto isFocusable(TargetId target, Domain const& domain) const -> bool {
        auto const resolved = m_registry.find(target);

        return resolved.has_value()
            && resolved->domain.isWithin(domain)
            && resolved->capabilities.focusable
            && resolved->visible
            && resolved->enabled
            && m_layers.ownsInput(target, m_registry);
    }

    [[nodiscard]] auto focusLinear(
        Domain const& domain,
        TargetId current,
        bool forward,
        bool wrap,
        std::string_view group
    ) const -> std::optional<TargetId> {
        std::vector<TargetId> const ring = this->focusRing(domain, group);
        if (ring.empty()) {
            return std::nullopt;
        }

        auto const found = std::find(ring.begin(), ring.end(), current);
        if (found == ring.end()) {
            return forward ? std::optional<TargetId>{ring.front()} : std::optional<TargetId>{ring.back()};
        }

        std::size_t const position = static_cast<std::size_t>(found - ring.begin());
        if (forward) {
            if (position + 1 < ring.size()) {
                return ring[position + 1];
            }

            return wrap ? std::optional<TargetId>{ring.front()} : std::nullopt;
        }

        if (position > 0) {
            return ring[position - 1];
        }

        return wrap ? std::optional<TargetId>{ring.back()} : std::nullopt;
    }

    TargetRegistry const& m_registry;
    LayerStack const& m_layers;
    std::unordered_map<std::string, TargetId> m_focusMemory;
};

} // END OF NAMESPACE `mts::interaction`
