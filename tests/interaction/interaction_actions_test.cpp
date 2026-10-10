#include <catch2/catch_test_macros.hpp>

import std;
import interaction;

using namespace mts::interaction;

namespace {

auto addTarget(
    TargetRegistry& registry,
    std::string domain,
    std::string role,
    Capabilities capabilities = {},
    std::string semanticKey = {}
) -> TargetId {
    TargetDescriptor descriptor;
    descriptor.domain = Domain(std::move(domain));
    descriptor.role = Role(std::move(role));
    descriptor.capabilities = capabilities;
    descriptor.semanticKey = std::move(semanticKey);

    return registry.attach(std::move(descriptor));
}

auto action(
    std::string id,
    std::string domain,
    std::string role,
    int order = 0,
    std::vector<ActionSurface> surfaces = {ActionSurface::ContextMenu}
) -> Action {
    Action result;
    result.id = std::move(id);
    result.label = result.id;
    result.scope.domain = Domain(std::move(domain));
    result.scope.role = Role(std::move(role));
    result.order = order;
    result.surfaces = std::move(surfaces);

    return result;
}

auto contextFor(
    TargetRegistry& registry,
    ContextStore& context,
    SelectionModel& selection,
    TargetId target,
    ActionRegistry const& actions,
    std::vector<ActionArgument> arguments = {}
) -> ActionContext {
    return actions.makeContext(*registry.find(target), context, selection, std::move(arguments));
}

}

static_assert(ActionPredicate<decltype([](ActionContext const&) -> bool { return true; })>);
static_assert(ActionHandler<decltype([](ActionContext const&) -> void {})>);
static_assert(!ActionPredicate<int>);

TEST_CASE("action scopes match hierarchical domains and exact roles", "[interaction][actions]") {
    TargetRegistry registry;
    Capabilities capabilities;
    TargetId const row = addTarget(registry, "app.sessionList.row", "session-row", capabilities);
    ContextStore context(registry);
    SelectionModel selection(registry);
    ActionRegistry actions;

    REQUIRE(actions.registerAction(action("session.rename", "app.sessionList", "session-row")));
    REQUIRE(actions.registerAction(action("session.any", "app.sessionList", "")));
    REQUIRE(actions.registerAction(action("app.global", "app", "")));
    REQUIRE(actions.registerAction(action("wrong.role", "app.sessionList", "button")));

    ActionContext const actionContext = contextFor(registry, context, selection, row, actions);
    auto const resolved = actions.resolve(actionContext);

    REQUIRE(resolved.size() == 3);
    REQUIRE(std::find_if(resolved.begin(), resolved.end(), [](ResolvedAction const& entry) {
        return entry.id == "session.rename";
    }) != resolved.end());
    REQUIRE(std::find_if(resolved.begin(), resolved.end(), [](ResolvedAction const& entry) {
        return entry.id == "wrong.role";
    }) == resolved.end());
}

TEST_CASE("declarative capability requirements gate resolved and invoked actions", "[interaction][actions]") {
    TargetRegistry registry;
    TargetId const row = addTarget(registry, "app.sessions", "session-row");
    ContextStore context(registry);
    SelectionModel selection(registry);
    ActionRegistry actions;

    Action rename = action("session.rename", "app.sessions", "session-row");
    rename.requirements.contextMenuable = true;
    rename.policy = ActionPolicy(
        [](ActionContext const&) { return true; },
        [](ActionContext const&) { return true; },
        [](ActionContext const&) {}
    );
    REQUIRE(actions.registerAction(std::move(rename)));

    ActionContext const actionContext = contextFor(registry, context, selection, row, actions);

    REQUIRE(actions.resolve(actionContext).empty());
    REQUIRE_FALSE(actions.isEnabled("session.rename", actionContext));
    REQUIRE(actions.invoke("session.rename", actionContext).status == ActionDispatchStatus::MissingCapability);
}

TEST_CASE("visible and enabled predicates are reevaluated from business context", "[interaction][actions]") {
    TargetRegistry registry;
    Capabilities capabilities;
    capabilities.contextMenuable = true;
    TargetId const row = addTarget(registry, "app.sessions", "session-row", capabilities, "session-42");
    ContextStore context(registry);
    SelectionModel selection(registry);
    ActionRegistry actions;

    Action remove = action("session.remove", "app.sessions", "session-row");
    remove.requirements.contextMenuable = true;
    remove.disabledReason = "The session is currently running.";
    remove.policy = ActionPolicy(
        [](ActionContext const& actionContext) { return !actionContext.target.semanticKey.empty(); },
        [](ActionContext const& actionContext) {
            auto const running = actionContext.businessValue("session.running");
            return !running.has_value() || *running != "true";
        },
        [](ActionContext const&) {}
    );
    REQUIRE(actions.registerAction(std::move(remove)));
    actions.setBusinessAccessor("session.running", [](TargetRef const& target) -> std::optional<std::string> {
        return target.semanticKey == "session-42" ? std::optional<std::string>{"true"} : std::optional<std::string>{"false"};
    });

    ActionContext const runningContext = contextFor(registry, context, selection, row, actions);
    auto const runningActions = actions.resolve(runningContext, ActionSurface::ContextMenu);
    REQUIRE(runningActions.size() == 1);
    REQUIRE_FALSE(runningActions.front().enabled);
    REQUIRE(runningActions.front().disabledReason == "The session is currently running.");
    REQUIRE(actions.invoke("session.remove", runningContext).status == ActionDispatchStatus::Disabled);

    actions.setBusinessValue("override", "unused");
    actions.setBusinessAccessor("session.running", [](TargetRef const&) -> std::optional<std::string> {
        return std::string("false");
    });

    ActionContext const idleContext = contextFor(registry, context, selection, row, actions);
    REQUIRE(actions.isEnabled("session.remove", idleContext));
}

TEST_CASE("predicate policy erases different closure types and dispatches handlers", "[interaction][actions]") {
    TargetRegistry registry;
    Capabilities capabilities;
    capabilities.activatable = true;
    TargetId const target = addTarget(registry, "app.button", "button", capabilities);
    ContextStore context(registry);
    SelectionModel selection(registry);
    ActionRegistry actions;
    int invoked = 0;
    std::string observedKey;
    std::string observedArgument;

    Action activate = action("button.activate", "app.button", "button", 0, {ActionSurface::Button});
    activate.requirements.activatable = true;
    activate.policy = ActionPolicy(
        [threshold = 1](ActionContext const& actionContext) {
            return actionContext.selectionCount() >= static_cast<std::size_t>(threshold - 1);
        },
        [enabled = true](ActionContext const&) { return enabled; },
        [&invoked, &observedKey, &observedArgument](ActionContext const& actionContext) {
            ++invoked;
            observedKey = actionContext.target.semanticKey;
            observedArgument = actionContext.arguments.front().value;
        }
    );
    REQUIRE(actions.registerAction(std::move(activate)));

    TargetDescriptor descriptor;
    descriptor.domain = Domain("app.button");
    descriptor.role = Role("button");
    descriptor.capabilities = capabilities;
    descriptor.semanticKey = "launch";
    registry.rekey(target, std::move(descriptor));

    ActionContext const actionContext = contextFor(
        registry,
        context,
        selection,
        target,
        actions,
        {ActionArgument{"mode", "fast"}}
    );
    REQUIRE(actions.invoke("button.activate", actionContext).invoked());
    REQUIRE(invoked == 1);
    REQUIRE(observedKey == "launch");
    REQUIRE(observedArgument == "fast");
}

TEST_CASE("action resolution is ordered by priority then stable registration order", "[interaction][actions]") {
    TargetRegistry registry;
    TargetId const target = addTarget(registry, "app.list", "row");
    ContextStore context(registry);
    SelectionModel selection(registry);
    ActionRegistry actions;

    REQUIRE(actions.registerAction(action("second", "app.list", "row", 2)));
    REQUIRE(actions.registerAction(action("first-a", "app.list", "row", 1)));
    REQUIRE(actions.registerAction(action("first-b", "app.list", "row", 1)));

    ActionContext const actionContext = contextFor(registry, context, selection, target, actions);
    auto const resolved = actions.resolve(actionContext);

    REQUIRE(resolved.size() == 3);
    REQUIRE(resolved[0].id == "first-a");
    REQUIRE(resolved[1].id == "first-b");
    REQUIRE(resolved[2].id == "second");
}

TEST_CASE("surface selection keeps one action definition available to menus and buttons", "[interaction][actions]") {
    TargetRegistry registry;
    Capabilities capabilities;
    capabilities.activatable = true;
    capabilities.contextMenuable = true;
    TargetId const target = addTarget(registry, "app.prompt", "input", capabilities);
    ContextStore context(registry);
    SelectionModel selection(registry);
    ActionRegistry actions;

    Action send = action("prompt.send", "app.prompt", "input", 0, {ActionSurface::Menu, ActionSurface::Button});
    send.requirements.activatable = true;
    send.policy = ActionPolicy(
        [](ActionContext const&) { return true; },
        [](ActionContext const& actionContext) {
            auto const text = actionContext.businessValue("prompt.text");
            return text.has_value() && !text->empty();
        },
        [](ActionContext const&) {}
    );
    REQUIRE(actions.registerAction(std::move(send)));

    ActionContext actionContext = contextFor(registry, context, selection, target, actions);
    REQUIRE(actions.resolve(actionContext, ActionSurface::Menu).size() == 1);
    REQUIRE(actions.resolve(actionContext, ActionSurface::Button).size() == 1);
    REQUIRE_FALSE(actions.isEnabled("prompt.send", actionContext, ActionSurface::Menu));
    REQUIRE_FALSE(actions.isEnabled("prompt.send", actionContext, ActionSurface::Button));
    REQUIRE_FALSE(actions.isEnabled("prompt.send", actionContext, ActionSurface::ContextMenu));
    REQUIRE_FALSE(actions.isEnabled("prompt.send", actionContext));

    actions.setBusinessValue("prompt.text", "hello");
    actionContext = contextFor(registry, context, selection, target, actions);
    REQUIRE(actions.isEnabled("prompt.send", actionContext, ActionSurface::Menu));
    REQUIRE(actions.isEnabled("prompt.send", actionContext, ActionSurface::Button));
    REQUIRE_FALSE(actions.isEnabled("prompt.send", actionContext, ActionSurface::ContextMenu));
    REQUIRE(actions.isEnabled("prompt.send", actionContext));
}

TEST_CASE("hidden, disabled, out-of-scope and missing actions are guarded", "[interaction][actions]") {
    TargetRegistry registry;
    TargetId const target = addTarget(registry, "app.list", "row");
    ContextStore context(registry);
    SelectionModel selection(registry);
    ActionRegistry actions;

    Action hidden = action("hidden", "app.list", "row");
    hidden.policy = ActionPolicy(
        [](ActionContext const&) { return false; },
        [](ActionContext const&) { return true; },
        [](ActionContext const&) {}
    );
    REQUIRE(actions.registerAction(std::move(hidden)));

    Action disabled = action("disabled", "app.list", "row");
    disabled.policy = ActionPolicy(
        [](ActionContext const&) { return true; },
        [](ActionContext const&) { return false; },
        [](ActionContext const&) {}
    );
    REQUIRE(actions.registerAction(std::move(disabled)));

    Action missingHandler = action("no-handler", "app.list", "row");
    REQUIRE(actions.registerAction(std::move(missingHandler)));

    ActionContext const actionContext = contextFor(registry, context, selection, target, actions);

    auto const resolved = actions.resolve(actionContext);
    REQUIRE(resolved.size() == 2);
    REQUIRE_FALSE(resolved[1].enabled);
    REQUIRE(resolved[1].disabledReason == "action is not configured");
    REQUIRE(actions.invoke("hidden", actionContext).status == ActionDispatchStatus::Hidden);
    REQUIRE(actions.invoke("disabled", actionContext).status == ActionDispatchStatus::Disabled);
    REQUIRE(actions.invoke("no-handler", actionContext).status == ActionDispatchStatus::MissingHandler);
    REQUIRE(actions.invoke("missing", actionContext).status == ActionDispatchStatus::NotFound);

    TargetId const other = addTarget(registry, "app.toolbar", "button");
    ActionContext const otherContext = contextFor(registry, context, selection, other, actions);
    REQUIRE(actions.invoke("disabled", otherContext).status == ActionDispatchStatus::OutOfScope);
}

TEST_CASE("registry indexes and unregisters actions by scope", "[interaction][actions]") {
    ActionRegistry actions;
    REQUIRE(actions.registerAction(action("row.rename", "app.sessions", "session-row")));
    REQUIRE(actions.registerAction(action("row.remove", "app.sessions", "session-row")));
    REQUIRE(actions.registerAction(action("app.about", "app", "window")));

    auto const domainIds = actions.actionIdsInDomain(Domain("app.sessions"));
    auto const roleIds = actions.actionIdsForRole(Role("session-row"));
    REQUIRE(domainIds.size() == 2);
    REQUIRE(roleIds.size() == 2);

    REQUIRE(actions.unregisterAction("row.rename"));
    REQUIRE_FALSE(actions.unregisterAction("row.rename"));
    REQUIRE_FALSE(actions.contains("row.rename"));
    REQUIRE(actions.actionIdsInDomain(Domain("app.sessions")).size() == 1);
}
