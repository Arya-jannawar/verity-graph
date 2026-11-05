#pragma once
// VERITY GRAPH — HashTable<K,V>: separate-chaining hash table.
//
// Real engine-path usage: NodeStore id index and ds::Graph node index.
//
// Design (frozen):
//   * Each bucket is ONE of our own LinkedList<std::pair<K,V>> (chaining).
//   * String keys use a POLYNOMIAL ROLLING HASH (h = h*131 + c, 64-bit) via
//     the HashPolicy specialization; integral keys use a multiply/shift mix;
//     everything else falls back to std::hash<K>.
//   * Capacity is always PRIME via verity::dm::nextPrime (strictly-greater),
//     growth at load factor > 0.75, doubling then rounding up to a prime.
//   * Not thread-safe (engine single-threads it; Logger is the only
//     thread-safe component).
//
// Complexity summary (n = size, m = bucketCount):
//   insert / insertOrAssign / get / contains / remove   O(1) average, O(n) worst
//   rehash                                              O(n + m)
//   iteration                                           O(n + m)

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include "verity/ds/LinkedList.hpp"
#include "verity/dm/NumTheory.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::ds {

namespace detail {

/// Hash policy: specialization per key family (frozen design).
template <class K, class Enable = void>
struct HashPolicy {
    [[nodiscard]] std::size_t operator()(const K& key) const noexcept(
        noexcept(std::hash<K>{}(key))) {
        return std::hash<K>{}(key);
    }
};

/// std::string keys: polynomial rolling hash (frozen constant 131).
template <class K>
struct HashPolicy<K, std::enable_if_t<std::is_same_v<K, std::string>>> {
    [[nodiscard]] std::size_t operator()(const std::string& key) const noexcept {
        std::uint64_t h = 0;
        for (const char c : key) {
            h = h * 131ULL + static_cast<unsigned char>(c);
        }
        return static_cast<std::size_t>(h);
    }
};

/// Integral keys: multiply-by-prime + xor-shift mix.
template <class K>
struct HashPolicy<K, std::enable_if_t<std::is_integral_v<K>>> {
    [[nodiscard]] std::size_t operator()(const K& key) const noexcept {
        std::uint64_t x = static_cast<std::uint64_t>(key);
        x *= 0x9E3779B97F4A7C15ULL;
        x ^= x >> 29;
        x *= 0xBF58476D1CE4E5B9ULL;
        x ^= x >> 32;
        return static_cast<std::size_t>(x);
    }
};

} // namespace detail

template <class K, class V, class Hash = detail::HashPolicy<K>>
class HashTable {
    using Entry = std::pair<K, V>;

public:
    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Entry;
        using difference_type = std::ptrdiff_t;
        using pointer = const Entry*;
        using reference = const Entry&;

        const_iterator(const HashTable* table, std::size_t bucket,
                       typename LinkedList<Entry>::const_iterator it) noexcept
            : table_(table), bucket_(bucket), it_(std::move(it)) {}

        reference operator*() const noexcept { return *it_; }
        pointer operator->() const noexcept { return &*it_; }

        const_iterator& operator++() {
            ++it_;
            while (bucket_ < table_->buckets_.size() && it_ == table_->buckets_[bucket_].end()) {
                ++bucket_;
                if (bucket_ < table_->buckets_.size()) {
                    it_ = table_->buckets_[bucket_].begin();
                }
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp(*this);
            ++(*this);
            return tmp;
        }

        [[nodiscard]] bool operator==(const const_iterator& o) const noexcept {
            return table_ == o.table_ && bucket_ == o.bucket_ && it_ == o.it_;
        }
        [[nodiscard]] bool operator!=(const const_iterator& o) const noexcept {
            return !(*this == o);
        }

    private:
        const HashTable* table_;
        std::size_t bucket_;
        typename LinkedList<Entry>::const_iterator it_;
    };

    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Entry;
        using difference_type = std::ptrdiff_t;
        using pointer = Entry*;
        using reference = Entry&;

        iterator(HashTable* table, std::size_t bucket,
                 typename LinkedList<Entry>::iterator it) noexcept
            : table_(table), bucket_(bucket), it_(std::move(it)) {}

        reference operator*() const noexcept { return *it_; }
        pointer operator->() const noexcept { return &*it_; }

        iterator& operator++() {
            ++it_;
            while (bucket_ < table_->buckets_.size() && it_ == table_->buckets_[bucket_].end()) {
                ++bucket_;
                if (bucket_ < table_->buckets_.size()) {
                    it_ = table_->buckets_[bucket_].begin();
                }
            }
            return *this;
        }

        iterator operator++(int) {
            iterator tmp(*this);
            ++(*this);
            return tmp;
        }

        [[nodiscard]] bool operator==(const iterator& o) const noexcept {
            return table_ == o.table_ && bucket_ == o.bucket_ && it_ == o.it_;
        }
        [[nodiscard]] bool operator!=(const iterator& o) const noexcept { return !(*this == o); }

        operator const_iterator() const noexcept {
            return const_iterator(table_, bucket_,
                                  typename LinkedList<Entry>::const_iterator(it_));
        }

    private:
        HashTable* table_;
        std::size_t bucket_;
        typename LinkedList<Entry>::iterator it_;
    };

    HashTable() { buckets_.resize(kInitialBuckets); }

    explicit HashTable(std::size_t expectedSize) {
        const std::size_t need = expectedSize + expectedSize / 2 + 2;  // lf<=0.66
        buckets_.resize(static_cast<std::size_t>(dm::nextPrime(need)));
    }

    // Move leaves the source EMPTY BUT VALID (prime-sized, zero buckets
    // would break % bucketCount() in bucketOf).
    HashTable(HashTable&& other) noexcept
        : buckets_(std::move(other.buckets_)), size_(other.size_) {
        other.buckets_.clear();
        other.buckets_.resize(kInitialBuckets);
        other.size_ = 0;
    }

    HashTable& operator=(HashTable&& other) noexcept {
        if (this != &other) {
            buckets_ = std::move(other.buckets_);
            size_ = other.size_;
            other.buckets_.clear();
            other.buckets_.resize(kInitialBuckets);
            other.size_ = 0;
        }
        return *this;
    }

    HashTable(const HashTable& other) = default;
    HashTable& operator=(const HashTable& other) = default;

    /// Inserts `value` under `key`; returns false if the key already exists.
    bool insert(const K& key, V value) {
        if (contains(key)) return false;
        maybeGrow();
        auto& chain = buckets_[bucketOf(key)];
        chain.push_back(Entry(key, std::move(value)));
        ++size_;
        return true;
    }

    /// Inserts or overwrites; returns true if newly inserted.
    bool insertOrAssign(const K& key, V value) {
        if (V* existing = get(key)) {
            *existing = std::move(value);
            return false;
        }
        return insert(key, std::move(value));
    }

    /// Pointer to the value, or nullptr. Mutating the KEY via returned entry
    /// internals is forbidden (value pointer only).
    [[nodiscard]] V* get(const K& key) noexcept {
        auto& chain = buckets_[bucketOf(key)];
        for (auto& entry : chain) {
            if (entry.first == key) return &entry.second;
        }
        return nullptr;
    }
    [[nodiscard]] const V* get(const K& key) const noexcept {
        const auto& chain = buckets_[bucketOf(key)];
        for (const auto& entry : chain) {
            if (entry.first == key) return &entry.second;
        }
        return nullptr;
    }

    /// Reference to the value; throws VerityError when the key is missing.
    [[nodiscard]] V& at(const K& key) {
        if (V* v = get(key)) return *v;
        throw util::VerityError("HashTable key not found");
    }
    [[nodiscard]] const V& at(const K& key) const {
        if (const V* v = get(key)) return *v;
        throw util::VerityError("HashTable key not found");
    }

    [[nodiscard]] bool contains(const K& key) const noexcept { return get(key) != nullptr; }

    /// Removes `key`; returns true when it existed.
    bool remove(const K& key) {
        auto& chain = buckets_[bucketOf(key)];
        const bool removed =
            chain.removeFirstMatching([&key](const Entry& e) { return e.first == key; });
        if (removed) --size_;
        return removed;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::size_t bucketCount() const noexcept { return buckets_.size(); }

    [[nodiscard]] double loadFactor() const noexcept {
        return buckets_.empty() ? 0.0 : static_cast<double>(size_) / static_cast<double>(buckets_.size());
    }

    void clear() noexcept {
        for (auto& chain : buckets_) chain.clear();
        size_ = 0;
    }

    /// Rebuilds the table with at least `newBucketCount` buckets (rounded up
    /// to a prime). All elements are rehashed.
    void rehash(std::size_t newBucketCount) {
        const std::size_t target =
            static_cast<std::size_t>(dm::nextPrime(newBucketCount > 2 ? newBucketCount : 2));
        if (target == buckets_.size()) return;
        std::vector<LinkedList<Entry>> fresh(target);
        for (auto& chain : buckets_) {
            for (auto& entry : chain) {
                const std::size_t b = static_cast<std::size_t>(hash_(entry.first)) % target;
                fresh[b].push_back(std::move(entry));
            }
        }
        buckets_.swap(fresh);
    }

    iterator begin() noexcept {
        std::size_t b = 0;
        while (b < buckets_.size() && buckets_[b].empty()) ++b;
        if (b >= buckets_.size()) return iterator(this, buckets_.size(), typename LinkedList<Entry>::iterator());
        return iterator(this, b, buckets_[b].begin());
    }
    iterator end() noexcept { return iterator(this, buckets_.size(), typename LinkedList<Entry>::iterator()); }

    const_iterator begin() const noexcept {
        std::size_t b = 0;
        while (b < buckets_.size() && buckets_[b].empty()) ++b;
        if (b >= buckets_.size()) return const_iterator(this, buckets_.size(), typename LinkedList<Entry>::const_iterator());
        return const_iterator(this, b, buckets_[b].begin());
    }
    const_iterator end() const noexcept {
        return const_iterator(this, buckets_.size(), typename LinkedList<Entry>::const_iterator());
    }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

private:
    static constexpr double kMaxLoadFactor = 0.75;
    static constexpr std::size_t kInitialBuckets = 11;   // prime

    [[nodiscard]] std::size_t bucketOf(const K& key) const noexcept {
        return static_cast<std::size_t>(hash_(key)) % buckets_.size();
    }

    void maybeGrow() {
        const double lf = static_cast<double>(size_ + 1) / static_cast<double>(buckets_.size());
        if (lf > kMaxLoadFactor) {
            rehash(buckets_.size() * 2);
        }
    }

    std::vector<LinkedList<Entry>> buckets_;
    std::size_t size_ = 0;
    Hash hash_{};   // stateless policies take 1 byte; [[no_unique_address]] is C++20
};

} // namespace verity::ds
