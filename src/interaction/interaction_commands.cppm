export module interaction:commands;

import std;
import :actions;
import :context;
import :layers;
import :model;
import :navigation;
import :registry;
import :selection;

export namespace mts::interaction {

enum class CommandKind : u8 {
    Focus,
    FocusNext,
    FocusPrevious,
    FocusFirst,
    FocusLast,
    FocusDirection,
    Select,
    ClearSelection,
    SelectAll,
    ExtendSelection,
    Activate,
    InvokeAction,
    OpenContextMenu,
    CloseContextMenu,
    PushLayer,
    PopLayer,
};

enum class CommandStatus : u8 {
    Succeeded,
    InvalidRequest,
    InvalidTarget,
    StaleTarget,
    CapabilityDenied,
    LayerBlocked,
    NoFocusableTarget,
    InvalidDirection,
    ActionNotFound,
    ActionOutOfScope,
    ActionHidden,
    ActionDisabled,
    ActionMissingCapability,
    ActionMissingHandler,
    LayerNotFound,
    Unsupported,
    WrongThread,
    DispatchUnavailable,
    FacadeClosed,
};

struct CommandRequest {
    CommandKind kind = CommandKind::Focus;
    TargetId target;
    std::vector<TargetId> targets;
    Domain domain;
    std::string group;
    NavigationDirection direction = NavigationDirection::Down;
    SelectionIntent selectionIntent = SelectionIntent::Replace;
    bool wrap = true;
    bool updateSelection = true;
    std::string actionId;
    std::vector<ActionArgument> arguments;
    Point position;
    LayerDescriptor layer;
    u64 layerId = 0;
};

struct CommandResult {
    CommandStatus status = CommandStatus::Succeeded;
    TargetId target;
    u64 layerId = 0;
    std::string actionId;
    std::string message;

    [[nodiscard]] auto succeeded() const noexcept -> bool {
        return this->status == CommandStatus::Succeeded;
    }
};

struct UiCommandHandlers {
    std::function<bool(TargetId)> focus;
    std::function<bool(TargetId)> activate;
    std::function<bool(TargetId, Point const&)> openContextMenu;
    std::function<bool()> closeContextMenu;
    std::function<void(Domain const&, TargetId)> selectionChanged;
    std::function<void(CommandResult const&)> commandCompleted;
    std::function<std::optional<u64>(LayerDescriptor const&)> pushLayer;
    std::function<bool(u64)> popLayer;
};

class CommandFacade {
public:
    using Dispatch = std::function<bool(std::function<void()>)>;

    CommandFacade(
        TargetRegistry const& registry,
        ContextStore& context,
        SelectionModel& selection,
        LayerStack& layers,
        ActionRegistry& actions,
        NavigationModel& navigation,
        UiCommandHandlers handlers,
        Dispatch dispatch
    )
        : m_registry(registry)
        , m_context(context)
        , m_selection(selection)
        , m_layers(layers)
        , m_actions(actions)
        , m_navigation(navigation)
        , m_handlers(std::move(handlers))
        , m_ownerThread(std::this_thread::get_id())
        , m_state(std::make_shared<State>()) {
        m_state->dispatch = std::move(dispatch);
        m_state->processor = [this](CommandRequest const& request) {
            return this->execute(request);
        };
    }

    ~CommandFacade() {
        std::vector<std::shared_ptr<std::promise<CommandResult>>> pending;
        {
            std::lock_guard const lock(m_state->mutex);
            m_state->alive = false;
            m_state->processor = {};
            m_state->dispatch = {};
            for (auto const& [id, promise] : m_state->pending) {
                pending.push_back(promise);
            }
            m_state->pending.clear();
        }

        for (auto const& promise : pending) {
            promise->set_value(CommandResult{
                CommandStatus::FacadeClosed,
                TargetId::invalid(),
                0,
                {},
                "interaction command facade is closed",
            });
        }
    }

    CommandFacade(CommandFacade const&) = delete;
    auto operator=(CommandFacade const&) -> CommandFacade& = delete;
    CommandFacade(CommandFacade&&) = delete;
    auto operator=(CommandFacade&&) -> CommandFacade& = delete;

    [[nodiscard]] auto execute(CommandRequest request) -> CommandResult {
        if (std::this_thread::get_id() != m_ownerThread) {
            return CommandResult{
                CommandStatus::WrongThread,
                request.target,
                request.layerId,
                request.actionId,
                "synchronous interaction commands must run on the owning GUI thread",
            };
        }

        CommandResult const result = this->executeOnOwner(request);
        if (m_handlers.commandCompleted) {
            m_handlers.commandCompleted(result);
        }

        return result;
    }

    [[nodiscard]] auto post(CommandRequest request) const -> std::future<CommandResult> {
        auto const state = m_state;
        auto promise = std::make_shared<std::promise<CommandResult>>();
        std::future<CommandResult> future = promise->get_future();
        std::uint64_t ticket = 0;
        Dispatch dispatch;

        {
            std::lock_guard const lock(state->mutex);
            if (!state->alive || !state->dispatch) {
                promise->set_value(CommandResult{
                    state->alive ? CommandStatus::DispatchUnavailable : CommandStatus::FacadeClosed,
                    request.target,
                    request.layerId,
                    request.actionId,
                    "interaction command dispatcher is unavailable",
                });

                return future;
            }

            ticket = state->nextTicket++;
            state->pending.emplace(ticket, promise);
            dispatch = state->dispatch;
        }

        bool accepted = false;
        try {
            accepted = dispatch([state, ticket, request = std::move(request)]() mutable {
                std::shared_ptr<std::promise<CommandResult>> promise;
                std::function<CommandResult(CommandRequest const&)> processor;
                {
                    std::lock_guard const lock(state->mutex);
                    auto const found = state->pending.find(ticket);
                    if (found == state->pending.end()) {
                        return;
                    }

                    promise = found->second;
                    state->pending.erase(found);
                    if (state->alive) {
                        processor = state->processor;
                    }
                }

                if (!processor) {
                    promise->set_value(CommandResult{
                        CommandStatus::FacadeClosed,
                        request.target,
                        request.layerId,
                        request.actionId,
                        "interaction command facade is closed",
                    });
                    return;
                }

                promise->set_value(processor(request));
            });
        } catch (...) {
            accepted = false;
        }

        if (!accepted) {
            std::shared_ptr<std::promise<CommandResult>> pending;
            {
                std::lock_guard const lock(state->mutex);
                auto const found = state->pending.find(ticket);
                if (found != state->pending.end()) {
                    pending = found->second;
                    state->pending.erase(found);
                }
            }

            if (pending != nullptr) {
                pending->set_value(CommandResult{
                    CommandStatus::DispatchUnavailable,
                    request.target,
                    request.layerId,
                    request.actionId,
                    "interaction command could not be queued on the owning thread",
                });
            }
        }

        return future;
    }

    [[nodiscard]] auto ownerThread() const noexcept -> std::thread::id {
        return m_ownerThread;
    }

private:
    struct State {
        std::mutex mutex;
        bool alive = true;
        std::uint64_t nextTicket = 1;
        Dispatch dispatch;
        std::function<CommandResult(CommandRequest const&)> processor;
        std::unordered_map<std::uint64_t, std::shared_ptr<std::promise<CommandResult>>> pending;
    };

    [[nodiscard]] auto executeOnOwner(CommandRequest const& request) -> CommandResult {
        switch (request.kind) {
            case CommandKind::Focus: {
                return this->focus(request.target);
            }

            case CommandKind::FocusNext: {
                return this->focusLinear(request, true);
            }

            case CommandKind::FocusPrevious: {
                return this->focusLinear(request, false);
            }

            case CommandKind::FocusFirst: {
                return this->focusEdge(request, true);
            }

            case CommandKind::FocusLast: {
                return this->focusEdge(request, false);
            }

            case CommandKind::FocusDirection: {
                return this->focusDirection(request);
            }

            case CommandKind::Select: {
                return this->select(request);
            }

            case CommandKind::ClearSelection: {
                m_selection.clear(request.domain);
                this->notifySelection(request.domain);
                return CommandResult{CommandStatus::Succeeded};
            }

            case CommandKind::SelectAll: {
                return this->selectAll(request.domain);
            }

            case CommandKind::ExtendSelection: {
                return this->extendSelection(request);
            }

            case CommandKind::Activate: {
                return this->activate(request.target);
            }

            case CommandKind::InvokeAction: {
                return this->invokeAction(request);
            }

            case CommandKind::OpenContextMenu: {
                return this->openContextMenu(request.target, request.position);
            }

            case CommandKind::CloseContextMenu: {
                if (!m_handlers.closeContextMenu || !m_handlers.closeContextMenu()) {
                    return CommandResult{CommandStatus::LayerNotFound, TargetId::invalid(), 0, {}, "no context menu is open"};
                }

                return CommandResult{CommandStatus::Succeeded};
            }

            case CommandKind::PushLayer: {
                if (request.layer.domain.empty()) {
                    return CommandResult{CommandStatus::InvalidRequest, TargetId::invalid(), 0, {}, "layer domain is required"};
                }
                if (!m_handlers.pushLayer) {
                    u64 const id = m_layers.push(request.layer);
                    return CommandResult{CommandStatus::Succeeded, TargetId::invalid(), id};
                }

                auto const id = m_handlers.pushLayer(request.layer);
                if (!id.has_value()) {
                    return CommandResult{CommandStatus::Unsupported, TargetId::invalid(), 0, {}, "layer could not be opened"};
                }

                return CommandResult{CommandStatus::Succeeded, TargetId::invalid(), *id};
            }

            case CommandKind::PopLayer: {
                if (m_handlers.popLayer) {
                    return m_handlers.popLayer(request.layerId)
                        ? CommandResult{CommandStatus::Succeeded, TargetId::invalid(), request.layerId}
                        : CommandResult{CommandStatus::LayerNotFound, TargetId::invalid(), request.layerId, {}, "layer was not found"};
                }

                return m_layers.remove(request.layerId)
                    ? CommandResult{CommandStatus::Succeeded, TargetId::invalid(), request.layerId}
                    : CommandResult{CommandStatus::LayerNotFound, TargetId::invalid(), request.layerId, {}, "layer was not found"};
            }
        }

        return CommandResult{CommandStatus::Unsupported, request.target, request.layerId, request.actionId, "unsupported interaction command"};
    }

    [[nodiscard]] auto focus(TargetId target) -> CommandResult {
        auto const resolved = m_registry.find(target);
        if (!resolved.has_value()) {
            return this->targetFailure(target);
        }

        if (!resolved->capabilities.focusable) {
            return CommandResult{CommandStatus::CapabilityDenied, target, 0, {}, "target is not focusable"};
        }
        if (!this->targetAllowed(target)) {
            return CommandResult{CommandStatus::LayerBlocked, target, 0, {}, "an active layer blocks focus outside its domain"};
        }
        if (!m_handlers.focus || !m_handlers.focus(target)) {
            return CommandResult{CommandStatus::Unsupported, target, 0, {}, "target has no focus handler"};
        }

        return CommandResult{CommandStatus::Succeeded, target};
    }

    [[nodiscard]] auto focusLinear(CommandRequest const& request, bool forward) -> CommandResult {
        TargetId current = m_context.focusedId();
        if (!current.valid()) {
            current = m_navigation.remembered(request.domain).value_or(m_context.currentId());
        }

        auto const target = forward
            ? m_navigation.focusNext(request.domain, current, request.wrap, request.group)
            : m_navigation.focusPrevious(request.domain, current, request.wrap, request.group);
        if (!target.has_value()) {
            return CommandResult{CommandStatus::NoFocusableTarget, current, 0, {}, "focus ring has no next target"};
        }

        return this->focus(*target);
    }

    [[nodiscard]] auto focusEdge(CommandRequest const& request, bool first) -> CommandResult {
        auto const target = first
            ? m_navigation.focusFirst(request.domain, request.group)
            : m_navigation.focusLast(request.domain, request.group);
        if (!target.has_value()) {
            return CommandResult{CommandStatus::NoFocusableTarget, TargetId::invalid(), 0, {}, "focus ring is empty"};
        }

        return this->focus(*target);
    }

    [[nodiscard]] auto focusDirection(CommandRequest const& request) -> CommandResult {
        TargetId current = m_context.focusedId();
        if (!current.valid()) {
            current = m_navigation.remembered(request.domain).value_or(m_context.currentId());
        }

        auto const target = m_navigation.focusDirection(request.domain, current, request.direction, request.group);
        if (!target.has_value()) {
            return CommandResult{CommandStatus::NoFocusableTarget, current, 0, {}, "no focusable target in that direction"};
        }

        auto const resolved = m_registry.find(*target);
        if (!resolved.has_value()) {
            return this->targetFailure(*target);
        }

        CommandResult result = this->focus(*target);
        if (!result.succeeded()) {
            return result;
        }

        if (request.updateSelection && resolved->capabilities.selectable) {
            m_selection.select(resolved->domain, *target, SelectionIntent::Replace);
            this->notifySelection(resolved->domain);
        }

        return result;
    }

    [[nodiscard]] auto select(CommandRequest const& request) -> CommandResult {
        if (request.targets.empty()) {
            return CommandResult{CommandStatus::InvalidTarget, TargetId::invalid(), 0, {}, "selection command has no targets"};
        }

        for (TargetId const target : request.targets) {
            auto const resolved = m_registry.find(target);
            if (!resolved.has_value()) {
                return this->targetFailure(target);
            }
            if (!resolved->capabilities.selectable) {
                return CommandResult{CommandStatus::CapabilityDenied, target, 0, {}, "target is not selectable"};
            }
            if (resolved->domain != request.domain) {
                return CommandResult{CommandStatus::InvalidTarget, target, 0, {}, "target belongs to a different selection domain"};
            }
            if (!this->targetAllowed(target)) {
                return CommandResult{CommandStatus::LayerBlocked, target, 0, {}, "an active layer blocks selection outside its domain"};
            }
        }

        if (request.selectionIntent == SelectionIntent::Replace) {
            m_selection.selectOnly(request.domain, request.targets);
        } else {
            for (TargetId const target : request.targets) {
                m_selection.select(request.domain, target, request.selectionIntent);
            }
        }

        this->notifySelection(request.domain);

        return CommandResult{CommandStatus::Succeeded, request.targets.back()};
    }

    [[nodiscard]] auto selectAll(Domain const& domain) -> CommandResult {
        std::vector<TargetId> targets;
        for (TargetRef const& target : m_registry.targets()) {
            if (target.domain.isWithin(domain)
                && target.capabilities.selectable
                && target.visible
                && target.enabled
                && this->targetAllowed(target.id)) {
                targets.push_back(target.id);
            }
        }

        m_selection.selectAll(domain, targets);
        this->notifySelection(domain);

        return CommandResult{CommandStatus::Succeeded};
    }

    [[nodiscard]] auto extendSelection(CommandRequest const& request) -> CommandResult {
        TargetId current = m_context.focusedId();
        if (!current.valid()) {
            current = m_navigation.remembered(request.domain).value_or(m_context.currentId());
        }

        auto const currentTarget = m_registry.find(current);
        if (!currentTarget.has_value() || !currentTarget->capabilities.selectable) {
            return CommandResult{CommandStatus::CapabilityDenied, current, 0, {}, "current target is not selectable"};
        }

        auto const target = m_navigation.focusDirection(
            request.domain,
            current,
            request.direction,
            request.group.empty() ? std::string_view(currentTarget->group) : std::string_view(request.group)
        );
        if (!target.has_value()) {
            return CommandResult{CommandStatus::NoFocusableTarget, current, 0, {}, "no selectable target in that direction"};
        }

        auto const nextTarget = m_registry.find(*target);
        if (!nextTarget.has_value() || !nextTarget->capabilities.selectable) {
            return CommandResult{CommandStatus::CapabilityDenied, *target, 0, {}, "next target is not selectable"};
        }

        if (!m_selection.anchor(request.domain).valid()) {
            m_selection.setAnchor(request.domain, current);
        }
        m_selection.select(request.domain, *target, SelectionIntent::Range);
        this->notifySelection(request.domain);

        CommandResult result = this->focus(*target);
        result.target = *target;

        return result;
    }

    [[nodiscard]] auto activate(TargetId target) -> CommandResult {
        auto const resolved = m_registry.find(target);
        if (!resolved.has_value()) {
            return this->targetFailure(target);
        }
        if (!resolved->capabilities.activatable) {
            return CommandResult{CommandStatus::CapabilityDenied, target, 0, {}, "target is not activatable"};
        }
        if (!this->targetAllowed(target)) {
            return CommandResult{CommandStatus::LayerBlocked, target, 0, {}, "an active layer blocks activation outside its domain"};
        }
        if (!m_handlers.activate || !m_handlers.activate(target)) {
            return CommandResult{CommandStatus::Unsupported, target, 0, {}, "target has no activation handler"};
        }

        return CommandResult{CommandStatus::Succeeded, target};
    }

    [[nodiscard]] auto invokeAction(CommandRequest const& request) -> CommandResult {
        auto const resolved = m_registry.find(request.target);
        if (!resolved.has_value()) {
            return this->targetFailure(request.target);
        }
        if (!this->targetAllowed(request.target)) {
            return CommandResult{CommandStatus::LayerBlocked, request.target, 0, request.actionId, "an active layer blocks actions outside its domain"};
        }

        ActionContext const context = m_actions.makeContext(*resolved, m_context, m_selection, request.arguments);
        ActionDispatchResult const result = m_actions.invoke(request.actionId, context);
        switch (result.status) {
            case ActionDispatchStatus::Invoked: {
                return CommandResult{CommandStatus::Succeeded, request.target, 0, request.actionId};
            }

            case ActionDispatchStatus::NotFound: {
                return CommandResult{CommandStatus::ActionNotFound, request.target, 0, request.actionId, result.reason};
            }

            case ActionDispatchStatus::OutOfScope: {
                return CommandResult{CommandStatus::ActionOutOfScope, request.target, 0, request.actionId, result.reason};
            }

            case ActionDispatchStatus::MissingCapability: {
                return CommandResult{CommandStatus::ActionMissingCapability, request.target, 0, request.actionId, result.reason};
            }

            case ActionDispatchStatus::Hidden: {
                return CommandResult{CommandStatus::ActionHidden, request.target, 0, request.actionId, result.reason};
            }

            case ActionDispatchStatus::Disabled: {
                return CommandResult{CommandStatus::ActionDisabled, request.target, 0, request.actionId, result.reason};
            }

            case ActionDispatchStatus::MissingHandler: {
                return CommandResult{CommandStatus::ActionMissingHandler, request.target, 0, request.actionId, result.reason};
            }
        }

        return CommandResult{CommandStatus::Unsupported, request.target, 0, request.actionId, "unknown action result"};
    }

    [[nodiscard]] auto openContextMenu(TargetId target, Point const& position) -> CommandResult {
        auto const resolved = m_registry.find(target);
        if (!resolved.has_value()) {
            return this->targetFailure(target);
        }
        if (!resolved->capabilities.contextMenuable) {
            return CommandResult{CommandStatus::CapabilityDenied, target, 0, {}, "target does not support context menus"};
        }
        if (!this->targetAllowed(target)) {
            return CommandResult{CommandStatus::LayerBlocked, target, 0, {}, "an active layer blocks this context menu"};
        }
        if (!m_handlers.openContextMenu || !m_handlers.openContextMenu(target, position)) {
            return CommandResult{CommandStatus::Unsupported, target, 0, {}, "target has no context menu handler"};
        }

        return CommandResult{CommandStatus::Succeeded, target};
    }

    auto notifySelection(Domain const& domain) const -> void {
        if (!m_handlers.selectionChanged) {
            return;
        }

        m_handlers.selectionChanged(domain, m_selection.cursor(domain));
    }

    [[nodiscard]] auto targetAllowed(TargetId target) const -> bool {
        return m_layers.ownsInput(target, m_registry);
    }

    [[nodiscard]] auto targetFailure(TargetId target) const -> CommandResult {
        return CommandResult{
            target.valid() ? CommandStatus::StaleTarget : CommandStatus::InvalidTarget,
            target,
            0,
            {},
            target.valid() ? "target is stale or detached" : "target id is invalid",
        };
    }

    TargetRegistry const& m_registry;
    ContextStore& m_context;
    SelectionModel& m_selection;
    LayerStack& m_layers;
    ActionRegistry& m_actions;
    NavigationModel& m_navigation;
    UiCommandHandlers m_handlers;
    std::thread::id m_ownerThread;
    std::shared_ptr<State> m_state;
};

} // END OF NAMESPACE `mts::interaction`
