#include <catch2/catch_test_macros.hpp>

import std;
import interaction;

using namespace mts::interaction;

namespace {

auto addFocusable(
    TargetRegistry& registry,
    std::string domain,
    std::string role,
    int order,
    Rect geometry,
    std::string group = {},
    bool visible = true,
    bool enabled = true
) -> TargetId {
    TargetDescriptor descriptor;
    descriptor.domain = Domain(std::move(domain));
    descriptor.role = Role(std::move(role));
    descriptor.order = order;
    descriptor.geometry = geometry;
    descriptor.group = std::move(group);
    descriptor.visible = visible;
    descriptor.enabled = enabled;
    descriptor.capabilities.focusable = true;
    descriptor.capabilities.navigable = true;

    return registry.attach(std::move(descriptor));
}

auto key(NavigationKey value, Modifiers modifiers = {}) -> KeyStroke {
    return KeyStroke{value, modifiers};
}

}

TEST_CASE("focus rings use explicit order then registration order", "[interaction][navigation]") {
    TargetRegistry registry;
    LayerStack layers;
    NavigationModel navigation(registry, layers);

    TargetId const third = addFocusable(registry, "app.list", "row", 3, Rect{0, 20, 10, 10});
    TargetId const first = addFocusable(registry, "app.list", "row", 1, Rect{0, 0, 10, 10});
    TargetId const second = addFocusable(registry, "app.list", "row", 1, Rect{0, 10, 10, 10});

    REQUIRE(navigation.focusRing(Domain("app.list")) == std::vector<TargetId>{first, second, third});
}

TEST_CASE("focus rings scope by hierarchical domain and group", "[interaction][navigation]") {
    TargetRegistry registry;
    LayerStack layers;
    NavigationModel navigation(registry, layers);

    TargetId const inGroup = addFocusable(registry, "app.list.rows", "row", 0, Rect{}, "rows");
    [[maybe_unused]] TargetId const otherGroup = addFocusable(registry, "app.list.rows", "button", 0, Rect{}, "actions");
    [[maybe_unused]] TargetId const sibling = addFocusable(registry, "app.toolbar", "button", 0, Rect{}, "rows");

    REQUIRE(navigation.focusRing(Domain("app.list"), "rows") == std::vector<TargetId>{inGroup});
}

TEST_CASE("focus rings exclude hidden disabled and non-focusable targets", "[interaction][navigation]") {
    TargetRegistry registry;
    LayerStack layers;
    NavigationModel navigation(registry, layers);

    TargetId const visible = addFocusable(registry, "app.list", "row", 0, Rect{});
    [[maybe_unused]] TargetId const hidden = addFocusable(registry, "app.list", "row", 1, Rect{}, {}, false, true);
    [[maybe_unused]] TargetId const disabled = addFocusable(registry, "app.list", "row", 2, Rect{}, {}, true, false);

    Capabilities const capabilities{};
    TargetDescriptor descriptor;
    descriptor.domain = Domain("app.list");
    descriptor.role = Role("decoration");
    descriptor.capabilities = capabilities;
    [[maybe_unused]] TargetId const notFocusable = registry.attach(std::move(descriptor));

    REQUIRE(navigation.focusRing(Domain("app.list")) == std::vector<TargetId>{visible});
}

TEST_CASE("focus memory is maintained per domain and drops stale targets", "[interaction][navigation]") {
    TargetRegistry registry;
    LayerStack layers;
    NavigationModel navigation(registry, layers);

    TargetId const session = addFocusable(registry, "app.sessions", "row", 0, Rect{});
    TargetId const prompt = addFocusable(registry, "app.prompt", "input", 0, Rect{});

    navigation.remember(Domain("app.sessions"), session);
    navigation.remember(Domain("app.prompt"), prompt);

    REQUIRE(navigation.remembered(Domain("app.sessions")) == session);
    REQUIRE(navigation.remembered(Domain("app.prompt")) == prompt);

    registry.detach(session);
    REQUIRE_FALSE(navigation.remembered(Domain("app.sessions")).has_value());
    REQUIRE(navigation.remembered(Domain("app.prompt")) == prompt);
}

TEST_CASE("linear focus traversal wraps or stops at ring boundaries", "[interaction][navigation]") {
    TargetRegistry registry;
    LayerStack layers;
    NavigationModel navigation(registry, layers);

    TargetId const first = addFocusable(registry, "app.list", "row", 0, Rect{});
    TargetId const second = addFocusable(registry, "app.list", "row", 1, Rect{});
    TargetId const third = addFocusable(registry, "app.list", "row", 2, Rect{});

    REQUIRE(navigation.focusFirst(Domain("app.list")) == first);
    REQUIRE(navigation.focusLast(Domain("app.list")) == third);
    REQUIRE(navigation.focusNext(Domain("app.list"), first) == second);
    REQUIRE(navigation.focusPrevious(Domain("app.list"), third) == second);
    REQUIRE(navigation.focusNext(Domain("app.list"), third) == first);
    REQUIRE_FALSE(navigation.focusNext(Domain("app.list"), third, false).has_value());
    REQUIRE(navigation.focusPrevious(Domain("app.list"), first) == third);
    REQUIRE_FALSE(navigation.focusPrevious(Domain("app.list"), first, false).has_value());
}

TEST_CASE("linear traversal starts at the ring edge for an unknown current target", "[interaction][navigation]") {
    TargetRegistry registry;
    LayerStack layers;
    NavigationModel navigation(registry, layers);

    TargetId const first = addFocusable(registry, "app.list", "row", 0, Rect{});
    TargetId const last = addFocusable(registry, "app.list", "row", 1, Rect{});

    REQUIRE(navigation.focusNext(Domain("app.list"), TargetId::invalid()) == first);
    REQUIRE(navigation.focusPrevious(Domain("app.list"), TargetId::invalid()) == last);
}

TEST_CASE("directional focus chooses the nearest target in the requested half-plane", "[interaction][navigation]") {
    TargetRegistry registry;
    LayerStack layers;
    NavigationModel navigation(registry, layers);

    TargetId const center = addFocusable(registry, "app.grid", "cell", 0, Rect{40, 40, 20, 20});
    TargetId const above = addFocusable(registry, "app.grid", "cell", 1, Rect{40, 0, 20, 20});
    [[maybe_unused]] TargetId const farAbove = addFocusable(registry, "app.grid", "cell", 2, Rect{40, -80, 20, 20});
    TargetId const right = addFocusable(registry, "app.grid", "cell", 3, Rect{100, 40, 20, 20});
    [[maybe_unused]] TargetId const below = addFocusable(registry, "app.grid", "cell", 4, Rect{40, 100, 20, 20});

    REQUIRE(navigation.focusDirection(Domain("app.grid"), center, NavigationDirection::Up) == above);
    REQUIRE(navigation.focusDirection(Domain("app.grid"), center, NavigationDirection::Right) == right);
    REQUIRE_FALSE(navigation.focusDirection(Domain("app.grid"), center, NavigationDirection::Left).has_value());
}

TEST_CASE("directional focus prefers aligned candidates over nearer diagonal targets", "[interaction][navigation]") {
    TargetRegistry registry;
    LayerStack layers;
    NavigationModel navigation(registry, layers);

    TargetId const center = addFocusable(registry, "app.grid", "cell", 0, Rect{0, 0, 10, 10});
    TargetId const aligned = addFocusable(registry, "app.grid", "cell", 1, Rect{80, 0, 10, 10});
    [[maybe_unused]] TargetId const diagonal = addFocusable(registry, "app.grid", "cell", 2, Rect{20, 15, 10, 10});

    REQUIRE(navigation.focusDirection(Domain("app.grid"), center, NavigationDirection::Right) == aligned);
}

TEST_CASE("focus trap restricts the focus ring to the layer domain", "[interaction][navigation]") {
    TargetRegistry registry;
    LayerStack layers;
    NavigationModel navigation(registry, layers);

    TargetId const outside = addFocusable(registry, "app.toolbar", "button", 0, Rect{});
    TargetId const inside = addFocusable(registry, "app.dialog", "button", 0, Rect{});
    [[maybe_unused]] u64 const layer = layers.push(LayerDescriptor{
        .name = "dialog",
        .domain = Domain("app.dialog"),
        .focusTrap = true,
    });

    REQUIRE_FALSE(layers.ownsInput(outside, registry));
    REQUIRE(layers.ownsInput(inside, registry));
    REQUIRE(navigation.focusRing(Domain("app")) == std::vector<TargetId>{inside});
}

TEST_CASE("key router resolves standard navigation bindings", "[interaction][navigation]") {
    KeyRouter router;
    Modifiers shift;
    shift.shift = true;
    Modifiers control;
    control.control = true;

    REQUIRE(router.resolve(key(NavigationKey::Tab), Domain("app.list"))->command == NavigationCommand::FocusNext);
    REQUIRE(router.resolve(key(NavigationKey::Tab, shift), Domain("app.list"))->command == NavigationCommand::FocusPrevious);
    REQUIRE(router.resolve(key(NavigationKey::ArrowDown), Domain("app.list"))->command == NavigationCommand::MoveDown);
    REQUIRE(router.resolve(key(NavigationKey::ArrowDown, shift), Domain("app.list"))->command == NavigationCommand::ExtendDown);
    REQUIRE(router.resolve(key(NavigationKey::Enter), Domain("app.list"))->command == NavigationCommand::Activate);
    REQUIRE(router.resolve(key(NavigationKey::Enter, control), Domain("app.prompt"))->command == NavigationCommand::Activate);
    REQUIRE(router.resolve(key(NavigationKey::Space), Domain("app.list"))->command == NavigationCommand::Activate);
    REQUIRE(router.resolve(key(NavigationKey::Escape), Domain("app.list"))->command == NavigationCommand::DismissLayer);
    REQUIRE(router.resolve(key(NavigationKey::Menu), Domain("app.list"))->command == NavigationCommand::ShowContextMenu);
    REQUIRE(router.resolve(key(NavigationKey::F10, shift), Domain("app.list"))->command == NavigationCommand::ShowContextMenu);
    REQUIRE(router.resolve(key(NavigationKey::A, control), Domain("app.list"))->command == NavigationCommand::SelectAll);
    REQUIRE(router.resolve(key(NavigationKey::ArrowUp), Domain("app.map"))->command == NavigationCommand::PanUp);
    REQUIRE(router.resolve(key(NavigationKey::ArrowRight), Domain("app.map"))->command == NavigationCommand::PanRight);
    REQUIRE(router.resolve(key(NavigationKey::Plus), Domain("app.map"))->command == NavigationCommand::ZoomIn);
    REQUIRE(router.resolve(key(NavigationKey::Plus, shift), Domain("app.map"))->command == NavigationCommand::ZoomIn);
    REQUIRE(router.resolve(key(NavigationKey::Minus), Domain("app.map"))->command == NavigationCommand::ZoomOut);
}

TEST_CASE("key router prefers domain and layer-specific bindings", "[interaction][navigation]") {
    KeyRouter router;
    KeyBinding const global{key(NavigationKey::Tab), {}, std::nullopt, NavigationCommand::FocusNext};
    KeyBinding const domainSpecific{
        key(NavigationKey::Tab),
        Domain("app.sessions"),
        std::nullopt,
        NavigationCommand::FocusLast,
    };
    KeyBinding const dialogSpecific{
        key(NavigationKey::Tab),
        Domain("app.sessions"),
        LayerKind::Dialog,
        NavigationCommand::FocusFirst,
    };

    router.bind(global);
    router.bind(domainSpecific);
    router.bind(dialogSpecific);

    REQUIRE(router.resolve(key(NavigationKey::Tab), Domain("app.sessions.row"))->command == NavigationCommand::FocusLast);
    REQUIRE(router.resolve(key(NavigationKey::Tab), Domain("app.sessions.row"), LayerKind::Dialog)->command == NavigationCommand::FocusFirst);
    REQUIRE(router.resolve(key(NavigationKey::Tab), Domain("app.toolbar"))->command == NavigationCommand::FocusNext);
}

TEST_CASE("key router replaces and removes custom bindings", "[interaction][navigation]") {
    KeyRouter router;
    KeyBinding binding{key(NavigationKey::F10), Domain("app"), std::nullopt, NavigationCommand::Activate};
    router.bind(binding);
    REQUIRE(router.resolve(key(NavigationKey::F10), Domain("app.window"))->command == NavigationCommand::Activate);

    binding.command = NavigationCommand::ShowContextMenu;
    router.bind(binding);
    REQUIRE(router.bindings().size() == 25);
    REQUIRE(router.resolve(key(NavigationKey::F10), Domain("app.window"))->command == NavigationCommand::ShowContextMenu);
    REQUIRE(router.unbind(key(NavigationKey::F10), Domain("app")));
    REQUIRE_FALSE(router.resolve(key(NavigationKey::F10), Domain("app.window")).has_value());
}
