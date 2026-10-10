export module interaction:registry;

import std;
import :model;

export namespace mts::interaction {

using HitTestProvider = std::function<std::vector<TargetId>(Point const&)>;

class TargetRegistry {
public:
    TargetRegistry() = default;

    TargetRegistry(TargetRegistry const&) = delete;
    auto operator=(TargetRegistry const&) -> TargetRegistry& = delete;
    TargetRegistry(TargetRegistry&&) = default;
    auto operator=(TargetRegistry&&) -> TargetRegistry& = default;

    auto attach(TargetDescriptor descriptor) -> TargetId;
    auto detach(TargetId id) -> void;
    auto rekey(TargetId id, TargetDescriptor descriptor) -> void;
    auto updateGeometry(TargetId id, Rect geometry) -> void;
    auto updateCapabilities(TargetId id, Capabilities capabilities) -> void;

    [[nodiscard]] auto contains(TargetId id) const -> bool;
    [[nodiscard]] auto find(TargetId id) const -> std::optional<TargetRef>;
    [[nodiscard]] auto size() const -> std::size_t;
    [[nodiscard]] auto targets() const -> std::vector<TargetRef>;
    [[nodiscard]] auto byDomain(Domain const& domain) const -> std::vector<TargetRef>;
    [[nodiscard]] auto byRole(Role const& role) const -> std::vector<TargetRef>;
    [[nodiscard]] auto order(Domain const& domain) const -> std::vector<TargetId>;

    auto addHitTestProvider(HitTestProvider provider) -> std::size_t;
    auto removeHitTestProvider(std::size_t token) -> void;
    [[nodiscard]] auto hitTest(Point const& point) const -> std::vector<TargetRef>;

private:
    struct Entry {
        TargetId id;
        TargetDescriptor descriptor;
        u64 sequence = 0;
    };

    [[nodiscard]] auto findEntry(TargetId id) const -> Entry const*;

    template <typename Predicate>
    [[nodiscard]] auto collect(Predicate predicate) const -> std::vector<TargetRef>;

    std::unordered_map<u64, Entry> m_entries;
    std::vector<u32> m_freeIndices;
    std::unordered_map<u32, u32> m_generations;
    u32 m_nextIndex = 1;
    u64 m_nextSequence = 1;
    std::vector<HitTestProvider> m_hitTestProviders;
    std::vector<std::size_t> m_hitTestTokens;
    std::size_t m_nextHitTestToken = 1;
};

inline auto TargetRegistry::findEntry(TargetId id) const -> Entry const* {
    if (!id.valid()) {
        return nullptr;
    }

    auto const found = this->m_entries.find(id.packed());
    if (found == this->m_entries.end()) {
        return nullptr;
    }

    return &found->second;
}

inline auto TargetRegistry::attach(TargetDescriptor descriptor) -> TargetId {
    u32 index = 0;
    if (!this->m_freeIndices.empty()) {
        index = this->m_freeIndices.back();
        this->m_freeIndices.pop_back();
    } else {
        index = this->m_nextIndex++;
        this->m_generations.emplace(index, 1);
    }

    u32 const generation = this->m_generations.at(index);
    TargetId const id{index, generation};

    Entry entry;
    entry.id = id;
    entry.descriptor = std::move(descriptor);
    entry.sequence = this->m_nextSequence++;
    this->m_entries.emplace(id.packed(), std::move(entry));

    return id;
}

inline auto TargetRegistry::detach(TargetId id) -> void {
    Entry const* entry = this->findEntry(id);
    if (entry == nullptr) {
        return;
    }

    u32 const index = entry->id.index;
    this->m_entries.erase(id.packed());
    this->m_generations[index] = this->m_generations.at(index) + 1;
    this->m_freeIndices.push_back(index);
}

inline auto TargetRegistry::rekey(TargetId id, TargetDescriptor descriptor) -> void {
    auto const found = this->m_entries.find(id.packed());
    if (found == this->m_entries.end() || found->second.id.generation != id.generation) {
        return;
    }

    found->second.descriptor = std::move(descriptor);
}

inline auto TargetRegistry::updateGeometry(TargetId id, Rect geometry) -> void {
    auto const found = this->m_entries.find(id.packed());
    if (found == this->m_entries.end() || found->second.id.generation != id.generation) {
        return;
    }

    found->second.descriptor.geometry = geometry;
}

inline auto TargetRegistry::updateCapabilities(TargetId id, Capabilities capabilities) -> void {
    auto const found = this->m_entries.find(id.packed());
    if (found == this->m_entries.end() || found->second.id.generation != id.generation) {
        return;
    }

    found->second.descriptor.capabilities = capabilities;
}

inline auto TargetRegistry::contains(TargetId id) const -> bool {
    return this->findEntry(id) != nullptr;
}

inline auto TargetRegistry::find(TargetId id) const -> std::optional<TargetRef> {
    Entry const* entry = this->findEntry(id);
    if (entry == nullptr) {
        return std::nullopt;
    }

    TargetRef reference;
    reference.id = entry->id;
    reference.domain = entry->descriptor.domain;
    reference.role = entry->descriptor.role;
    reference.capabilities = entry->descriptor.capabilities;
    reference.geometry = entry->descriptor.geometry;
    reference.visible = entry->descriptor.visible;
    reference.enabled = entry->descriptor.enabled;
    reference.label = entry->descriptor.label;
    reference.group = entry->descriptor.group;
    reference.semanticKey = entry->descriptor.semanticKey;
    reference.order = entry->descriptor.order;

    return reference;
}

inline auto TargetRegistry::size() const -> std::size_t {
    return this->m_entries.size();
}

template <typename Predicate>
inline auto TargetRegistry::collect(Predicate predicate) const -> std::vector<TargetRef> {
    std::vector<Entry const*> entries;
    entries.reserve(this->m_entries.size());
    for (auto const& [key, entry] : this->m_entries) {
        if (predicate(entry)) {
            entries.push_back(&entry);
        }
    }

    std::sort(entries.begin(), entries.end(), [](Entry const* left, Entry const* right) {
        return left->sequence < right->sequence;
    });

    std::vector<TargetRef> result;
    result.reserve(entries.size());
    for (Entry const* entry : entries) {
        if (auto const reference = this->find(entry->id); reference.has_value()) {
            result.push_back(*reference);
        }
    }

    return result;
}

inline auto TargetRegistry::targets() const -> std::vector<TargetRef> {
    return this->collect([](Entry const&) { return true; });
}

inline auto TargetRegistry::byDomain(Domain const& domain) const -> std::vector<TargetRef> {
    return this->collect([&domain](Entry const& entry) {
        return entry.descriptor.domain == domain;
    });
}

inline auto TargetRegistry::byRole(Role const& role) const -> std::vector<TargetRef> {
    return this->collect([&role](Entry const& entry) {
        return entry.descriptor.role == role;
    });
}

inline auto TargetRegistry::order(Domain const& domain) const -> std::vector<TargetId> {
    std::vector<Entry const*> entries;
    for (auto const& [key, entry] : this->m_entries) {
        if (entry.descriptor.domain == domain) {
            entries.push_back(&entry);
        }
    }

    std::sort(entries.begin(), entries.end(), [](Entry const* left, Entry const* right) {
        if (left->descriptor.order != right->descriptor.order) {
            return left->descriptor.order < right->descriptor.order;
        }

        return left->sequence < right->sequence;
    });

    std::vector<TargetId> result;
    result.reserve(entries.size());
    for (Entry const* entry : entries) {
        result.push_back(entry->id);
    }

    return result;
}

inline auto TargetRegistry::addHitTestProvider(HitTestProvider provider) -> std::size_t {
    std::size_t const token = this->m_nextHitTestToken++;
    this->m_hitTestProviders.push_back(std::move(provider));
    this->m_hitTestTokens.push_back(token);

    return token;
}

inline auto TargetRegistry::removeHitTestProvider(std::size_t token) -> void {
    for (std::size_t position = 0; position < this->m_hitTestTokens.size(); ++position) {
        if (this->m_hitTestTokens[position] != token) {
            continue;
        }

        this->m_hitTestTokens.erase(this->m_hitTestTokens.begin() + static_cast<std::ptrdiff_t>(position));
        this->m_hitTestProviders.erase(this->m_hitTestProviders.begin() + static_cast<std::ptrdiff_t>(position));

        return;
    }
}

inline auto TargetRegistry::hitTest(Point const& point) const -> std::vector<TargetRef> {
    std::vector<TargetRef> result;
    std::unordered_set<u64> seen;

    for (auto const& provider : std::views::reverse(this->m_hitTestProviders)) {
        for (TargetId const id : provider(point)) {
            if (!seen.insert(id.packed()).second) {
                continue;
            }

            if (auto const reference = this->find(id); reference.has_value()) {
                result.push_back(*reference);
            }
        }
    }

    return result;
}

} // END OF NAMESPACE `mts::interaction`
