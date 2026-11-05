#pragma once
// VERITY GRAPH — ds/Trie: prefix tree for autocomplete (Phase 2).
//
// Case-insensitive by contract: every stored word and query is lowercased
// internally (a-z plus digits, everything else preserved verbatim after
// ASCII-lowercasing). EntityResolver uses prefix blocking through the Trie;
// the CLI autocomplete command serves `query autocomplete`.
//
// Complexity summary (m = word length, k = number of results):
//   insert / search / startsWith   O(m)
//   remove                         O(m)
//   wordsWithPrefix                O(m + k·avg-word-len)
//   clear                          O(total nodes)

#include <cstddef>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace verity::ds {

namespace detail {
[[nodiscard]] inline std::string trieNormalize(const std::string& word) {
    std::string out(word);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}
} // namespace detail

class Trie {
    struct Node {
        std::unique_ptr<Node> children[26]{};
        bool isWord = false;
        std::string original;   // stored verbatim for suggestions
    };

public:
    Trie() = default;

    /// Inserts a word (case-insensitive; non-letter characters are skipped
    /// during the path walk, and the original string is kept at the leaf).
    /// Returns true if newly inserted.
    bool insert(const std::string& word) {
        std::string w;
        w.reserve(word.size());
        for (const char c : detail::trieNormalize(word)) {
            if (c >= 'a' && c <= 'z') w.push_back(c);
        }
        if (w.empty()) return false;
        Node* node = &root_;
        for (const char c : w) {
            const std::size_t idx = charIndex(c);   // guaranteed in [0,26)
            if (!node->children[idx]) node->children[idx] = std::make_unique<Node>();
            node = node->children[idx].get();
        }
        if (node->isWord) return false;
        node->isWord = true;
        node->original = word;
        ++size_;
        return true;
    }

    [[nodiscard]] bool search(const std::string& word) const {
        const Node* node = walk(word);
        return node != nullptr && node->isWord;
    }

    [[nodiscard]] bool startsWith(const std::string& prefix) const {
        return walk(prefix) != nullptr;
    }

    /// Removes a word; returns false when absent. Prunes dead branches.
    /// Uses the same letters-only key normalization as insert.
    bool remove(const std::string& word) {
        std::string w;
        for (const char c : detail::trieNormalize(word)) {
            if (c >= 'a' && c <= 'z') w.push_back(c);
        }
        if (w.empty() || !search(w)) return false;
        removeFrom(root_, w, 0);
        --size_;
        return true;
    }

    /// All stored words beginning with `prefix`, lexicographically sorted.
    [[nodiscard]] std::vector<std::string> wordsWithPrefix(const std::string& prefix) const {
        std::vector<std::string> out;
        const std::string p = detail::trieNormalize(prefix);
        const Node* node = walk(p);
        if (node == nullptr) return out;
        std::string current = p;
        collect(node, current, out);
        std::sort(out.begin(), out.end());
        return out;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    void clear() {
        root_ = Node{};
        size_ = 0;
    }

private:
    [[nodiscard]] static std::size_t charIndex(char c) {
        return static_cast<std::size_t>(c - 'a');
    }

    [[nodiscard]] const Node* walk(const std::string& s) const {
        const Node* node = &root_;
        for (const char c : detail::trieNormalize(s)) {
            if (c < 'a' || c > 'z') continue;   // punctuation is transparent
            const Node* next = node->children[charIndex(c)].get();
            if (next == nullptr) return nullptr;
            node = next;
        }
        return node;
    }

    /// Returns true when `node` (after processing word[at..]) became prunable.
    bool removeFrom(Node& node, const std::string& word, std::size_t at) {
        if (at == word.size()) {
            node.isWord = false;
            return !hasChildren(node);
        }
        const std::size_t idx = charIndex(word[at]);
        Node* child = node.children[idx].get();
        if (child == nullptr) return false;
        if (removeFrom(*child, word, at + 1)) {
            node.children[idx].reset();
            return !node.isWord && !hasChildren(node);
        }
        return false;
    }

    [[nodiscard]] static bool hasChildren(const Node& node) {
        for (const auto& child : node.children) {
            if (child != nullptr) return true;
        }
        return false;
    }

    void collect(const Node* node, std::string& current, std::vector<std::string>& out) const {
        if (node->isWord) out.push_back(node->original.empty() ? current : node->original);
        for (std::size_t i = 0; i < 26; ++i) {
            if (node->children[i] != nullptr) {
                current.push_back(static_cast<char>('a' + i));
                collect(node->children[i].get(), current, out);
                current.pop_back();
            }
        }
    }

    Node root_;
    std::size_t size_ = 0;
};

} // namespace verity::ds
