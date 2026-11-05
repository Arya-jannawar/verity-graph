#pragma once
// VERITY GRAPH — dm/Ledger: teaching-grade tamper-evident evidence ledger
// (DM Unit 4, Phase 4).
//
// Hash chain: h_i = H(record_i || h_{i-1}) where H is a MODULAR polynomial
// hash over a 64-bit prime field. This is explicitly TEACHING-GRADE — it
// demonstrates chain integrity for the classroom; it is NOT cryptographic
// protection (not SHA-256, not collision-resistant against an adversary).
// Every consumer must present it with that label.
//
// verifyLedger recomputes the chain and reports the FIRST mismatching entry.
// Tampering with ANY record (or any stored hash) breaks verification on a
// disposable copy — the pristine file is never touched.
//
// Complexity: append O(1); verify O(n).

#include <cstdint>
#include <string>
#include <vector>

#include "verity/dm/NumTheory.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::dm {

/// Teaching-grade modular hash: polynomial over field 2^61 - 1 (a Mersenne
/// prime) with fixed seeds. Documented as non-cryptographic.
struct LedgerHasher {
    static constexpr std::uint64_t kPrime = 2305843009213693951ULL;   // 2^61 - 1
    static constexpr std::uint64_t kBase = 1315423911ULL;

    [[nodiscard]] std::uint64_t operator()(const std::string& data) const noexcept {
        std::uint64_t h = 1469598103934665603ULL;   // FNV-style offset basis
        for (const unsigned char c : data) {
            h = (h << 7) ^ (h >> 57);   // rotate-mix (NOT cryptographic)
            h = (h + c) % kPrime;
        }
        return h;
    }

    /// Chain step: h_i = H(record || "-" || h_{i-1}).
    [[nodiscard]] std::uint64_t chain(std::uint64_t previous,
                                      const std::string& record) const noexcept {
        const std::string combined =
            record + "-" + std::to_string(previous);
        return operator()(combined);
    }
};

struct LedgerEntry {
    std::string record;    // serialized record payload (teaching text form)
    std::uint64_t hash = 0;
};

class EvidenceLedger {
public:
    explicit EvidenceLedger(LedgerHasher hasher = {}) : hasher_(hasher) {}

    /// Appends a record and chains its hash; genesis hash is 0.
    void append(const std::string& record) {
        const std::uint64_t previous = entries_.empty() ? 0 : entries_.back().hash;
        entries_.push_back(LedgerEntry{record, hasher_.chain(previous, record)});
    }

    /// Recomputes the whole chain; returns the index of the first corrupted
    /// entry, or -1 when the ledger is intact.
    [[nodiscard]] long verify() const {
        std::uint64_t expected = 0;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const std::uint64_t actual = hasher_.chain(expected, entries_[i].record);
            if (actual != entries_[i].hash) return static_cast<long>(i);
            expected = entries_[i].hash;
        }
        return -1;
    }

    [[nodiscard]] const std::vector<LedgerEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

private:
    LedgerHasher hasher_;
    std::vector<LedgerEntry> entries_;
};

} // namespace verity::dm
