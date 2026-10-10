#include <catch2/catch_test_macros.hpp>

import std;
import interaction;

using namespace mts::interaction;

namespace {

auto makeTarget(
    TargetRegistry& registry,
    std::string const& domain,
    std::string const& role,
    Rect geometry = Rect{}
) -> TargetId {
    TargetDescriptor descriptor;
    descriptor.domain = Domain(domain);
    descriptor.role = Role(role);
    descriptor.geometry = geometry;

    return registry.attach(std::move(descriptor));
}

struct Recorder {
    std::vector<ContextChangeKind> kinds;
    std::vector<TargetId> targets;

    auto operator()(ContextChange const& change) -> void {
        kinds.push_back(change.kind);
        targets.push_back(change.target);
    }
};

auto makeClock(std::int64_t& tick) -> ContextStore::Clock {
    return [&tick] { return tick; };
}

}

TEST_CASE("context resolves focused, current, hovered and lastClicked targets", "[interaction][context]") {
    TargetRegistry registry;
    ContextStore store(registry);

    TargetId const row = makeTarget(registry, "app.list", "row");

    store.setFocused(row);
    store.setCurrent(row);
    store.setHovered(row);
    store.setLastClicked(row);

    REQUIRE(store.focused().has_value());
    REQUIRE(store.focused()->id == row);
    REQUIRE(store.current()->id == row);
    REQUIRE(store.hovered()->id == row);
    REQUIRE(store.lastClicked()->id == row);
}

TEST_CASE("context state clears when the target is detached", "[interaction][context]") {
    TargetRegistry registry;
    ContextStore store(registry);

    TargetId const row = makeTarget(registry, "app.list", "row");
    store.setCurrent(row);
    REQUIRE(store.current().has_value());

    registry.detach(row);

    REQUIRE_FALSE(store.current().has_value());
}

TEST_CASE("context notifies on focus, current and hover transitions only", "[interaction][context]") {
    TargetRegistry registry;
    ContextStore store(registry);
    Recorder recorder;
    [[maybe_unused]] std::size_t const subscription = store.subscribe([&recorder](ContextChange const& change) {
        recorder(change);
    });

    TargetId const row = makeTarget(registry, "app.list", "row");
    TargetId const other = makeTarget(registry, "app.list", "row");

    store.setFocused(row);
    store.setFocused(row);
    store.setFocused(other);

    store.setCurrent(row);
    store.setCurrent(row);

    store.setHovered(row);
    store.setHovered(row);

    REQUIRE(recorder.kinds.size() == 4);
    REQUIRE(recorder.kinds[0] == ContextChangeKind::FocusChanged);
    REQUIRE(recorder.kinds[1] == ContextChangeKind::FocusChanged);
    REQUIRE(recorder.kinds[2] == ContextChangeKind::CurrentChanged);
    REQUIRE(recorder.kinds[3] == ContextChangeKind::HoverChanged);

    store.unsubscribe(subscription);
    store.setHovered(other);
    REQUIRE(recorder.kinds.size() == 4);
}

TEST_CASE("pointer press and release track buttons", "[interaction][context]") {
    TargetRegistry registry;
    ContextStore store(registry);

    store.recordPress(PointerButton::Primary, Point{1.0, 2.0}, Modifiers{});
    REQUIRE(store.pointer().primaryPressed);
    REQUIRE_FALSE(store.pointer().secondaryPressed);
    REQUIRE(store.lastPressedButton() == PointerButton::Primary);

    store.recordPress(PointerButton::Secondary, Point{1.0, 2.0}, Modifiers{});
    REQUIRE(store.pointer().primaryPressed);
    REQUIRE(store.pointer().secondaryPressed);
    REQUIRE(store.lastPressedButton() == PointerButton::Secondary);

    store.recordRelease(PointerButton::Primary, Point{1.0, 2.0}, Modifiers{});
    REQUIRE_FALSE(store.pointer().primaryPressed);
    REQUIRE(store.pointer().secondaryPressed);
    REQUIRE(store.lastPressedButton() == PointerButton::Secondary);
}

TEST_CASE("pointer moves update position without notifications", "[interaction][context]") {
    TargetRegistry registry;
    ContextStore store(registry);
    Recorder recorder;
    [[maybe_unused]] std::size_t const subscription = store.subscribe([&recorder](ContextChange const& change) {
        recorder(change);
    });

    store.recordMove(Point{10.0, 20.0}, Modifiers{});

    REQUIRE(store.pointer().x == 10.0);
    REQUIRE(store.pointer().y == 20.0);
    REQUIRE(recorder.kinds.empty());
}

TEST_CASE("modifier transitions notify, identical modifiers do not", "[interaction][context]") {
    TargetRegistry registry;
    ContextStore store(registry);
    Recorder recorder;
    [[maybe_unused]] std::size_t const subscription = store.subscribe([&recorder](ContextChange const& change) {
        recorder(change);
    });

    Modifiers const plain;
    Modifiers shifted;
    shifted.shift = true;

    store.recordModifiers(plain);
    store.recordModifiers(shifted);
    store.recordModifiers(shifted);

    REQUIRE(recorder.kinds.size() == 1);
    REQUIRE(recorder.kinds[0] == ContextChangeKind::ModifiersChanged);
    REQUIRE(store.modifiers().shift);
}

TEST_CASE("click count repeats for the same button near the same position", "[interaction][context]") {
    std::int64_t tick = 0;
    TargetRegistry registry;
    ContextStore store(registry, makeClock(tick));

    store.recordPress(PointerButton::Primary, Point{4.0, 4.0}, Modifiers{});
    REQUIRE(store.clickCount() == 1);

    tick = 100;
    store.recordPress(PointerButton::Primary, Point{5.0, 5.0}, Modifiers{});
    REQUIRE(store.clickCount() == 2);

    tick = 1200;
    store.recordPress(PointerButton::Primary, Point{5.0, 5.0}, Modifiers{});
    REQUIRE(store.clickCount() == 1);
}

TEST_CASE("recordClick promotes the target to current and lastClicked", "[interaction][context]") {
    TargetRegistry registry;
    ContextStore store(registry);
    Recorder recorder;
    [[maybe_unused]] std::size_t const subscription = store.subscribe([&recorder](ContextChange const& change) {
        recorder(change);
    });

    TargetId const row = makeTarget(registry, "app.list", "row");

    store.recordClick(row, PointerButton::Primary, Point{0.0, 0.0});

    REQUIRE(store.currentId() == row);
    REQUIRE(store.lastClickedId() == row);
    REQUIRE(recorder.kinds.back() == ContextChangeKind::Clicked);
    REQUIRE(recorder.targets.back() == row);
}

TEST_CASE("domain activity reflects focused and current containment", "[interaction][context]") {
    TargetRegistry registry;
    ContextStore store(registry);

    TargetId const row = makeTarget(registry, "app.list", "row");
    TargetId const canvas = makeTarget(registry, "app.map", "surface");

    store.setFocused(row);
    REQUIRE(store.domainIsActive(Domain("app")));
    REQUIRE(store.domainIsActive(Domain("app.list")));
    REQUIRE_FALSE(store.domainIsActive(Domain("app.map")));

    store.setFocused(TargetId::invalid());
    store.setCurrent(canvas);
    REQUIRE(store.domainIsActive(Domain("app.map")));
    REQUIRE_FALSE(store.domainIsActive(Domain("app.list")));

    REQUIRE(store.isWithin(row, Domain("app.list")));
    REQUIRE_FALSE(store.isWithin(canvas, Domain("app.list")));
}

TEST_CASE("clearAll resets every context field", "[interaction][context]") {
    TargetRegistry registry;
    ContextStore store(registry);

    TargetId const row = makeTarget(registry, "app.list", "row");
    store.setFocused(row);
    store.setCurrent(row);
    store.setHovered(row);
    store.recordPress(PointerButton::Primary, Point{1.0, 1.0}, Modifiers{});

    store.clearAll();

    REQUIRE_FALSE(store.focused().has_value());
    REQUIRE_FALSE(store.current().has_value());
    REQUIRE_FALSE(store.hovered().has_value());
    REQUIRE_FALSE(store.lastClicked().has_value());
    REQUIRE(store.clickCount() == 0);
    REQUIRE(store.pointer().primaryPressed == false);
}
