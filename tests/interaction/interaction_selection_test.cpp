#include <catch2/catch_test_macros.hpp>

import std;
import interaction;

using namespace mts::interaction;

namespace {

auto attachTarget(
    TargetRegistry& registry,
    std::string const& domain,
    std::string const& role,
    int order = 0
) -> TargetId {
    TargetDescriptor descriptor;
    descriptor.domain = Domain(domain);
    descriptor.role = Role(role);
    descriptor.order = order;

    return registry.attach(std::move(descriptor));
}

struct Recorder {
    std::vector<SelectionChange> changes;

    auto operator()(SelectionChange const& change) -> void {
        this->changes.push_back(change);
    }

    [[nodiscard]] auto ids(std::size_t index) const -> std::vector<TargetId> {
        return changes[index].added;
    }
};

auto idsOf(std::vector<TargetId> const& list) -> std::vector<u64> {
    std::vector<u64> result;
    for (TargetId const id : list) {
        result.push_back(id.packed());
    }

    return result;
}

}

TEST_CASE("replace intent selects a single target", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");

    TargetId const first = attachTarget(registry, "app.list", "row");
    TargetId const second = attachTarget(registry, "app.list", "row");

    selection.select(list, first, SelectionIntent::Replace);
    selection.select(list, second, SelectionIntent::Replace);

    REQUIRE(selection.count(list) == 1);
    REQUIRE(selection.ordered(list) == std::vector<TargetId>{second});
    REQUIRE(selection.anchor(list) == second);
    REQUIRE(selection.cursor(list) == second);
}

TEST_CASE("add intent appends without clearing", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");

    TargetId const first = attachTarget(registry, "app.list", "row");
    TargetId const second = attachTarget(registry, "app.list", "row");

    selection.select(list, first, SelectionIntent::Replace);
    selection.select(list, second, SelectionIntent::Add);

    REQUIRE(selection.count(list) == 2);
    REQUIRE(idsOf(selection.ordered(list)) == idsOf({first, second}));
    REQUIRE(selection.cursor(list) == second);
}

TEST_CASE("toggle adds and removes", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");

    TargetId const first = attachTarget(registry, "app.list", "row");
    TargetId const second = attachTarget(registry, "app.list", "row");

    selection.select(list, first, SelectionIntent::Replace);
    selection.toggle(list, second);
    REQUIRE(selection.count(list) == 2);

    selection.toggle(list, second);
    REQUIRE(selection.count(list) == 1);
    REQUIRE(selection.contains(list, first));
    REQUIRE_FALSE(selection.contains(list, second));
}

TEST_CASE("range replaces with the registry order span", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");

    TargetId const a = attachTarget(registry, "app.list", "row");
    TargetId const b = attachTarget(registry, "app.list", "row");
    TargetId const c = attachTarget(registry, "app.list", "row");
    TargetId const d = attachTarget(registry, "app.list", "row");

    selection.select(list, a, SelectionIntent::Replace);
    selection.select(list, d, SelectionIntent::Range);

    REQUIRE(idsOf(selection.ordered(list)) == idsOf({a, b, c, d}));
    REQUIRE(selection.anchor(list) == a);
    REQUIRE(selection.cursor(list) == d);
}

TEST_CASE("add range unions the span with the existing selection", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");

    TargetId const a = attachTarget(registry, "app.list", "row");
    TargetId const b = attachTarget(registry, "app.list", "row");
    TargetId const c = attachTarget(registry, "app.list", "row");
    TargetId const d = attachTarget(registry, "app.list", "row");

    selection.select(list, a, SelectionIntent::Replace);
    selection.select(list, d, SelectionIntent::AddRange);

    REQUIRE(idsOf(selection.ordered(list)) == idsOf({a, b, c, d}));
}

TEST_CASE("range works in reverse direction", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");

    TargetId const a = attachTarget(registry, "app.list", "row");
    TargetId const b = attachTarget(registry, "app.list", "row");
    TargetId const c = attachTarget(registry, "app.list", "row");

    selection.select(list, c, SelectionIntent::Replace);
    selection.select(list, a, SelectionIntent::Range);

    REQUIRE(idsOf(selection.ordered(list)) == idsOf({a, b, c}));
}

TEST_CASE("clear, selectAll, invert and selectOnly", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");

    TargetId const a = attachTarget(registry, "app.list", "row");
    TargetId const b = attachTarget(registry, "app.list", "row");
    TargetId const c = attachTarget(registry, "app.list", "row");

    selection.selectAll(list, {a, b, c});
    REQUIRE(selection.count(list) == 3);
    REQUIRE(selection.first(list).has_value());
    REQUIRE(selection.first(list)->packed() == a.packed());

    selection.invert(list, {a, b, c});
    REQUIRE(selection.count(list) == 0);

    selection.selectAll(list, {a, b, c});
    selection.clear(list);
    REQUIRE(selection.count(list) == 0);

    selection.selectOnly(list, {b, c});
    REQUIRE(idsOf(selection.ordered(list)) == idsOf({b, c}));
}

TEST_CASE("selectOnly drops foreign domain targets", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");
    Domain const map("app.map");

    TargetId const row = attachTarget(registry, "app.list", "row");
    TargetId const surface = attachTarget(registry, "app.map", "surface");

    selection.selectOnly(list, {row, surface});

    REQUIRE(selection.count(list) == 1);
    REQUIRE(selection.contains(list, row));
    REQUIRE_FALSE(selection.contains(list, surface));
}

TEST_CASE("selections are isolated per domain", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");
    Domain const map("app.map");

    TargetId const row = attachTarget(registry, "app.list", "row");
    TargetId const surface = attachTarget(registry, "app.map", "surface");

    selection.select(list, row, SelectionIntent::Replace);
    selection.select(map, surface, SelectionIntent::Replace);

    REQUIRE(selection.count(list) == 1);
    REQUIRE(selection.count(map) == 1);

    selection.clear(list);
    REQUIRE(selection.count(list) == 0);
    REQUIRE(selection.count(map) == 1);
}

TEST_CASE("detached targets are pruned and reported", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");
    Recorder recorder;
    [[maybe_unused]] std::size_t const subscription = selection.subscribe([&recorder](SelectionChange const& change) {
        recorder(change);
    });

    TargetId const a = attachTarget(registry, "app.list", "row");
    TargetId const b = attachTarget(registry, "app.list", "row");
    selection.selectAll(list, {a, b});

    registry.detach(a);
    selection.prune();

    REQUIRE(selection.count(list) == 1);
    REQUIRE(idsOf(selection.ordered(list)) == idsOf({b}));
    REQUIRE(recorder.changes.back().kind == SelectionChangeKind::Removed);
    REQUIRE(recorder.changes.back().removed.size() == 1);
}

TEST_CASE("select reports change deltas", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");
    Recorder recorder;
    [[maybe_unused]] std::size_t const subscription = selection.subscribe([&recorder](SelectionChange const& change) {
        recorder(change);
    });

    TargetId const a = attachTarget(registry, "app.list", "row");
    TargetId const b = attachTarget(registry, "app.list", "row");

    selection.select(list, a, SelectionIntent::Replace);
    REQUIRE(recorder.changes.back().kind == SelectionChangeKind::Added);

    selection.select(list, b, SelectionIntent::Add);
    REQUIRE(recorder.changes.back().kind == SelectionChangeKind::Added);
    REQUIRE(recorder.changes.back().added.size() == 1);

    selection.select(list, b, SelectionIntent::Toggle);
    REQUIRE(recorder.changes.back().kind == SelectionChangeKind::Removed);
    REQUIRE(recorder.changes.back().removed.size() == 1);

    selection.clear(list);
    REQUIRE(recorder.changes.back().kind == SelectionChangeKind::Removed);
}

TEST_CASE("modifier to intent mapping follows platform conventions", "[interaction][selection]") {
    REQUIRE(intentFrom(false, false) == SelectionIntent::Replace);
    REQUIRE(intentFrom(true, false) == SelectionIntent::Range);
    REQUIRE(intentFrom(false, true) == SelectionIntent::Toggle);
    REQUIRE(intentFrom(true, true) == SelectionIntent::AddRange);
}

TEST_CASE("invalid or foreign targets are ignored", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");
    Domain const map("app.map");

    TargetId const row = attachTarget(registry, "app.list", "row");

    selection.select(list, TargetId::invalid(), SelectionIntent::Replace);
    selection.select(map, row, SelectionIntent::Replace);
    REQUIRE(selection.count(list) == 0);
    REQUIRE(selection.count(map) == 0);
}

TEST_CASE("anchor is preserved across add and toggle", "[interaction][selection]") {
    TargetRegistry registry;
    SelectionModel selection(registry);
    Domain const list("app.list");

    TargetId const a = attachTarget(registry, "app.list", "row");
    TargetId const b = attachTarget(registry, "app.list", "row");
    TargetId const c = attachTarget(registry, "app.list", "row");

    selection.select(list, a, SelectionIntent::Replace);
    selection.select(list, c, SelectionIntent::Add);
    REQUIRE(selection.anchor(list) == a);
    REQUIRE(selection.cursor(list) == c);

    selection.toggle(list, b);
    REQUIRE(selection.anchor(list) == a);
}
