export module interaction:selection;

import std;
import :model;
import :registry;

export namespace mts::interaction {

enum class SelectionIntent : u8 {
    Replace,
    Toggle,
    Range,
    AddRange,
    Add,
};

enum class SelectionChangeKind : u8 {
    Added,
    Removed,
    Reset,
};

struct SelectionChange {
    Domain domain;
    SelectionChangeKind kind;
    std::vector<TargetId> added;
    std::vector<TargetId> removed;
};

[[nodiscard]] auto intentFrom(bool shift, bool control) -> SelectionIntent {
    if (shift && control) {
        return SelectionIntent::AddRange;
    }

    if (shift) {
        return SelectionIntent::Range;
    }

    if (control) {
        return SelectionIntent::Toggle;
    }

    return SelectionIntent::Replace;
}

class SelectionModel {
public:
    using Handler = std::function<void(SelectionChange const&)>;

    explicit SelectionModel(TargetRegistry const& registry)
        : m_registry(registry) {}

    SelectionModel(SelectionModel const&) = delete;
    auto operator=(SelectionModel const&) -> SelectionModel& = delete;
    SelectionModel(SelectionModel&&) = delete;
    auto operator=(SelectionModel&&) -> SelectionModel& = delete;

    [[nodiscard]] auto subscribe(Handler handler) -> std::size_t {
        std::size_t const id = m_nextSubscription++;
        m_handlers.push_back(Entry{id, std::move(handler)});

        return id;
    }

    auto unsubscribe(std::size_t id) -> void {
        std::erase_if(m_handlers, [id](Entry const& entry) {
            return entry.id == id;
        });
    }

    auto select(Domain const& domain, TargetId target, SelectionIntent intent) -> void {
        if (!this->acceptable(domain, target)) {
            return;
        }

        std::vector<TargetId> const previous = this->ordered(domain);

        switch (intent) {
            case SelectionIntent::Replace: {
                this->commit(domain, previous, std::vector<TargetId>{target}, target, target);
                break;
            }

            case SelectionIntent::Add: {
                this->commit(domain, previous, this->merge(previous, {target}), this->anchorOr(domain, target), target);
                break;
            }

            case SelectionIntent::Toggle: {
                std::vector<TargetId> next = previous;
                std::erase(next, target);
                if (!this->member(domain, target)) {
                    next.push_back(target);
                }

                this->commit(domain, previous, next, this->anchorOr(domain, target), target);
                break;
            }

            case SelectionIntent::Range: {
                this->applyRange(domain, previous, this->anchorOr(domain, target), target, false);
                break;
            }

            case SelectionIntent::AddRange: {
                this->applyRange(domain, previous, this->anchorOr(domain, target), target, true);
                break;
            }
        }
    }

    auto selectRange(Domain const& domain, TargetId from, TargetId to, bool additive) -> void {
        if (!this->acceptable(domain, from) || !this->acceptable(domain, to)) {
            return;
        }

        this->applyRange(domain, this->ordered(domain), from, to, additive);
    }

    auto toggle(Domain const& domain, TargetId target) -> void {
        this->select(domain, target, SelectionIntent::Toggle);
    }

    auto clear(Domain const& domain) -> void {
        this->commit(domain, this->ordered(domain), {}, TargetId::invalid(), TargetId::invalid());
    }

    auto selectAll(Domain const& domain, std::vector<TargetId> const& candidates) -> void {
        std::vector<TargetId> const ordered = this->registryOrder(domain);
        std::vector<TargetId> next;
        for (TargetId const candidate : candidates) {
            if (std::find(ordered.begin(), ordered.end(), candidate) != ordered.end()) {
                next.push_back(candidate);
            }
        }

        TargetId const anchor = next.empty() ? TargetId::invalid() : next.front();
        TargetId const cursor = next.empty() ? TargetId::invalid() : next.back();
        this->commit(domain, this->ordered(domain), std::move(next), anchor, cursor);
    }

    auto invert(Domain const& domain, std::vector<TargetId> const& candidates) -> void {
        std::vector<TargetId> next;
        for (TargetId const candidate : candidates) {
            if (this->acceptable(domain, candidate) && !this->member(domain, candidate)) {
                next.push_back(candidate);
            }
        }

        this->commit(domain, this->ordered(domain), std::move(next), TargetId::invalid(), TargetId::invalid());
    }

    auto selectOnly(Domain const& domain, std::vector<TargetId> const& targets) -> void {
        std::vector<TargetId> next;
        for (TargetId const target : targets) {
            if (this->acceptable(domain, target)) {
                next.push_back(target);
            }
        }

        TargetId const anchor = next.empty() ? TargetId::invalid() : next.front();
        TargetId const cursor = next.empty() ? TargetId::invalid() : next.back();
        this->commit(domain, this->ordered(domain), std::move(next), anchor, cursor);
    }

    auto setAnchor(Domain const& domain, TargetId target) -> void {
        DomainSelection& selection = this->selectionFor(domain);
        if (!this->acceptable(domain, target)) {
            return;
        }

        selection.anchor = target;
    }

    auto setCursor(Domain const& domain, TargetId target) -> void {
        if (!this->acceptable(domain, target)) {
            return;
        }

        this->selectionFor(domain).cursor = target;
    }

    [[nodiscard]] auto contains(Domain const& domain, TargetId target) -> bool {
        this->pruneDomain(domain);

        return this->member(domain, target);
    }

    [[nodiscard]] auto count(Domain const& domain) -> std::size_t {
        this->pruneDomain(domain);

        return this->memberSet(domain).size();
    }

    [[nodiscard]] auto first(Domain const& domain) -> std::optional<TargetId> {
        std::vector<TargetId> const ordered = this->ordered(domain);

        return ordered.empty() ? std::nullopt : std::optional<TargetId>{ordered.front()};
    }

    [[nodiscard]] auto ordered(Domain const& domain) const -> std::vector<TargetId> {
        std::vector<TargetId> result;
        for (TargetId const id : this->selectionOf(domain).ordered) {
            if (m_registry.contains(id)) {
                result.push_back(id);
            }
        }

        return result;
    }

    [[nodiscard]] auto anchor(Domain const& domain) const -> TargetId {
        return this->selectionOf(domain).anchor;
    }

    [[nodiscard]] auto cursor(Domain const& domain) const -> TargetId {
        return this->selectionOf(domain).cursor;
    }

    [[nodiscard]] auto domains() const -> std::vector<Domain> {
        std::vector<Domain> result;
        for (auto const& [key, selection] : m_selections) {
            result.push_back(Domain(key));
        }

        return result;
    }

    auto prune() -> void {
        std::vector<Domain> const domainList = this->domains();
        for (Domain const& domain : domainList) {
            this->pruneDomain(domain);
        }
    }

private:
    struct DomainSelection {
        std::vector<TargetId> ordered;
        std::unordered_set<u64> members;
        TargetId anchor;
        TargetId cursor;
    };

    struct Entry {
        std::size_t id;
        Handler handler;
    };

    [[nodiscard]] auto selectionOf(Domain const& domain) const -> DomainSelection const& {
        auto const found = m_selections.find(std::string(domain.path()));
        if (found == m_selections.end()) {
            return m_empty;
        }

        return found->second;
    }

    [[nodiscard]] auto selectionFor(Domain const& domain) -> DomainSelection& {
        return m_selections[std::string(domain.path())];
    }

    [[nodiscard]] auto memberSet(Domain const& domain) const -> std::unordered_set<u64> const& {
        return this->selectionOf(domain).members;
    }

    [[nodiscard]] auto member(Domain const& domain, TargetId target) const -> bool {
        return this->memberSet(domain).contains(target.packed());
    }

    [[nodiscard]] auto acceptable(Domain const& domain, TargetId target) const -> bool {
        if (!target.valid() || !m_registry.contains(target)) {
            return false;
        }

        auto const resolved = m_registry.find(target);

        return resolved.has_value() && resolved->domain == domain;
    }

    [[nodiscard]] auto registryOrder(Domain const& domain) const -> std::vector<TargetId> {
        return m_registry.order(domain);
    }

    [[nodiscard]] auto anchorOr(Domain const& domain, TargetId fallback) const -> TargetId {
        TargetId const anchor = this->selectionOf(domain).anchor;

        return anchor.valid() ? anchor : fallback;
    }

    [[nodiscard]] auto merge(std::vector<TargetId> const& base, std::vector<TargetId> additions) const -> std::vector<TargetId> {
        std::vector<TargetId> next = base;
        for (TargetId const addition : additions) {
            if (std::find(next.begin(), next.end(), addition) == next.end()) {
                next.push_back(addition);
            }
        }

        return next;
    }

    [[nodiscard]] auto rangeBetween(Domain const& domain, TargetId from, TargetId to) const -> std::vector<TargetId> {
        std::vector<TargetId> const ordered = this->registryOrder(domain);
        auto const fromPosition = std::find(ordered.begin(), ordered.end(), from);
        auto const toPosition = std::find(ordered.begin(), ordered.end(), to);
        if (fromPosition == ordered.end() || toPosition == ordered.end()) {
            return {};
        }

        std::size_t const lower = std::min(
            static_cast<std::size_t>(fromPosition - ordered.begin()),
            static_cast<std::size_t>(toPosition - ordered.begin())
        );
        std::size_t const upper = std::max(
            static_cast<std::size_t>(fromPosition - ordered.begin()),
            static_cast<std::size_t>(toPosition - ordered.begin())
        );

        return std::vector<TargetId>(ordered.begin() + static_cast<std::ptrdiff_t>(lower), ordered.begin() + static_cast<std::ptrdiff_t>(upper) + 1);
    }

    auto applyRange(Domain const& domain, std::vector<TargetId> const& previous, TargetId from, TargetId to, bool additive) -> void {
        std::vector<TargetId> const range = this->rangeBetween(domain, from, to);
        if (range.empty()) {
            return;
        }

        std::vector<TargetId> const next = additive ? this->merge(previous, range) : range;
        this->commit(domain, previous, next, from, to);
    }

    auto pruneDomain(Domain const& domain) -> void {
        DomainSelection& selection = this->selectionFor(domain);
        std::vector<TargetId> const previous = selection.ordered;
        std::erase_if(selection.ordered, [this](TargetId const id) {
            return !m_registry.contains(id);
        });
        if (previous.size() == selection.ordered.size()) {
            return;
        }

        selection.members.clear();
        for (TargetId const id : selection.ordered) {
            selection.members.insert(id.packed());
        }

        if (selection.anchor.valid() && !m_registry.contains(selection.anchor)) {
            selection.anchor = TargetId::invalid();
        }

        if (selection.cursor.valid() && !m_registry.contains(selection.cursor)) {
            selection.cursor = TargetId::invalid();
        }

        std::vector<TargetId> const kept = selection.ordered;
        std::vector<TargetId> const removed = this->difference(previous, kept);
        this->emit(SelectionChange{domain, SelectionChangeKind::Removed, {}, removed});
    }

    auto commit(Domain const& domain, std::vector<TargetId> const& previous, std::vector<TargetId> next, TargetId anchor, TargetId cursor) -> void {
        DomainSelection& selection = this->selectionFor(domain);
        selection.ordered = std::move(next);
        selection.anchor = anchor;
        selection.cursor = cursor;

        selection.members.clear();
        for (TargetId const id : selection.ordered) {
            selection.members.insert(id.packed());
        }

        std::vector<TargetId> const added = this->difference(selection.ordered, previous);
        std::vector<TargetId> const removed = this->difference(previous, selection.ordered);

        SelectionChangeKind kind = SelectionChangeKind::Reset;
        if (!added.empty() && removed.empty()) {
            kind = SelectionChangeKind::Added;
        } else if (added.empty() && !removed.empty()) {
            kind = SelectionChangeKind::Removed;
        }

        this->emit(SelectionChange{domain, kind, added, removed});
    }

    [[nodiscard]] auto difference(std::vector<TargetId> const& left, std::vector<TargetId> const& right) const -> std::vector<TargetId> {
        std::vector<TargetId> result;
        for (TargetId const id : left) {
            if (std::find(right.begin(), right.end(), id) == right.end()) {
                result.push_back(id);
            }
        }

        return result;
    }

    auto emit(SelectionChange const& change) const -> void {
        for (Entry const& entry : m_handlers) {
            entry.handler(change);
        }
    }

    TargetRegistry const& m_registry;
    std::unordered_map<std::string, DomainSelection> m_selections;
    DomainSelection m_empty;
    std::vector<Entry> m_handlers;
    std::size_t m_nextSubscription = 1;
};

} // END OF NAMESPACE `mts::interaction`
