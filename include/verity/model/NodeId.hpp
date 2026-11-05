#pragma once
// VERITY GRAPH — NodeId: strong wrapper around a stable string ID.
//
// Frozen format (docs/CONTRACT.md §2):
//   E###   entities      EV###  events      C###  claims      D###  documents
//   digits are zero-padded to at least 3. Storage/CLI/fixtures always use the
//   padded form. A shorter numeric part ("D6") is accepted on *input* and
//   canonicalized to "D006" so hand-written gold files are forgiving while
//   canonical form stays unique. Display-only unpadded rendering is a UI
//   concern and documented in SAMPLE_DATA.md.
//
// Complexity: validation/canonicalization O(n) in id length; comparisons O(n).

#include <cctype>
#include <string>
#include <utility>

#include "verity/util/VerityError.hpp"

namespace verity::model {

class NodeId {
public:
    // Default constructor is deliberately absent: an ID is always explicit.
    explicit NodeId(const std::string& raw) : raw_(canonicalize(raw)) {}
    explicit NodeId(const char* raw) : NodeId(std::string(raw)) {}

    [[nodiscard]] const std::string& str() const noexcept { return raw_; }
    [[nodiscard]] char prefix() const noexcept { return raw_.empty() ? '\0' : raw_[0]; }

    [[nodiscard]] bool operator==(const NodeId& other) const noexcept { return raw_ == other.raw_; }
    [[nodiscard]] bool operator!=(const NodeId& other) const noexcept { return raw_ != other.raw_; }
    [[nodiscard]] bool operator<(const NodeId& other) const noexcept { return raw_ < other.raw_; }
    [[nodiscard]] bool operator<=(const NodeId& other) const noexcept { return raw_ <= other.raw_; }
    [[nodiscard]] bool operator>(const NodeId& other) const noexcept { return raw_ > other.raw_; }
    [[nodiscard]] bool operator>=(const NodeId& other) const noexcept { return raw_ >= other.raw_; }

private:
    // Validates <prefix><digits> and canonicalizes the numeric part to >=3
    // digits. Throws ParseError on malformed input.
    static std::string canonicalize(const std::string& raw) {
        if (raw.size() < 2) {
            throw util::ParseError("NodeId too short: '" + raw + "'");
        }
        std::size_t digits_start = 0;
        if (raw.size() >= 3 && raw[0] == 'E' && raw[1] == 'V') {
            digits_start = 2;   // two-letter prefix must be checked first
        } else if (raw[0] == 'E' || raw[0] == 'C' || raw[0] == 'D') {
            digits_start = 1;
        } else {
            throw util::ParseError(
                "NodeId must start with E, EV, C or D: '" + raw + "'");
        }
        if (digits_start >= raw.size()) {
            throw util::ParseError("NodeId missing numeric part: '" + raw + "'");
        }
        for (std::size_t i = digits_start; i < raw.size(); ++i) {
            if (std::isdigit(static_cast<unsigned char>(raw[i])) == 0) {
                throw util::ParseError("NodeId has non-digit character: '" + raw + "'");
            }
        }
        std::string digits = raw.substr(digits_start);
        std::size_t first_non_zero = digits.find_first_not_of('0');
        if (first_non_zero == std::string::npos) {
            throw util::ParseError("NodeId number cannot be all zeros: '" + raw + "'");
        }
        digits = digits.substr(first_non_zero);
        while (digits.size() < 3) digits.insert(digits.begin(), '0');
        return raw.substr(0, digits_start) + digits;
    }

    std::string raw_;
};

} // namespace verity::model

namespace std {
template <>
struct hash<verity::model::NodeId> {
    [[nodiscard]] std::size_t operator()(const verity::model::NodeId& id) const noexcept {
        return std::hash<std::string>{}(id.str());
    }
};
} // namespace std
