#include "verity/engine/EntityResolver.hpp"

#include <algorithm>
#include <cctype>
#include <set>

#include "verity/ds/DisjointSet.hpp"
#include "verity/ds/Levenshtein.hpp"
#include "verity/ds/Trie.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::engine {

namespace {

std::string lower(const std::string& s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

/// Strips leading honorific titles ("mr.", "ms.", "mrs.", "dr.") after
/// lowercasing — "Mr. Smith" parses like a bare surname.
std::string stripTitles(const std::string& lowered) {
    static const char* titles[] = {"mr. ", "ms. ", "mrs. ", "dr. "};
    std::string out = lowered;
    bool changed = true;
    while (changed) {
        changed = false;
        for (const char* t : titles) {
            const std::string prefix(t);
            if (out.rfind(prefix, 0) == 0) {
                out = out.substr(prefix.size());
                changed = true;
            }
        }
    }
    return out;
}

/// Surname token: "Smith, John" -> "smith"; "John Smith" -> "smith";
/// "Mr. Smith" -> "smith". Titles are stripped first.
std::string surnameToken(const std::string& name) {
    const std::string l = stripTitles(lower(name));
    const std::size_t comma = l.find(',');
    if (comma != std::string::npos) {
        return l.substr(0, comma);
    }
    const std::size_t lastSpace = l.find_last_of(' ');
    if (lastSpace == std::string::npos) return l;
    return l.substr(lastSpace + 1);
}

/// First-name candidate (the part that is not the surname token), lowercased;
/// titles stripped.
std::string firstNameToken(const std::string& name) {
    const std::string l = stripTitles(lower(name));
    const std::size_t comma = l.find(',');
    if (comma != std::string::npos && comma + 2 <= l.size()) {
        return l.substr(comma + 2);   // after ", "
    }
    const std::size_t lastSpace = l.find_last_of(' ');
    if (lastSpace == std::string::npos) return "";
    return l.substr(0, lastSpace);
}

/// "J." / "J" counts as an initial.
[[nodiscard]] bool isInitial(const std::string& first) {
    return first.size() <= 2 && (first.size() == 1 ||
        (first.size() == 2 && first[1] == '.'));
}

/// True when two first-name tokens are compatible: identical, or one is an
/// initial matching the other's first character. An EMPTY first name (a bare
/// honorific like "Mr. Smith") is NOT compatible — it would merge every
/// same-surname person; title-form entities merge through the alias table
/// instead (gold records the contextual alias).
[[nodiscard]] bool firstNamesCompatible(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return false;
    if (a == b) return true;
    if (isInitial(a)) return a[0] == b[0];
    if (isInitial(b)) return b[0] == a[0];
    // distinct known names: also allow tiny typos (Levenshtein <= 1)
    return ds::levenshtein(a, b) <= 1;
}

struct EntityInfo {
    std::string id;
    std::string name;
    std::string type;
    std::vector<std::string> tokens;   // all names+aliases (lowercased)
};

EntityInfo infoOf(const model::EntityRecord& e) {
    EntityInfo info;
    info.id = e.id;
    info.name = e.name;
    info.type = e.type;
    info.tokens.push_back(lower(e.name));
    for (const std::string& alias : e.aliases) info.tokens.push_back(lower(alias));
    return info;
}

/// Do two entities describe the same person? (identity policy above)
bool refersToSamePerson(const EntityInfo& a, const EntityInfo& b) {
    // (a) exact token overlap — any name/alias of one equals any of the other
    for (const std::string& ta : a.tokens) {
        for (const std::string& tb : b.tokens) {
            if (ta == tb) return true;
        }
    }
    // (b) same surname + compatible first names
    if (a.type != "Person" || b.type != "Person") return false;
    const std::string sa = surnameToken(a.name);
    const std::string sb = surnameToken(b.name);
    if (sa != sb) return false;
    return firstNamesCompatible(firstNameToken(a.name), firstNameToken(b.name));
}

} // namespace

ResolutionResult EntityResolver::resolve(const model::ExtractionRecord& records) {
    std::vector<EntityInfo> infos;
    infos.reserve(records.entities.size());
    for (const auto& e : records.entities) infos.push_back(infoOf(e));

    // 1. Blocking: Trie over surname tokens; candidates share a surname.
    ds::Trie surnameTrie;
    for (const EntityInfo& info : infos) surnameTrie.insert(surnameToken(info.name));

    // 2. Pair scan. Blocking gate: exact token overlap OR shared surname —
    // refersToSamePerson short-circuits on the cheap surname comparison first,
    // so unrelated entities never reach the expensive checks.
    const std::size_t n = infos.size();
    ds::DisjointSet dsu(n);
    std::vector<std::pair<std::size_t, std::size_t>> matchedPairs;
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            if (refersToSamePerson(infos[i], infos[j])) {
                matchedPairs.emplace_back(i, j);
                dsu.unionSets(i, j);
            }
        }
    }

    // 3. dm::Relation + equivalence closure.
    std::vector<std::string> universe;
    universe.reserve(n);
    for (const EntityInfo& info : infos) universe.push_back(info.id);
    dm::Relation<std::string> relation(universe);
    for (const auto& [i, j] : matchedPairs) {
        relation.add(infos[i].id, infos[j].id);
        relation.add(infos[j].id, infos[i].id);
    }
    const dm::Relation<std::string> closed = relation.symmetricClosure().reflexiveClosure().transitiveClosure();

    // 4. DSU cross-check happens in the test; here we build classes from DSU.
    std::vector<std::vector<std::string>> dsuClasses;
    {
        std::vector<std::vector<std::string>> buckets(n);
        for (std::size_t i = 0; i < n; ++i) buckets[dsu.find(i)].push_back(infos[i].id);
        for (auto& bucket : buckets) {
            if (!bucket.empty()) {
                std::sort(bucket.begin(), bucket.end());
                dsuClasses.push_back(std::move(bucket));
            }
        }
        std::sort(dsuClasses.begin(), dsuClasses.end());
    }
    const auto relationClasses = closed.isEquivalence() ? closed.equivalenceClasses()
                                                        : std::vector<std::vector<std::string>>{};

    ResolutionResult result;
    result.mergedPairs = matchedPairs.size();
    // canonical classes come from the DSU (same partition); keep sorted order
    result.classes = dsuClasses;

    // 5. Canonical representative: prefer the name that is neither an
    // abbreviation (contains an initial like "j.") nor a rearranged form
    // (contains a comma like "Smith, John") nor a bare title ("Mr. Smith");
    // then prefer the LONGER name; ties broken by smallest id. So
    // "John Smith" beats "J. Smith" / "Smith, John" / "Mr. Smith".
    auto nameQuality = [](const std::string& name) {
        // 0 = worst (rearranged "Smith, John"), 3 = best (plain full name).
        std::string l = lower(name);
        if (l.find(',') != std::string::npos) return 0;
        if (l.rfind("mr. ", 0) == 0 || l.rfind("ms. ", 0) == 0 ||
            l.rfind("mrs. ", 0) == 0 || l.rfind("dr. ", 0) == 0) {
            return 1;
        }
        if (l.find(". ") != std::string::npos) return 2;   // "j. smith" initials
        return 3;
    };
    for (const auto& cls : result.classes) {
        std::string canonical = cls.front();
        for (const std::string& id : cls) {
            const EntityInfo* a = nullptr;
            const EntityInfo* b = nullptr;
            for (const EntityInfo& info : infos) {
                if (info.id == id) a = &info;
                if (info.id == canonical) b = &info;
            }
            if (a != nullptr && b != nullptr) {
                const int qa = nameQuality(a->name);
                const int qb = nameQuality(b->name);
                const bool aBetter = qa != qb ? qa > qb            // higher quality value = worse
                                              : (a->name.size() != b->name.size()
                                                     ? a->name.size() > b->name.size()
                                                     : a->id < b->id);
                if (aBetter) canonical = id;
            }
        }
        std::vector<std::string> aliases;
        for (const std::string& id : cls) {
            for (const EntityInfo& info : infos) {
                if (info.id == id) {
                    aliases.push_back(info.name);
                }
            }
        }
        std::sort(aliases.begin(), aliases.end());
        aliases.erase(std::unique(aliases.begin(), aliases.end()), aliases.end());
        result.aliasesByCanonical.emplace_back(canonical, std::move(aliases));
        for (const std::string& id : cls) {
            result.mapping.emplace_back(id, canonical);
        }
    }
    std::sort(result.mapping.begin(), result.mapping.end());

    // invariant: relation closure classes == DSU classes (verified by callers;
    // loud error if the two independent computations ever disagree)
    if (relationClasses.size() != dsuClasses.size()) {
        throw util::VerityError("EntityResolver internal error: relation closure and "
                                "DisjointSet disagree on class count");
    }
    return result;
}

std::string EntityResolver::canonicalOf(const ResolutionResult& result,
                                        const std::string& entityId) {
    for (const auto& [from, to] : result.mapping) {
        if (from == entityId) return to;
    }
    return entityId;
}

} // namespace verity::engine
