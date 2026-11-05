#pragma once
// VERITY GRAPH — engine/FixedCache: fixed-capacity LRU cache template with a
// NON-TYPE parameter (OOP Unit 4). Used by the ConflictDetector for entity
// lookups (label -> entity id).
//
// Storage is a fixed std::array<pair<K,V>, N>; recency via a tick counter
// (evict least-recently-used slot on insert when full). No allocation, no
// growth — N is baked in at compile time.
//
// Complexity: get O(N) (teaching scale: N is small), put O(N); the point of
// the exercise is the non-type template parameter and the eviction policy,
// not asymptotic speed.

#include <array>
#include <cstddef>
#include <optional>

namespace verity::engine {

template <class K, class V, std::size_t N>
class FixedCache {
    struct Slot {
        K key{};
        V value{};
        std::uint64_t tick = 0;
        bool used = false;
    };

public:
    FixedCache() = default;

    [[nodiscard]] std::optional<V> get(const K& key) {
        Slot* slot = findSlot(key);
        if (slot == nullptr) return std::nullopt;
        slot->tick = ++tick_;
        return slot->value;
    }

    void put(const K& key, V value) {
        if (Slot* slot = findSlot(key)) {
            slot->value = std::move(value);
            slot->tick = ++tick_;
            return;
        }
        // evict LRU slot (or take the first unused one)
        Slot* victim = &slots_[0];
        for (Slot& s : slots_) {
            if (!s.used) { victim = &s; break; }
            if (s.tick < victim->tick) victim = &s;
        }
        victim->key = key;
        victim->value = std::move(value);
        victim->used = true;
        victim->tick = ++tick_;
    }

    [[nodiscard]] std::size_t size() const {
        std::size_t n = 0;
        for (const Slot& s : slots_) n += s.used ? 1u : 0u;
        return n;
    }
    [[nodiscard]] std::size_t capacity() const noexcept { return N; }
    void clear() {
        for (Slot& s : slots_) s.used = false;
        tick_ = 0;
    }

private:
    [[nodiscard]] Slot* findSlot(const K& key) {
        for (Slot& s : slots_) {
            if (s.used && s.key == key) return &s;
        }
        return nullptr;
    }

    std::array<Slot, N> slots_{};
    std::uint64_t tick_ = 0;
};

} // namespace verity::engine
