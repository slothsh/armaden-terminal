#include <catch2/catch_test_macros.hpp>

import std;
import interaction;

using namespace mts::interaction;

namespace {

auto addTarget(TargetRegistry& registry, std::string domain) -> TargetId {
    TargetDescriptor descriptor;
    descriptor.domain = Domain(std::move(domain));
    descriptor.role = Role("target");

    return registry.attach(std::move(descriptor));
}

auto makeLayer(std::string name, std::string domain, bool escape = true, bool outside = true, bool trap = false) -> LayerDescriptor {
    LayerDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.domain = Domain(std::move(domain));
    descriptor.dismissOnEscape = escape;
    descriptor.dismissOnOutsideClick = outside;
    descriptor.focusTrap = trap;

    return descriptor;
}

struct LayerRecorder {
    std::vector<LayerChange> changes;

    auto operator()(LayerChange const& change) -> void {
        this->changes.push_back(change);
    }
};

}

TEST_CASE("layer stack tracks pushes and pops in order", "[interaction][layers]") {
    LayerStack layers;
    LayerRecorder recorder;
    [[maybe_unused]] std::size_t const subscription = layers.subscribe([&recorder](LayerChange const& change) {
        recorder(change);
    });

    u64 const first = layers.push(makeLayer("menu", "app.menu"));
    u64 const second = layers.push(makeLayer("dialog", "app.dialog"));

    REQUIRE(first < second);
    REQUIRE(layers.depth() == 2);
    REQUIRE(layers.top()->id == second);
    REQUIRE(layers.topDomain()->path() == "app.dialog");
    REQUIRE(recorder.changes.size() == 2);
    REQUIRE(recorder.changes[0].kind == LayerChangeKind::Pushed);
    REQUIRE(recorder.changes[0].depth == 1);
    REQUIRE(recorder.changes[1].depth == 2);

    auto const popped = layers.pop();
    REQUIRE(popped.has_value());
    REQUIRE(popped->id == second);
    REQUIRE(layers.top()->id == first);
}

TEST_CASE("removing a layer preserves layers stacked above it", "[interaction][layers]") {
    LayerStack layers;
    u64 const first = layers.push(makeLayer("menu", "app.menu"));
    u64 const second = layers.push(makeLayer("dialog", "app.dialog"));
    [[maybe_unused]] u64 const third = layers.push(makeLayer("confirm", "app.confirm"));

    REQUIRE(layers.remove(second));
    REQUIRE(layers.depth() == 2);
    REQUIRE(layers.top()->id == third);
    REQUIRE_FALSE(layers.find(second).has_value());

    REQUIRE(layers.remove(first));
    REQUIRE(layers.depth() == 1);
    REQUIRE(layers.top()->id == third);
}

TEST_CASE("popTo keeps the requested layer and removes descendants", "[interaction][layers]") {
    LayerStack layers;
    u64 const first = layers.push(makeLayer("menu", "app.menu"));
    [[maybe_unused]] u64 const second = layers.push(makeLayer("dialog", "app.dialog"));
    [[maybe_unused]] u64 const third = layers.push(makeLayer("confirm", "app.confirm"));

    REQUIRE(layers.popTo(first));
    REQUIRE(layers.depth() == 1);
    REQUIRE(layers.top()->id == first);
    REQUIRE_FALSE(layers.popTo(0));
}

TEST_CASE("escape policy is determined by the top layer", "[interaction][layers]") {
    LayerStack layers;
    u64 const popup = layers.push(makeLayer("menu", "app.menu", true, true));
    REQUIRE(layers.shouldDismissOnEscape());

    [[maybe_unused]] u64 const pinned = layers.push(makeLayer("pinned", "app.pane", false, false));
    REQUIRE_FALSE(layers.shouldDismissOnEscape());

    REQUIRE(layers.popTo(popup));
    REQUIRE(layers.shouldDismissOnEscape());
}

TEST_CASE("outside clicks respect layer domain and dismissal policy", "[interaction][layers]") {
    TargetRegistry registry;
    TargetId const inside = addTarget(registry, "app.dialog");
    TargetId const descendant = addTarget(registry, "app.dialog.body");
    TargetId const outside = addTarget(registry, "app.content");
    LayerStack layers;
    [[maybe_unused]] u64 const dialog = layers.push(makeLayer("dialog", "app.dialog", false, true, true));

    REQUIRE_FALSE(layers.shouldDismissOnEscape());
    REQUIRE_FALSE(layers.outsideClickDismisses(inside, registry));
    REQUIRE_FALSE(layers.outsideClickDismisses(descendant, registry));
    REQUIRE(layers.outsideClickDismisses(outside, registry));
    REQUIRE(layers.outsideClickDismisses(TargetId::invalid(), registry));
}

TEST_CASE("pinned layers do not dismiss on outside clicks", "[interaction][layers]") {
    TargetRegistry registry;
    TargetId const outside = addTarget(registry, "app.content");
    LayerStack layers;
    [[maybe_unused]] u64 const pane = layers.push(makeLayer("pane", "app.pane", false, false, false));

    REQUIRE_FALSE(layers.shouldDismissOnEscape());
    REQUIRE_FALSE(layers.outsideClickDismisses(outside, registry));
}

TEST_CASE("focus trap confines input to the top layer domain", "[interaction][layers]") {
    TargetRegistry registry;
    TargetId const inside = addTarget(registry, "app.dialog.content");
    TargetId const outside = addTarget(registry, "app.toolbar");
    LayerStack layers;
    [[maybe_unused]] u64 const dialog = layers.push(makeLayer("dialog", "app.dialog", true, false, true));

    REQUIRE(layers.ownsInput(inside, registry));
    REQUIRE_FALSE(layers.ownsInput(outside, registry));
    REQUIRE_FALSE(layers.ownsInput(TargetId::invalid(), registry));
}

TEST_CASE("non-trapping layers leave input ownership unrestricted", "[interaction][layers]") {
    TargetRegistry registry;
    TargetId const outside = addTarget(registry, "app.content");
    LayerStack layers;
    [[maybe_unused]] u64 const popup = layers.push(makeLayer("popup", "app.popup", true, true, false));

    REQUIRE(layers.ownsInput(outside, registry));
}

TEST_CASE("context menu layers permit commands for their owner without weakening modal traps", "[interaction][layers]") {
    TargetRegistry registry;
    TargetId const owner = addTarget(registry, "app.map");
    TargetId const unrelated = addTarget(registry, "app.toolbar");
    LayerStack layers;

    LayerDescriptor contextMenu = makeLayer("context-menu", "app.contextMenu", true, true, true);
    contextMenu.kind = LayerKind::ContextMenu;
    contextMenu.owner = owner;
    [[maybe_unused]] u64 const menu = layers.push(contextMenu);

    REQUIRE(layers.ownsInput(owner, registry));
    REQUIRE_FALSE(layers.ownsInput(unrelated, registry));

    layers.clear();
    LayerDescriptor dialog = makeLayer("dialog", "app.dialog", true, false, true);
    dialog.kind = LayerKind::Dialog;
    dialog.owner = owner;
    [[maybe_unused]] u64 const modal = layers.push(dialog);

    REQUIRE_FALSE(layers.ownsInput(owner, registry));
}

TEST_CASE("clear removes all layers and notifies each pop", "[interaction][layers]") {
    LayerStack layers;
    LayerRecorder recorder;
    [[maybe_unused]] std::size_t const subscription = layers.subscribe([&recorder](LayerChange const& change) {
        recorder(change);
    });

    [[maybe_unused]] u64 const first = layers.push(makeLayer("menu", "app.menu"));
    [[maybe_unused]] u64 const second = layers.push(makeLayer("dialog", "app.dialog"));

    layers.clear();

    REQUIRE(layers.empty());
    REQUIRE(layers.depth() == 0);
    REQUIRE(recorder.changes.size() == 4);
    REQUIRE(recorder.changes[2].kind == LayerChangeKind::Popped);
    REQUIRE(recorder.changes[3].kind == LayerChangeKind::Popped);
}
