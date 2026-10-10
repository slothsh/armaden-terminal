export module interaction:actions;

import std;
import :context;
import :model;
import :registry;
import :selection;

export namespace mts::interaction {

enum class ActionSurface : u8 {
    Any,
    ContextMenu,
    Menu,
    Button,
    Shortcut,
};

struct ActionArgument {
    std::string name;
    std::string value;
};

struct ActionContext {
    TargetRef target;
    TargetId focused;
    TargetId current;
    TargetId hovered;
    TargetId lastClicked;
    PointerState pointer;
    std::vector<TargetId> selectedTargets;
    std::vector<ActionArgument> arguments;
    std::unordered_map<std::string, std::string> businessValues;

    [[nodiscard]] auto selected() const -> bool {
        return std::find(this->selectedTargets.begin(), this->selectedTargets.end(), this->target.id)
            != this->selectedTargets.end();
    }

    [[nodiscard]] auto selectionCount() const noexcept -> std::size_t {
        return this->selectedTargets.size();
    }

    [[nodiscard]] auto businessValue(std::string_view name) const -> std::optional<std::string_view> {
        auto const found = this->businessValues.find(std::string(name));
        if (found == this->businessValues.end()) {
            return std::nullopt;
        }

        return found->second;
    }
};

template <typename Predicate>
concept ActionPredicate = std::predicate<Predicate const&, ActionContext const&>;

template <typename Handler>
concept ActionHandler = std::invocable<Handler const&, ActionContext const&>;

class ActionPolicy {
public:
    using Predicate = std::function<bool(ActionContext const&)>;
    using Handler = std::function<void(ActionContext const&)>;

    ActionPolicy()
        : m_visible([](ActionContext const&) { return true; })
        , m_enabled([](ActionContext const&) { return true; }) {}

    template <ActionPredicate Visible, ActionPredicate Enabled, ActionHandler Run>
    ActionPolicy(Visible&& visible, Enabled&& enabled, Run&& run)
        : m_visible(std::forward<Visible>(visible))
        , m_enabled(std::forward<Enabled>(enabled))
        , m_run(std::forward<Run>(run)) {}

    [[nodiscard]] auto visible(ActionContext const& context) const -> bool {
        return !m_visible || m_visible(context);
    }

    [[nodiscard]] auto enabled(ActionContext const& context) const -> bool {
        return !m_enabled || m_enabled(context);
    }

    auto run(ActionContext const& context) const -> void {
        if (m_run) {
            m_run(context);
        }
    }

    [[nodiscard]] auto canRun() const noexcept -> bool {
        return static_cast<bool>(m_run);
    }

private:
    Predicate m_visible;
    Predicate m_enabled;
    Handler m_run;
};

struct ActionScope {
    Domain domain;
    Role role;

    [[nodiscard]] auto matches(TargetRef const& target) const -> bool {
        return target.domain.isWithin(this->domain)
            && (this->role.empty() || this->role == target.role);
    }
};

struct ActionRequirements {
    bool focusable = false;
    bool selectable = false;
    bool activatable = false;
    bool contextMenuable = false;
    bool navigable = false;
    bool editable = false;
    bool draggable = false;
    bool scrollable = false;
    bool hitTestable = false;

    [[nodiscard]] auto satisfiedBy(Capabilities const& capabilities) const noexcept -> bool {
        return (!this->focusable || capabilities.focusable)
            && (!this->selectable || capabilities.selectable)
            && (!this->activatable || capabilities.activatable)
            && (!this->contextMenuable || capabilities.contextMenuable)
            && (!this->navigable || capabilities.navigable)
            && (!this->editable || capabilities.editable)
            && (!this->draggable || capabilities.draggable)
            && (!this->scrollable || capabilities.scrollable)
            && (!this->hitTestable || capabilities.hitTestable);
    }
};

struct Action {
    std::string id;
    std::string label;
    std::string group;
    std::string icon;
    std::string shortcut;
    std::string disabledReason;
    ActionScope scope;
    ActionRequirements requirements;
    ActionPolicy policy;
    std::vector<ActionSurface> surfaces;
    int order = 0;
};

struct ResolvedAction {
    std::string id;
    std::string label;
    std::string group;
    std::string icon;
    std::string shortcut;
    std::string disabledReason;
    bool enabled = false;
    int order = 0;
};

enum class ActionDispatchStatus : u8 {
    Invoked,
    NotFound,
    OutOfScope,
    MissingCapability,
    Hidden,
    Disabled,
    MissingHandler,
};

struct ActionDispatchResult {
    ActionDispatchStatus status = ActionDispatchStatus::NotFound;
    std::string actionId;
    std::string reason;

    [[nodiscard]] auto invoked() const noexcept -> bool {
        return this->status == ActionDispatchStatus::Invoked;
    }
};

class ActionRegistry {
public:
    using BusinessAccessor = std::function<std::optional<std::string>(TargetRef const&)>;

    ActionRegistry() = default;

    ActionRegistry(ActionRegistry const&) = delete;
    auto operator=(ActionRegistry const&) -> ActionRegistry& = delete;
    ActionRegistry(ActionRegistry&&) = delete;
    auto operator=(ActionRegistry&&) -> ActionRegistry& = delete;

    auto registerAction(Action action) -> bool {
        if (action.id.empty()) {
            return false;
        }

        auto const found = m_actions.find(action.id);
        u64 sequence = m_nextSequence++;
        if (found != m_actions.end()) {
            sequence = found->second.sequence;
            this->removeIndexes(found->second.action);
        }

        RegisteredAction registered{std::move(action), sequence};
        this->addIndexes(registered.action);
        std::string const id = registered.action.id;
        m_actions.insert_or_assign(id, std::move(registered));

        return true;
    }

    auto unregisterAction(std::string_view id) -> bool {
        auto const found = m_actions.find(std::string(id));
        if (found == m_actions.end()) {
            return false;
        }

        this->removeIndexes(found->second.action);
        m_actions.erase(found);

        return true;
    }

    [[nodiscard]] auto contains(std::string_view id) const -> bool {
        return m_actions.contains(std::string(id));
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return m_actions.size();
    }

    auto setBusinessValue(std::string key, std::string value) -> void {
        m_businessValues.insert_or_assign(std::move(key), std::move(value));
    }

    auto removeBusinessValue(std::string_view key) -> void {
        m_businessValues.erase(std::string(key));
    }

    auto setBusinessAccessor(std::string key, BusinessAccessor accessor) -> void {
        m_businessAccessors.insert_or_assign(std::move(key), std::move(accessor));
    }

    auto removeBusinessAccessor(std::string_view key) -> void {
        m_businessAccessors.erase(std::string(key));
    }

    [[nodiscard]] auto makeContext(
        TargetRef target,
        ContextStore const& context,
        SelectionModel& selection,
        std::vector<ActionArgument> arguments = {}
    ) const -> ActionContext {
        ActionContext result;
        result.target = std::move(target);
        result.focused = context.focusedId();
        result.current = context.currentId();
        result.hovered = context.hoveredId();
        result.lastClicked = context.lastClickedId();
        result.pointer = context.pointer();
        result.selectedTargets = selection.ordered(result.target.domain);
        result.arguments = std::move(arguments);
        result.businessValues = m_businessValues;

        for (auto const& [key, accessor] : m_businessAccessors) {
            if (auto const value = accessor(result.target); value.has_value()) {
                result.businessValues.insert_or_assign(key, *value);
            }
        }

        return result;
    }

    [[nodiscard]] auto resolve(ActionContext const& context, ActionSurface surface = ActionSurface::Any) const -> std::vector<ResolvedAction> {
        std::vector<RegisteredAction const*> candidates = this->candidates(context.target);
        std::vector<RegisteredAction const*> applicable;
        applicable.reserve(candidates.size());

        if (surface == ActionSurface::ContextMenu && !context.target.capabilities.contextMenuable) {
            return {};
        }

        for (RegisteredAction const* registered : candidates) {
            Action const& action = registered->action;
            if (!action.scope.matches(context.target)
                || !action.requirements.satisfiedBy(context.target.capabilities)
                || !this->hasSurface(action, surface)
                || !action.policy.visible(context)) {
                continue;
            }

            applicable.push_back(registered);
        }

        std::sort(applicable.begin(), applicable.end(), [](RegisteredAction const* left, RegisteredAction const* right) {
            if (left->action.order != right->action.order) {
                return left->action.order < right->action.order;
            }

            return left->sequence < right->sequence;
        });

        std::vector<ResolvedAction> result;
        result.reserve(applicable.size());
        for (RegisteredAction const* registered : applicable) {
            Action const& action = registered->action;
            bool const enabled = action.policy.canRun() && action.policy.enabled(context);
            std::string const disabledReason = !action.policy.canRun()
                ? "action is not configured"
                : (enabled ? std::string{} : action.disabledReason);
            result.push_back(ResolvedAction{
                action.id,
                action.label,
                action.group,
                action.icon,
                action.shortcut,
                disabledReason,
                enabled,
                action.order,
            });
        }

        return result;
    }

    [[nodiscard]] auto isEnabled(
        std::string_view id,
        ActionContext const& context,
        ActionSurface surface = ActionSurface::Any
    ) const -> bool {
        Action const* action = this->findAction(id);
        return action != nullptr
            && action->scope.matches(context.target)
            && action->requirements.satisfiedBy(context.target.capabilities)
            && this->hasSurface(*action, surface)
            && (surface != ActionSurface::ContextMenu || context.target.capabilities.contextMenuable)
            && action->policy.visible(context)
            && action->policy.canRun()
            && action->policy.enabled(context);
    }

    [[nodiscard]] auto invoke(std::string_view id, ActionContext const& context) const -> ActionDispatchResult {
        Action const* action = this->findAction(id);
        if (action == nullptr) {
            return ActionDispatchResult{ActionDispatchStatus::NotFound, std::string(id), "unknown action"};
        }

        if (!action->scope.matches(context.target)) {
            return ActionDispatchResult{ActionDispatchStatus::OutOfScope, action->id, "action does not apply to this target"};
        }

        if (!action->requirements.satisfiedBy(context.target.capabilities)) {
            return ActionDispatchResult{ActionDispatchStatus::MissingCapability, action->id, "target lacks a required capability"};
        }

        if (!action->policy.visible(context)) {
            return ActionDispatchResult{ActionDispatchStatus::Hidden, action->id, "action is not visible in the current context"};
        }

        if (!action->policy.enabled(context)) {
            return ActionDispatchResult{ActionDispatchStatus::Disabled, action->id, action->disabledReason};
        }

        if (!action->policy.canRun()) {
            return ActionDispatchResult{ActionDispatchStatus::MissingHandler, action->id, "action has no handler"};
        }

        action->policy.run(context);

        return ActionDispatchResult{ActionDispatchStatus::Invoked, action->id, {}};
    }

    [[nodiscard]] auto actionIdsInDomain(Domain const& domain) const -> std::vector<std::string> {
        auto const found = m_byDomain.find(std::string(domain.path()));
        if (found == m_byDomain.end()) {
            return {};
        }

        std::vector<std::string> result(found->second.begin(), found->second.end());
        std::sort(result.begin(), result.end());

        return result;
    }

    [[nodiscard]] auto actionIdsForRole(Role const& role) const -> std::vector<std::string> {
        auto const found = m_byRole.find(std::string(role.name()));
        if (found == m_byRole.end()) {
            return {};
        }

        std::vector<std::string> result(found->second.begin(), found->second.end());
        std::sort(result.begin(), result.end());

        return result;
    }

private:
    struct RegisteredAction {
        Action action;
        u64 sequence;
    };

    [[nodiscard]] auto findAction(std::string_view id) const -> Action const* {
        auto const found = m_actions.find(std::string(id));
        if (found == m_actions.end()) {
            return nullptr;
        }

        return &found->second.action;
    }

    [[nodiscard]] auto candidates(TargetRef const& target) const -> std::vector<RegisteredAction const*> {
        std::unordered_set<std::string> domainActions;
        std::string path(target.domain.path());

        while (true) {
            if (auto const found = m_byDomain.find(path); found != m_byDomain.end()) {
                domainActions.insert(found->second.begin(), found->second.end());
            }

            if (path.empty()) {
                break;
            }

            std::size_t const separator = path.find_last_of('.');
            if (separator == std::string::npos) {
                path.clear();
            } else {
                path.erase(separator);
            }
        }

        auto const roles = m_byRole.find(std::string(target.role.name()));
        auto const anyRole = m_byRole.find({});
        std::vector<RegisteredAction const*> result;
        for (std::string const& id : domainActions) {
            bool const roleMatches = roles != m_byRole.end() && roles->second.contains(id);
            bool const anyRoleMatches = anyRole != m_byRole.end() && anyRole->second.contains(id);
            if (!roleMatches && !anyRoleMatches) {
                continue;
            }

            auto const found = m_actions.find(id);
            if (found != m_actions.end()) {
                result.push_back(&found->second);
            }
        }

        return result;
    }

    [[nodiscard]] auto hasSurface(Action const& action, ActionSurface surface) const -> bool {
        return surface == ActionSurface::Any
            || action.surfaces.empty()
            || std::find(action.surfaces.begin(), action.surfaces.end(), surface) != action.surfaces.end();
    }

    auto addIndexes(Action const& action) -> void {
        m_byDomain[std::string(action.scope.domain.path())].insert(action.id);
        m_byRole[std::string(action.scope.role.name())].insert(action.id);
    }

    auto removeIndexes(Action const& action) -> void {
        auto const domain = m_byDomain.find(std::string(action.scope.domain.path()));
        if (domain != m_byDomain.end()) {
            domain->second.erase(action.id);
            if (domain->second.empty()) {
                m_byDomain.erase(domain);
            }
        }

        auto const role = m_byRole.find(std::string(action.scope.role.name()));
        if (role != m_byRole.end()) {
            role->second.erase(action.id);
            if (role->second.empty()) {
                m_byRole.erase(role);
            }
        }
    }

    std::unordered_map<std::string, RegisteredAction> m_actions;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_byDomain;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_byRole;
    std::unordered_map<std::string, std::string> m_businessValues;
    std::unordered_map<std::string, BusinessAccessor> m_businessAccessors;
    u64 m_nextSequence = 1;
};

} // END OF NAMESPACE `mts::interaction`
