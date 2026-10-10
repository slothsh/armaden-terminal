#include <catch2/catch_test_macros.hpp>

import std;
import interaction;

using namespace mts::interaction;

namespace {

struct Fixture {
    TargetRegistry registry;
    ContextStore context{registry};
    SelectionModel selection{registry};
    LayerStack layers;
    NavigationModel navigation{registry, layers};
    ActionRegistry actions;
    TargetId focused;
    TargetId activated;
    TargetId contextTarget;
    Point contextPosition;
    bool contextClosed = false;
    u64 pushedLayer = 0;
    u64 poppedLayer = 0;
    Domain notifiedSelectionDomain;
    TargetId notifiedSelectionCursor;
    int selectionNotifications = 0;
    std::vector<CommandStatus> completedCommands;
    UiCommandHandlers handlers;

    Fixture() {
        this->handlers.focus = [this](TargetId target) {
            this->focused = target;
            this->context.setFocused(target);

            return true;
        };
        this->handlers.activate = [this](TargetId target) {
            this->activated = target;

            return true;
        };
        this->handlers.openContextMenu = [this](TargetId target, Point const& position) {
            this->contextTarget = target;
            this->contextPosition = position;

            return true;
        };
        this->handlers.closeContextMenu = [this] {
            this->contextClosed = true;

            return true;
        };
        this->handlers.selectionChanged = [this](Domain const& domain, TargetId cursor) {
            this->notifiedSelectionDomain = domain;
            this->notifiedSelectionCursor = cursor;
            this->selectionNotifications += 1;
        };
        this->handlers.commandCompleted = [this](CommandResult const& result) {
            this->completedCommands.push_back(result.status);
        };
        this->handlers.pushLayer = [this](LayerDescriptor const& descriptor) -> std::optional<u64> {
            this->pushedLayer = this->layers.push(descriptor);

            return this->pushedLayer;
        };
        this->handlers.popLayer = [this](u64 layer) {
            this->poppedLayer = layer;

            return this->layers.remove(layer);
        };
    }

    [[nodiscard]] auto attach(
        std::string domain,
        std::string role,
        Capabilities capabilities,
        int order = 0,
        std::string semanticKey = {},
        Rect geometry = Rect{}
    ) -> TargetId {
        TargetDescriptor descriptor;
        descriptor.domain = Domain(std::move(domain));
        descriptor.role = Role(std::move(role));
        descriptor.capabilities = capabilities;
        descriptor.order = order;
        descriptor.semanticKey = std::move(semanticKey);
        descriptor.geometry = geometry;

        return this->registry.attach(std::move(descriptor));
    }

    [[nodiscard]] auto facade(CommandFacade::Dispatch dispatch = [](std::function<void()> command) {
        command();

        return true;
    }) -> CommandFacade {
        return CommandFacade(
            this->registry,
            this->context,
            this->selection,
            this->layers,
            this->actions,
            this->navigation,
            this->handlers,
            std::move(dispatch)
        );
    }
};

}

TEST_CASE("command facade focuses targets and reports validation failures", "[interaction][commands]") {
    Fixture fixture;
    Capabilities capabilities;
    capabilities.focusable = true;
    TargetId const target = fixture.attach("app.list", "row", capabilities);
    CommandFacade facade = fixture.facade();

    CommandRequest request;
    request.kind = CommandKind::Focus;
    request.target = target;
    CommandResult const result = facade.execute(request);

    REQUIRE(result.succeeded());
    REQUIRE(fixture.focused == target);
    REQUIRE(fixture.context.focusedId() == target);

    request.target = TargetId{999, 3};
    REQUIRE(facade.execute(request).status == CommandStatus::StaleTarget);

    request.target = TargetId::invalid();
    REQUIRE(facade.execute(request).status == CommandStatus::InvalidTarget);
}

TEST_CASE("command facade enforces target capabilities and layer traps", "[interaction][commands]") {
    Fixture fixture;
    TargetId const row = fixture.attach("app.list", "row", Capabilities{});
    CommandFacade facade = fixture.facade();

    CommandRequest focus;
    focus.kind = CommandKind::Focus;
    focus.target = row;
    REQUIRE(facade.execute(focus).status == CommandStatus::CapabilityDenied);

    Capabilities focusable;
    focusable.focusable = true;
    TargetId const outside = fixture.attach("app.toolbar", "button", focusable);
    [[maybe_unused]] u64 const modal = fixture.layers.push(LayerDescriptor{
        .name = "modal",
        .domain = Domain("app.dialog"),
        .focusTrap = true,
    });

    focus.target = outside;
    REQUIRE(facade.execute(focus).status == CommandStatus::LayerBlocked);
}

TEST_CASE("selection commands funnel through the per-domain selection model", "[interaction][commands]") {
    Fixture fixture;
    Capabilities selectable;
    selectable.focusable = true;
    selectable.selectable = true;
    selectable.selection = SelectionMode::Multiple;
    TargetId const first = fixture.attach("app.sessions", "row", selectable, 0);
    TargetId const second = fixture.attach("app.sessions", "row", selectable, 1);
    CommandFacade facade = fixture.facade();

    CommandRequest request;
    request.kind = CommandKind::Select;
    request.domain = Domain("app.sessions");
    request.targets = {first};
    request.selectionIntent = SelectionIntent::Replace;
    REQUIRE(facade.execute(request).succeeded());

    request.targets = {second};
    request.selectionIntent = SelectionIntent::Add;
    REQUIRE(facade.execute(request).succeeded());
    REQUIRE(fixture.selection.ordered(request.domain) == std::vector<TargetId>{first, second});
    REQUIRE(fixture.notifiedSelectionDomain == request.domain);
    REQUIRE(fixture.notifiedSelectionCursor == second);
    REQUIRE(fixture.selectionNotifications == 2);

    request.kind = CommandKind::ClearSelection;
    REQUIRE(facade.execute(request).succeeded());
    REQUIRE(fixture.selection.count(Domain("app.sessions")) == 0);

    request.kind = CommandKind::SelectAll;
    REQUIRE(facade.execute(request).succeeded());
    REQUIRE(fixture.selection.count(Domain("app.sessions")) == 2);
}

TEST_CASE("navigation commands share focus and selection state", "[interaction][commands]") {
    Fixture fixture;
    Capabilities selectable;
    selectable.focusable = true;
    selectable.selectable = true;
    TargetId const first = fixture.attach("app.sessions", "row", selectable, 0, {}, Rect{0, 0, 10, 10});
    TargetId const second = fixture.attach("app.sessions", "row", selectable, 1, {}, Rect{0, 20, 10, 10});
    CommandFacade facade = fixture.facade();

    CommandRequest request;
    request.kind = CommandKind::FocusFirst;
    request.domain = Domain("app.sessions");
    REQUIRE(facade.execute(request).succeeded());
    REQUIRE(fixture.focused == first);

    request.kind = CommandKind::FocusDirection;
    request.direction = NavigationDirection::Down;
    REQUIRE(facade.execute(request).succeeded());
    REQUIRE(fixture.focused == second);
    REQUIRE(fixture.selection.ordered(Domain("app.sessions")) == std::vector<TargetId>{second});

    request.kind = CommandKind::ExtendSelection;
    request.direction = NavigationDirection::Up;
    REQUIRE(facade.execute(request).succeeded());
    REQUIRE(fixture.selection.ordered(Domain("app.sessions")) == std::vector<TargetId>{first, second});
}

TEST_CASE("activation, actions and context menus return structured statuses", "[interaction][commands]") {
    Fixture fixture;
    Capabilities capabilities;
    capabilities.activatable = true;
    capabilities.contextMenuable = true;
    TargetId const target = fixture.attach("app.list", "row", capabilities, 0, "row-key", Rect{10, 20, 30, 40});
    CommandFacade facade = fixture.facade();

    CommandRequest activate;
    activate.kind = CommandKind::Activate;
    activate.target = target;
    REQUIRE(facade.execute(activate).succeeded());
    REQUIRE(fixture.activated == target);

    Action registered;
    registered.id = "row.inspect";
    registered.label = "Inspect";
    registered.scope.domain = Domain("app.list");
    registered.scope.role = Role("row");
    registered.policy = ActionPolicy(
        [](ActionContext const&) { return true; },
        [](ActionContext const&) { return true; },
        [](ActionContext const&) {}
    );
    REQUIRE(fixture.actions.registerAction(std::move(registered)));

    CommandRequest menu;
    menu.kind = CommandKind::OpenContextMenu;
    menu.target = target;
    menu.position = Point{12, 24};
    REQUIRE(facade.execute(menu).succeeded());
    REQUIRE(fixture.contextTarget == target);
    REQUIRE(fixture.contextPosition == Point{12, 24});

    menu.kind = CommandKind::CloseContextMenu;
    REQUIRE(facade.execute(menu).succeeded());
    REQUIRE(fixture.contextClosed);

    CommandRequest invoke;
    invoke.kind = CommandKind::InvokeAction;
    invoke.target = target;
    invoke.actionId = "row.inspect";
    REQUIRE(facade.execute(invoke).succeeded());

    invoke.actionId = "row.missing";
    REQUIRE(facade.execute(invoke).status == CommandStatus::ActionNotFound);
}

TEST_CASE("layer commands report ids and missing layers", "[interaction][commands]") {
    Fixture fixture;
    CommandFacade facade = fixture.facade();

    CommandRequest push;
    push.kind = CommandKind::PushLayer;
    push.layer.name = "dialog";
    push.layer.domain = Domain("app.dialog");
    CommandResult const opened = facade.execute(push);

    REQUIRE(opened.succeeded());
    REQUIRE(opened.layerId == fixture.pushedLayer);
    REQUIRE(fixture.layers.depth() == 1);

    CommandRequest pop;
    pop.kind = CommandKind::PopLayer;
    pop.layerId = opened.layerId;
    REQUIRE(facade.execute(pop).succeeded());
    REQUIRE(fixture.poppedLayer == opened.layerId);
    REQUIRE(fixture.layers.empty());
    REQUIRE(facade.execute(pop).status == CommandStatus::LayerNotFound);
}

TEST_CASE("post marshals backend commands through the configured executor", "[interaction][commands]") {
    Fixture fixture;
    std::vector<std::function<void()>> queued;
    CommandFacade facade = fixture.facade([&queued](std::function<void()> command) {
        queued.push_back(std::move(command));

        return true;
    });
    Capabilities focusable;
    focusable.focusable = true;
    TargetId const target = fixture.attach("app.list", "row", focusable);

    CommandRequest request;
    request.kind = CommandKind::Focus;
    request.target = target;
    std::future<CommandResult> future = facade.post(request);

    REQUIRE(future.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout);
    REQUIRE(queued.size() == 1);
    queued.front()();
    REQUIRE(future.get().succeeded());
    REQUIRE(fixture.focused == target);
    REQUIRE(fixture.completedCommands.back() == CommandStatus::Succeeded);
}

TEST_CASE("synchronous commands reject non-owner threads and queue rejection is reported", "[interaction][commands]") {
    Fixture fixture;
    CommandFacade facade = fixture.facade([](std::function<void()>) { return false; });

    CommandRequest request;
    request.kind = CommandKind::ClearSelection;
    request.domain = Domain("app.sessions");
    CommandStatus workerStatus = CommandStatus::Succeeded;
    std::thread worker([&facade, &request, &workerStatus] {
        workerStatus = facade.execute(request).status;
    });
    worker.join();

    REQUIRE(workerStatus == CommandStatus::WrongThread);
    REQUIRE(facade.post(request).get().status == CommandStatus::DispatchUnavailable);
}

TEST_CASE("pending commands are completed as cancelled when the facade closes", "[interaction][commands]") {
    Fixture fixture;
    std::vector<std::function<void()>> queued;
    std::future<CommandResult> future;
    {
        CommandFacade facade = fixture.facade([&queued](std::function<void()> command) {
            queued.push_back(std::move(command));

            return true;
        });
        CommandRequest request;
        request.kind = CommandKind::ClearSelection;
        request.domain = Domain("app.sessions");
        future = facade.post(request);
    }

    REQUIRE(future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
    REQUIRE(future.get().status == CommandStatus::FacadeClosed);
    queued.front()();
}

TEST_CASE("dispatching an action can open a new layer before closing its menu target", "[interaction][commands]") {
    Fixture fixture;
    Capabilities capabilities;
    capabilities.activatable = true;
    capabilities.contextMenuable = true;
    TargetId const menuTarget = fixture.attach("app.appMenu", "layer-content", capabilities);
    LayerDescriptor menu;
    menu.name = "app-menu";
    menu.kind = LayerKind::Popup;
    menu.domain = Domain("app.appMenu");
    u64 const menuLayer = fixture.layers.push(menu);

    Action about;
    about.id = "app.about";
    about.scope.domain = Domain("app.appMenu");
    about.scope.role = Role("layer-content");
    about.requirements.activatable = true;
    about.policy = ActionPolicy(
        [](ActionContext const&) { return true; },
        [](ActionContext const&) { return true; },
        [&fixture](ActionContext const&) {
            LayerDescriptor dialog;
            dialog.name = "about";
            dialog.kind = LayerKind::Dialog;
            dialog.domain = Domain("app.dialog");
            dialog.focusTrap = true;
            [[maybe_unused]] u64 const layer = fixture.layers.push(dialog);
        }
    );
    REQUIRE(fixture.actions.registerAction(std::move(about)));

    CommandFacade facade = fixture.facade();
    CommandRequest invoke;
    invoke.kind = CommandKind::InvokeAction;
    invoke.target = menuTarget;
    invoke.actionId = "app.about";

    REQUIRE(facade.execute(invoke).succeeded());
    REQUIRE(fixture.layers.depth() == 2);
    REQUIRE(fixture.layers.top()->descriptor.domain == Domain("app.dialog"));

    TargetDescriptor hidden;
    hidden.domain = Domain("app.appMenu");
    hidden.role = Role("layer-content");
    hidden.capabilities = capabilities;
    hidden.visible = false;
    fixture.registry.rekey(menuTarget, std::move(hidden));
    REQUIRE(fixture.layers.remove(menuLayer));
    REQUIRE(fixture.layers.depth() == 1);
    REQUIRE(fixture.layers.top()->descriptor.domain == Domain("app.dialog"));
}
