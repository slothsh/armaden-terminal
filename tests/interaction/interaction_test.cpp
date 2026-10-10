#include <catch2/catch_test_macros.hpp>

import std;
import interaction;

using namespace mts::interaction;

namespace {

auto makeDescriptor(
    std::string domain,
    std::string role,
    int order = 0,
    Rect geometry = Rect{}
) -> TargetDescriptor {
    TargetDescriptor descriptor;
    descriptor.domain = Domain(std::move(domain));
    descriptor.role = Role(std::move(role));
    descriptor.order = order;
    descriptor.geometry = geometry;

    return descriptor;
}

}

TEST_CASE("registry attaches and resolves targets", "[interaction]") {
    TargetRegistry registry;

    TargetId const id = registry.attach(makeDescriptor("app.list", "row"));

    REQUIRE(id.valid());
    REQUIRE(registry.size() == 1);
    REQUIRE(registry.contains(id));

    auto const resolved = registry.find(id);
    REQUIRE(resolved.has_value());
    REQUIRE(resolved->id == id);
    REQUIRE(resolved->domain.path() == "app.list");
    REQUIRE(resolved->role.name() == "row");
}

TEST_CASE("registry rejects stale handles after detach and recycle", "[interaction]") {
    TargetRegistry registry;

    TargetId const first = registry.attach(makeDescriptor("app.list", "row"));
    registry.detach(first);

    REQUIRE_FALSE(registry.contains(first));

    TargetId const recycled = registry.attach(makeDescriptor("app.list", "row"));

    REQUIRE(recycled.index == first.index);
    REQUIRE(recycled.generation != first.generation);
    REQUIRE(registry.contains(recycled));
    REQUIRE_FALSE(registry.contains(first));
}

TEST_CASE("registry rekeys a recycled delegate in place", "[interaction]") {
    TargetRegistry registry;

    TargetId const id = registry.attach(makeDescriptor("app.list", "row", 0, Rect{0.0, 0.0, 10.0, 10.0}));
    registry.rekey(id, makeDescriptor("app.list", "row", 4, Rect{20.0, 0.0, 10.0, 10.0}));

    auto const resolved = registry.find(id);
    REQUIRE(resolved.has_value());
    REQUIRE(resolved->order == 4);
    REQUIRE(resolved->geometry.x == 20.0);
}

TEST_CASE("registry updates geometry and capabilities", "[interaction]") {
    TargetRegistry registry;

    TargetId const id = registry.attach(makeDescriptor("app.list", "row"));
    registry.updateGeometry(id, Rect{2.0, 4.0, 6.0, 8.0});

    Capabilities capabilities;
    capabilities.focusable = true;
    capabilities.selection = SelectionMode::Multiple;
    registry.updateCapabilities(id, capabilities);

    auto const resolved = registry.find(id);
    REQUIRE(resolved.has_value());
    REQUIRE(resolved->geometry.width == 6.0);
    REQUIRE(resolved->capabilities.focusable);
    REQUIRE(resolved->capabilities.selection == SelectionMode::Multiple);
}

TEST_CASE("registry indexes by domain and role", "[interaction]") {
    TargetRegistry registry;

    TargetId const first = registry.attach(makeDescriptor("app.list", "row"));
    TargetId const second = registry.attach(makeDescriptor("app.list", "row"));
    TargetId const other = registry.attach(makeDescriptor("app.toolbar", "button"));

    auto const rows = registry.byRole(Role("row"));
    REQUIRE(rows.size() == 2);
    REQUIRE(rows[0].id == first);
    REQUIRE(rows[1].id == second);

    auto const list = registry.byDomain(Domain("app.list"));
    REQUIRE(list.size() == 2);

    auto const toolbar = registry.byDomain(Domain("app.toolbar"));
    REQUIRE(toolbar.size() == 1);
    REQUIRE(toolbar[0].id == other);
}

TEST_CASE("registry orders a domain by explicit order then insertion", "[interaction]") {
    TargetRegistry registry;

    TargetId const late = registry.attach(makeDescriptor("app.list", "row", 5));
    TargetId const earlyA = registry.attach(makeDescriptor("app.list", "row", 1));
    TargetId const earlyB = registry.attach(makeDescriptor("app.list", "row", 1));

    auto const ordered = registry.order(Domain("app.list"));
    REQUIRE(ordered.size() == 3);
    REQUIRE(ordered[0] == earlyA);
    REQUIRE(ordered[1] == earlyB);
    REQUIRE(ordered[2] == late);
}

TEST_CASE("domain containment matches segment boundaries", "[interaction]") {
    Domain const root("app");
    Domain const list("app.list");
    Domain const line("app.listRow");

    REQUIRE(list.isWithin(root));
    REQUIRE(list.isWithin(list));
    REQUIRE(root.isWithin(root));
    REQUIRE_FALSE(root.isWithin(list));
    REQUIRE_FALSE(line.isWithin(list));
}

TEST_CASE("registry hit test providers resolve top-down without duplicates", "[interaction]") {
    TargetRegistry registry;

    TargetId const background = registry.attach(makeDescriptor("app.map", "surface"));
    TargetId const annotation = registry.attach(makeDescriptor("app.map", "annotation"));

    registry.addHitTestProvider([background](Point const&) {
        return std::vector<TargetId>{background};
    });
    registry.addHitTestProvider([background, annotation](Point const&) {
        return std::vector<TargetId>{annotation, background};
    });

    auto const hits = registry.hitTest(Point{1.0, 1.0});

    REQUIRE(hits.size() == 2);
    REQUIRE(hits[0].id == annotation);
    REQUIRE(hits[1].id == background);
}

TEST_CASE("registry removes hit test providers", "[interaction]") {
    TargetRegistry registry;

    TargetId const surface = registry.attach(makeDescriptor("app.map", "surface"));
    std::size_t const token = registry.addHitTestProvider([surface](Point const&) {
        return std::vector<TargetId>{surface};
    });

    REQUIRE(registry.hitTest(Point{}).size() == 1);

    registry.removeHitTestProvider(token);

    REQUIRE(registry.hitTest(Point{}).empty());
}

TEST_CASE("target id packs and round-trips", "[interaction]") {
    TargetId const id{7, 3};

    REQUIRE(TargetId::fromPacked(id.packed()) == id);
    REQUIRE(TargetId::invalid().valid() == false);
}
