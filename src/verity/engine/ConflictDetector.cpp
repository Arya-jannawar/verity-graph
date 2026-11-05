#include "verity/engine/ConflictDetector.hpp"

#include <algorithm>
#include <set>
#include <sstream>

#include <iostream>

#include "verity/dm/NoisyOr.hpp"

namespace verity::engine {

namespace {

/// Normalized comparable value of a claim object ("E002", "Bellhaven", "3 kg").
std::string objectValue(const model::ClaimObject& o) {
    switch (o.kind) {
        case model::ClaimObject::Kind::EntityRef:   return o.entity_id;
        case model::ClaimObject::Kind::StringValue: return o.text;
        case model::ClaimObject::Kind::DateValue:   return o.date.toString();
        case model::ClaimObject::Kind::QuantityValue:
            return o.unit + ":" + std::to_string(o.quantity);
    }
    return "";
}

/// Do two object values genuinely clash? DateValue pairs clash only when
/// their precision-preserving intervals are DISJOINT: a month and a day
/// inside that month overlap and are NOT a contradiction (CONTRACT 10.2).
bool valuesConflict(const model::ClaimObject& a, const model::ClaimObject& b) {
    if (a.kind == model::ClaimObject::Kind::DateValue &&
        b.kind == model::ClaimObject::Kind::DateValue) {
        return !a.date.overlaps(b.date);
    }
    return objectValue(a) != objectValue(b);
}

/// Optional valid-time interval of a claim. A lone valid_from means OPEN-ENDED
/// from that date ("since 2022-01" has no end); a lone valid_to means open at
/// the start. One-sided bounds expand to full-precision calendar units so
/// "since 2022-01" covers [2022-01-01, +inf) rather than a single month.
std::optional<model::DateInterval> validTimeOf(const model::ClaimRecord& c) {
    if (!c.valid_from && !c.valid_to) return std::nullopt;
    if (c.valid_from && c.valid_to) {
        return model::DateInterval::fromJson(
            nlohmann::json{{"from", *c.valid_from}, {"to", *c.valid_to}});
    }
    const model::DatePoint kFar{9999, 12, 31};
    const model::DatePoint kPast{1, 1, 1};
    if (c.valid_from) {
        const model::DateInterval start = model::DateInterval::fromString(*c.valid_from);
        const model::DateInterval expanded =
            start.precision == model::DatePrecision::Day
                ? start
                : model::DateInterval::fromPoint(start.start);
        model::DateInterval out = expanded;
        out.precision = model::DatePrecision::Day;   // exact bounds below
        out.end = kFar;                              // open-ended future
        return out;
    }
    const model::DateInterval end = model::DateInterval::fromString(*c.valid_to);
    const model::DateInterval expanded =
        end.precision == model::DatePrecision::Day
            ? end
            : model::DateInterval::fromPoint(end.start);
    model::DateInterval out = expanded;
    out.precision = model::DatePrecision::Day;   // exact bounds below
    out.start = kPast;                           // open-ended past
    return out;
}

/// Two claims' validity intervals overlap (absent intervals count as
/// unbounded, i.e. always overlapping — closed-world teaching assumption
/// documented in the benign explanations).
bool validityOverlaps(const model::ClaimRecord& a, const model::ClaimRecord& b) {
    const auto ta = validTimeOf(a);
    const auto tb = validTimeOf(b);
    if (!ta || !tb) return true;   // unbounded side(s)
    return ta->overlaps(*tb);
}

Conflict makeConflict(std::string rule, std::string statement, const model::ClaimRecord& a,
                      const model::ClaimRecord& b, std::string subjectId, std::string severity,
                      std::string benign) {
    Conflict finding;
    finding.rule = std::move(rule);
    finding.statement = std::move(statement);
    finding.claimA = a.id;
    finding.claimB = b.id;
    finding.subjectId = std::move(subjectId);
    finding.predicate = a.predicate;
    std::set<std::string> docs{a.document_id, b.document_id};
    finding.documentIds.assign(docs.begin(), docs.end());
    for (const model::ClaimRecord* c : {&a, &b}) {
        Conflict::Excerpt excerpt;
        excerpt.documentId = c->document_id;
        excerpt.start = c->span.start;
        excerpt.end = c->span.end;
        excerpt.quote = c->span.quote;
        finding.excerpts.push_back(std::move(excerpt));
    }
    finding.severity = std::move(severity);
    finding.confidence = verity::dm::noisyOrAggregate({a.confidence, b.confidence});
    finding.benignExplanation = std::move(benign);
    return finding;
}

// ---------------------------------------------------------------------------
// Concrete strategies
// ---------------------------------------------------------------------------

/// Same canonical subject + FUNCTIONAL predicate (has_ceo, located_at) +
/// different object values on overlapping validity intervals.
class FunctionalPredicateRule final : public ConflictRule {
public:
    FunctionalPredicateRule(std::string predicate, std::string flag)
        : ConflictRule("FunctionalPredicateRule"), predicate_(std::move(predicate)),
          overlap_(flag == "overlap") {}

    void detect(const DetectionContext& context, std::vector<Conflict>& findings) const override {
        if (predicate_ != "any") {
            // single-predicate mode: scan only that (subject, predicate) group
            detectGroup(context, predicate_, findings);
            return;
        }
        for (const auto& [key, claimIds] : context.claimIndex) {
            detectKey(context, key.second, key.first, claimIds, findings);
        }
    }

private:
    void detectGroup(const DetectionContext& context, const std::string& predicate,
                     std::vector<Conflict>& findings) const {
        for (const auto& [key, claimIds] : context.claimIndex) {
            if (key.second != predicate) continue;
            detectKey(context, predicate, key.first, claimIds, findings);
        }
    }

    void detectKey(const DetectionContext& context, const std::string& /*predicate*/,
                   const std::string& subject, const std::vector<std::string>& claimIds,
                   std::vector<Conflict>& findings) const {
        // canonical subject grouping: only compare claims whose CANONICAL
        // subject matches this group's canonical subject
        std::vector<const model::ClaimRecord*> claims;
        for (const std::string& id : claimIds) {
            const auto* claim = context.claimById.at(id);
            if (context.canonicalOf(claim->subject_id) == context.canonicalOf(subject)) {
                claims.push_back(claim);
            }
        }
        for (std::size_t i = 0; i < claims.size(); ++i) {
            for (std::size_t j = i + 1; j < claims.size(); ++j) {
                const model::ClaimRecord& a = *claims[i];
                const model::ClaimRecord& b = *claims[j];
                // polarity Unknown is preserved, never treated as a clash
                // only two POSITIVE claims with different same-kind values
                // clash here; negative/unknown polarity is NegationRule's or
                // is preserved (CONTRACT 10.4)
                if (a.polarity != "Positive" || b.polarity != "Positive") continue;
                if (a.object.kind != b.object.kind) continue;
                // QuantityValue pairs belong to NumericMismatchRule
                if (a.object.kind == model::ClaimObject::Kind::QuantityValue) continue;
                if (!valuesConflict(a.object, b.object)) continue;
                if (overlap_ && !validityOverlaps(a, b)) continue;
                findings.push_back(makeConflict(
                    name(), "Potential contradiction", a, b,
                    context.canonicalOf(subject), "high",
                    "Different values for a functional predicate are only contradictory when "
                    "both hold in the same interval; disjoint validity times are consistent, "
                    "and unrelated predicates (announced vs launched) are never compared."));
            }
        }
    }

    std::string predicate_;
    bool overlap_;
};

/// Same subject + predicate + object value with OPPOSITE polarity on
/// overlapping intervals (Positive vs Negative). Unknown never matches.
class NegationRule final : public ConflictRule {
public:
    NegationRule(std::string predicate, std::string flag)
        : ConflictRule("NegationRule"), predicate_(std::move(predicate)),
          overlap_(flag == "overlap") {}

    void detect(const DetectionContext& context, std::vector<Conflict>& findings) const override {
        for (const auto& [key, claimIds] : context.claimIndex) {
            if (predicate_ != "any" && key.second != predicate_) continue;
            std::vector<const model::ClaimRecord*> claims;
            for (const std::string& id : claimIds) {
                claims.push_back(context.claimById.at(id));
            }
            for (std::size_t i = 0; i < claims.size(); ++i) {
                for (std::size_t j = i + 1; j < claims.size(); ++j) {
                    const model::ClaimRecord& a = *claims[i];
                    const model::ClaimRecord& b = *claims[j];
                    const bool opposite = (a.polarity == "Positive" && b.polarity == "Negative") ||
                                          (a.polarity == "Negative" && b.polarity == "Positive");
                    if (!opposite) continue;   // Unknown preserved, never a clash
                    if (objectValue(a.object) != objectValue(b.object)) continue;
                    if (context.canonicalOf(a.subject_id) != context.canonicalOf(b.subject_id)) {
                        continue;
                    }
                    if (overlap_ && !validityOverlaps(a, b)) continue;
                    findings.push_back(makeConflict(
                        name(), "Potential contradiction", a, b,
                        context.canonicalOf(a.subject_id), "high",
                        "A polarity flip often reflects a retraction or correction timeline; "
                        "the sources may disagree about WHEN rather than WHETHER."));
                }
            }
        }
    }

private:
    std::string predicate_;
    bool overlap_;
};

/// Quantity claims with the SAME unit but different amounts (overlapping).
class NumericMismatchRule final : public ConflictRule {
public:
    NumericMismatchRule(std::string predicate, std::string flag)
        : ConflictRule("NumericMismatchRule"), predicate_(std::move(predicate)),
          sameUnit_(flag == "same_unit") {}

    void detect(const DetectionContext& context, std::vector<Conflict>& findings) const override {
        for (const auto& [key, claimIds] : context.claimIndex) {
            if (predicate_ != "any" && key.second != predicate_) continue;
            std::vector<const model::ClaimRecord*> claims;
            for (const std::string& id : claimIds) {
                claims.push_back(context.claimById.at(id));
            }
            for (std::size_t i = 0; i < claims.size(); ++i) {
                for (std::size_t j = i + 1; j < claims.size(); ++j) {
                    const model::ClaimRecord& a = *claims[i];
                    const model::ClaimRecord& b = *claims[j];
                    if (a.object.kind != model::ClaimObject::Kind::QuantityValue ||
                        b.object.kind != model::ClaimObject::Kind::QuantityValue) {
                        continue;
                    }
                    if (sameUnit_ && a.object.unit != b.object.unit) continue;
                    if (a.object.quantity == b.object.quantity) continue;
                    if (context.canonicalOf(a.subject_id) != context.canonicalOf(b.subject_id)) {
                        continue;
                    }
                    if (!validityOverlaps(a, b)) continue;
                    findings.push_back(makeConflict(
                        name(), "Potential contradiction", a, b,
                        context.canonicalOf(a.subject_id), "medium",
                        "Quantities with different units are never compared; same-unit "
                        "mismatches may reflect projections, estimates or amended reports."));
                }
            }
        }
    }

private:
    std::string predicate_;
    bool sameUnit_;
};

/// present_at claims placing one person at different locations with a
/// justified temporal overlap (the alibi check; policy documented).
class LocationTimeRule final : public ConflictRule {
public:
    LocationTimeRule(std::string predicate, std::string flag)
        : ConflictRule("LocationTimeRule"), predicate_(std::move(predicate)),
          overlap_(flag == "overlap") {}

    void detect(const DetectionContext& context, std::vector<Conflict>& findings) const override {
        for (const auto& [key, claimIds] : context.claimIndex) {
            if (key.second != predicate_) continue;
            std::vector<const model::ClaimRecord*> claims;
            for (const std::string& id : claimIds) {
                claims.push_back(context.claimById.at(id));
            }
            for (std::size_t i = 0; i < claims.size(); ++i) {
                for (std::size_t j = i + 1; j < claims.size(); ++j) {
                    const model::ClaimRecord& a = *claims[i];
                    const model::ClaimRecord& b = *claims[j];
                    if (a.object.kind != model::ClaimObject::Kind::StringValue ||
                        b.object.kind != model::ClaimObject::Kind::StringValue) {
                        continue;
                    }
                    if (a.object.text == b.object.text) continue;
                    if (context.canonicalOf(a.subject_id) != context.canonicalOf(b.subject_id)) {
                        continue;
                    }
                    if (a.polarity == "Unknown" || b.polarity == "Unknown") continue;
                    if (overlap_ && !validityOverlaps(a, b)) continue;
                    findings.push_back(makeConflict(
                        name(), "Potential contradiction", a, b,
                        context.canonicalOf(a.subject_id), "high",
                        "Presence findings require a justified temporal overlap; the engine "
                        "does NOT model travel time, so same-day presence in two places may "
                        "still be physically possible."));
                }
            }
        }
    }

private:
    std::string predicate_;
    bool overlap_;
};

/// The planted happened-before cycle becomes a temporal finding with the
/// witness path and the responsible claim ids.
class TemporalCycleRule final : public ConflictRule {
public:
    explicit TemporalCycleRule(std::string predicate)
        : ConflictRule("TemporalCycleRule"), predicate_(std::move(predicate)) {}

    void detect(const DetectionContext& context, std::vector<Conflict>& findings) const override {
        if (!context.timeline || !context.timeline->finding.has_value()) return;
        const TimelineFinding& f = *context.timeline->finding;
        Conflict finding;
        finding.rule = name();
        finding.statement = f.statement;   // "Potential temporal contradiction"
        finding.subjectId = f.witness.empty() ? "" : f.witness.front();
        finding.predicate = predicate_;
        // claims on the cycle become "claimA" entries (claimB empty)
        if (!f.claimIds.empty()) finding.claimA = f.claimIds.front();
        for (const std::string& claimId : f.claimIds) {
            const auto it = context.claimById.find(claimId);
            if (it == context.claimById.end()) continue;
            const model::ClaimRecord* claim = it->second;
            finding.documentIds.push_back(claim->document_id);
            Conflict::Excerpt excerpt;
            excerpt.documentId = claim->document_id;
            excerpt.start = claim->span.start;
            excerpt.end = claim->span.end;
            excerpt.quote = claim->span.quote;
            finding.excerpts.push_back(std::move(excerpt));
        }
        finding.derivation.push_back("cycle witness: " + [&f] {
            std::string path;
            for (const std::string& id : f.witness) path += (path.empty() ? "" : " -> ") + id;
            return path;
        }());
        for (const std::string& claimId : f.claimIds) {
            finding.derivation.push_back("happened_before claim on the cycle: " + claimId);
        }
        finding.severity = "medium";
        finding.confidence = 0.9;
        finding.benignExplanation =
            "The happened-before claims may encode different framings of the same milestone; "
            "the witness path shows the exact claims to inspect. No Hasse diagram or topological "
            "order is produced for cyclic data.";
        findings.push_back(std::move(finding));
    }

private:
    std::string predicate_;
};

} // namespace

ConflictDetector::ConflictDetector() = default;

void ConflictDetector::addRule(std::unique_ptr<ConflictRule> rule) {
    rules_.push_back(std::move(rule));
}

void ConflictDetector::addListener(ConflictListener* listener) {
    listeners_.push_back(listener);
}

std::vector<Conflict> ConflictDetector::run(const DetectionContext& context) const {
    std::vector<Conflict> findings;
    for (const auto& rule : rules_) {
        rule->detect(context, findings);
    }
    // deterministic order, then stable ids (X001...)
    std::sort(findings.begin(), findings.end(), [](const Conflict& a, const Conflict& b) {
        if (a.rule != b.rule) return a.rule < b.rule;
        if (a.claimA != b.claimA) return a.claimA < b.claimA;
        return a.claimB < b.claimB;
    });
    std::size_t counter = 0;
    for (Conflict& finding : findings) {
        finding.id = "X" + std::to_string(counter + 1);
        while (finding.id.size() < 4) finding.id.insert(1, "0");
        ++counter;
    }
    for (ConflictListener* listener : listeners_) {
        for (const Conflict& finding : findings) {
            listener->onFinding(finding);
        }
    }
    util::Logger::instance().info("ConflictDetector produced " + std::to_string(findings.size()) +
                            " finding(s) from " + std::to_string(rules_.size()) + " rule(s)");
    return findings;
}

DetectionContext ConflictDetector::buildContext(const model::ExtractionRecord& records,
                                                const ResolutionResult& resolution,
                                                const TimelineResult& timeline) {
    DetectionContext context;
    context.records = &records;
    context.resolution = &resolution;
    context.timeline = &timeline;

    // handwritten HashTable: (subject|predicate) -> claim ids (real engine path)
    ds::HashTable<std::string, std::vector<std::string>> index;
    ds::HashTable<std::string, const model::ClaimRecord*> byId;
    for (const auto& claim : records.claims) {
        const std::string key = claim.subject_id + "|" + claim.predicate;
        if (auto* existing = index.get(key)) {
            existing->push_back(claim.id);
        } else {
            index.insert(key, std::vector<std::string>{claim.id});
        }
        byId.insert(claim.id, &claim);
    }
    // transfer into the context's ordered map (deterministic iteration)
    for (const auto& entry : index) {
        const std::size_t sep = entry.first.find('|');
        context.claimIndex[{entry.first.substr(0, sep), entry.first.substr(sep + 1)}] =
            entry.second;
    }
    for (const auto& entry : byId) {
        context.claimById[entry.first] = entry.second;
    }
    return context;
}

std::unique_ptr<ConflictRule> ConflictDetector::RuleFactory::fromText(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) tokens.push_back(token);
    if (tokens.empty() || tokens[0].empty() || tokens[0][0] == '#') return nullptr;
    if (tokens.size() == 2 && tokens[0] == "temporal_cycle") {
        return std::make_unique<TemporalCycleRule>(tokens[1]);
    }
    if (tokens.size() == 3 && tokens[0] == "functional") {
        return std::make_unique<FunctionalPredicateRule>(tokens[1], tokens[2]);
    }
    if (tokens.size() == 3 && tokens[0] == "negation") {
        return std::make_unique<NegationRule>(tokens[1], tokens[2]);
    }
    if (tokens.size() == 3 && tokens[0] == "numeric") {
        return std::make_unique<NumericMismatchRule>(tokens[1], tokens[2]);
    }
    if (tokens.size() == 3 && tokens[0] == "location") {
        return std::make_unique<LocationTimeRule>(tokens[1], tokens[2]);
    }
    throw util::RuleError("rules.txt grammar violation: '" + line +
                          "' (expected: functional|negation|numeric|location <predicate|any> "
                          "overlap|same_unit, or temporal_cycle <predicate>)");
}

void ConflictCliPrinter::onFinding(const Conflict& finding) {
    std::ostringstream out;
    out << finding.id << " [" << finding.severity << "] " << finding.rule << ": "
        << finding.statement << "\n";
    out << "  subject " << finding.subjectId << " predicate " << finding.predicate << "\n";
    for (const Conflict::Excerpt& e : finding.excerpts) {
        out << "  " << e.documentId << " [" << e.start << "," << e.end << ") \"" << e.quote
            << "\"\n";
    }
    out << "  benign: " << finding.benignExplanation << "\n";
    std::cout << out.str();
}

void ConflictJsonCollector::onFinding(const Conflict& finding) {
    findings_.push_back(finding);
}

} // namespace verity::engine
