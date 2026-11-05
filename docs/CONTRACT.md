# VERITY GRAPH — Day-1 Frozen Contract

Status: **FROZEN** (v1.0). Changing anything in this file requires an explicit
decision recorded in `docs/DECISIONS.md`.

This contract freezes the stable IDs, enums, graph API, schemas, predicates,
date-interval semantics and error conventions **before** implementation, as
required by the build brief. SPEC.md was unavailable; the defaults below come
from the build brief and PHASES.md (see `docs/ASSUMPTIONS.md`).

---

## 1. Namespaces and targets

| Namespace | Contains | CMake target |
|---|---|---|
| `verity::util` | errors, Logger, string/format helpers | `verity_core` (static) |
| `verity::model` | NodeId, enums, Node family, NodeStore, Extraction structs | `verity_core` |
| `verity::ds` | LinkedList, Stack, Queue, HashTable, Graph, graph algorithms | `verity_ds` (interface, header-only) |
| `verity::dm` | numtheory, sets, counting, relations, logic, algebra, graph analysis | `verity_dm` (interface, header-only) |
| `verity::io` | JsonLoader, GraphSerializer, exporters | `verity_core` |
| `verity::engine` | VerityEngine, GraphBuilder, resolvers, detectors, tracers | `verity_core` |
| `verity` (CLI) | `app/main.cpp` → executable **`verity`** | `verity_cli` |
| tests | doctest suite → executable **`verity_tests`** | `verity_tests` |

Header convention: `include/verity/<area>/<Thing>.hpp`; templates and small
inline code are header-only; non-template logic gets a `.cpp` under
`src/verity/<area>/`.

## 2. NodeId

```cpp
namespace verity::model {
class NodeId {                 // strong wrapper around a stable string ID
public:
    explicit NodeId(std::string raw);   // validates prefix + format
    const std::string& str() const noexcept;
    bool operator==(const NodeId&) const;  // <=>, <, != also defined (lexicographic)
    char prefix() const noexcept;          // 'E','C','EV','D'
};
}
```

- Prefixes and counters: **`E`** entities (`E001`…), **`C`** claims (`C001`…),
  **`EV`** events (`EV001`…), **`D`** documents (`D001`…).
- Format: `<PREFIX><3+ zero-padded digits>`; display normalization strips
  leading zeros of the numeric part **only in UI rendering** ("D006" ↔ "D6");
  storage, CLI output, fixtures and frontend links always use the padded form.
- ID generation: `Node::nextId(prefix)` uses per-prefix static counters plus a
  live-object counter; counters resume from `max(existing)+1` after load so
  saved/loaded graphs never collide.
- JSON serialization: plain string, exactly the padded form.

## 3. Enums (fixed names; serialized as these exact strings)

```cpp
enum class EntityType { Person, Organization, Product, Location };
enum class RelationType {
    SubjectOf,        // claim -> subject entity
    ObjectOf,         // claim -> object entity (when object is an entity ref)
    MentionedIn,      // entity  -> document (mention evidence)
    DocumentEvidence, // evidence -> document (claim <-> doc support links)
    Cites,            // document -> document citation
    AliasOf,          // entity mention -> canonical entity
    HappenedBefore,   // event  -> event (strict order claim)
    DerivedFrom       // finding/claim derivation provenance link
};
enum class Polarity { Positive, Negative, Unknown };
enum class EvidenceKind { Supporting, Contradicting, Neutral };
```

Serialization uses `to_string(RelationType)` → `"SubjectOf"`, etc.
(defined in `include/verity/model/Enums.hpp` with `from_string` that throws
`ParseError` on unknown values).

## 4. Node family (model)

```cpp
class Node {                       // abstract, virtual base
public:
    NodeId id() const; const std::string& label() const;
    virtual std::string type() const = 0;      // dynamic binding
    virtual std::string describe() const = 0;  // Phase 2 pure virtual (already declared)
    virtual nlohmann::json toJson() const = 0;
    virtual std::unique_ptr<Node> clone() const = 0;   // polymorphic copying
    virtual ~Node() noexcept;                  // virtual destructor everywhere
    static std::string nextId(char prefix);    // C001-style counters
    static int liveObjects() noexcept;         // live-object counter
};
class Entity : public Node { EntityType entityType; std::vector<std::string> aliases; };
class ClaimNode : public Node { /* see §6 */ };
class EventNode : public Node { /* date interval + event_type */ };
class DocumentNode : public Node { /* format, normalized text length, citations */ };
```

- Encapsulation: validated setters — `ClaimNode::setConfidence(double)` throws
  `VerityError` outside `[0,1]`; `Polarity`/dates validated at construction.
- Constructors: overloading + default arguments demonstrated; initializer
  lists used; `this`-pointer chaining (`ClaimNode& withConfidence(double)` …).
- `friend std::ostream& operator<<(std::ostream&, const Node&)` dispatches
  virtually via `describe()`; `GraphInspector` is a friend class of Node
  family exposing internals for tests/debug.
- Const correctness: all accessors const; graph traversal APIs take const refs.

### NodeStore (Rule of Five, raw ownership)

```cpp
class NodeStore {   // owns Node* via raw new/delete (teaching requirement)
public:
    NodeStore(); explicit NodeStore(const std::string& name);
    NodeStore(const NodeStore&);              // deep copy via Node::clone()
    NodeStore& operator=(const NodeStore&);   // copy-and-protect against self-assignment
    NodeStore(NodeStore&&) noexcept;          // move leaves other empty-but-valid
    NodeStore& operator=(NodeStore&&) noexcept;
    ~NodeStore() noexcept;                    // deletes all owned nodes
    Node* add(std::unique_ptr<Node> n);       // takes ownership; throws DuplicateNodeError
    Node* find(const NodeId& id);  const Node* find(const NodeId&) const;
    std::size_t size() const;                 // copy/move/destruct must balance liveObjects()
};
```

- The raw-ownership exercise is confined to NodeStore + its tests; the rest of
  the application passes `std::unique_ptr<Node>` / uses NodeStore as owner.
- Clone-based polymorphic copying (`Node::clone`), tested for self-assignment,
  move-from state, exception during construction, and leak-freedom.

## 5. ds::Graph (frozen interface)

```cpp
namespace verity::ds {
template <class N, class E>
class Graph {                     // adjacency list; supports directed+undirected
public:
    void addNode(const N& id);                 // throws DuplicateNodeError
    void addEdge(const N& a, const N& b, const E& w);        // directed
    void addEdgeUndirected(const N& a, const N& b, const E& w);
    // overloads (Phase 2): addEdge(const Edge<N,E>&)
    bool hasNode(const N&) const;
    bool hasEdge(const N& a, const N& b) const;   // any parallel edge counts
    const std::vector<N>& nodes() const;
    const std::vector<std::pair<N,E>>& neighbors(const N&) const;     // out-edges
    const std::vector<std::pair<N,E>>& inNeighbors(const N&) const;   // in-edges (directed)
    std::size_t outDegree(const N&) const; std::size_t inDegree(const N&) const;
    std::size_t degree(const N&) const;   // undirected: loop counts 2 (graph-theory convention)
    bool removeEdge(const N& a, const N& b, const E* w = nullptr); // removes one/first-matching
    bool removeNode(const N&);
    Graph<N,E> transpose() const;                // directed reverse
    E& operator[](const std::pair<N,N>& endpoints);  // edge weight access
    std::size_t nodeCount() const; std::size_t edgeCount() const; // directed: |A|; undirected: each pair once
    static constexpr bool kSelfLoopsAllowed = true;
    static constexpr bool kParallelEdgesAllowed = true;
};
}
```

**Policies (frozen):** self-loops allowed (counted as 2 in undirected degree);
parallel edges allowed (first match removed by `removeEdge` unless weight
given). `happened_before` data is stored in a directed `Graph<NodeId,EdgeAttr>`;
strict-partial-order checks reject cycles (see §9).

## 6. Claim, object model, spans, dates

```cpp
struct Span {                 // half-open [start, end) in UNICODE CODE POINTS
    std::size_t start;        // of the *normalized extracted text*
    std::size_t end;
    std::string quote;        // exact substring text[start,end) — validated on ingest
};
struct DatePoint { int year; int month; int day; };   // month/day = 0 when unspecified
enum class DatePrecision { Year, Month, Day };
struct DateInterval {         // preserved original precision
    DatePrecision precision;
    DatePoint start;          // inclusive
    DatePoint end;            // inclusive
    static DateInterval fromPoint(const DatePoint& p);   // expands Month/Year to range
    bool overlaps(const DateInterval&) const noexcept;
    bool strictlyBefore(const DateInterval&) const noexcept;
    std::string toJsonString() const;   // "2024" | "2024-03" | "2024-03-15" or {"from":..,"to":..}
};
struct ClaimObject {          // typed object: one of
    enum class Kind { EntityRef, StringValue, DateValue, QuantityValue } kind;
    std::string entity_id;    // Kind==EntityRef
    std::string text;         // StringValue
    DateInterval date;        // DateValue
    double quantity; std::string unit;   // QuantityValue (unit required; compared only if same unit)
};
class ClaimNode : public Node {
    NodeId subject_id;  std::string predicate;  ClaimObject object;
    Polarity polarity;  double confidence;      // validated [0,1]
    NodeId document_id; Span span;
    std::optional<DateInterval> valid_time;     // when the claim holds
};
```

**Span policy:** spans index *code points* of the normalized extracted text
(normalized = `\r\n|\r → \n`, NFC, single trailing newline). Python extraction
counts code points; C++ validates by decoding UTF-8 to code points; the
browser converts UTF-16 index ↔ code point explicitly. Byte offsets are never
mixed silently. `quote` must equal the exact slice or ingest fails.

## 7. Extraction JSON schema (ai/schema/extraction.schema.json)

```jsonc
{
  "schema_version": "1.0.0",
  "documents": [ { "id": "D001", "filename": "d001.txt", "format": "txt|pdf|docx",
                   "normalized_text": "...", "metadata": {}, "citations": ["D002"] } ],
  "entities":  [ { "id": "E001", "name": "John Smith", "type": "Person",
                   "aliases": ["J. Smith"], "mentions": [ {"document_id":"D001","span":{"start":0,"end":10,"quote":"John Smith"}} ] } ],
  "claims":    [ { "id": "C001", "subject_id": "E001", "predicate": "has_ceo",
                   "object": {"kind":"EntityRef","entity_id":"E002"},
                   "polarity": "Positive", "confidence": 0.9,
                   "document_id": "D001", "span": {"start":0,"end":10,"quote":"..."},
                   "valid_from": "2023-01", "valid_to": null } ],
  "events":    [ { "id": "EV001", "label": "Product X launch", "entity_id": "E003",
                   "date": "2024-03", "document_id": "D004", "span": {...} } ],
  "citations": [ { "citing": "D006", "cited": "D007", "span": {...} } ]
}
```

- Validation is real: `jsonschema` (Python) at extraction time **and** a
  structural+semantic validator in C++ `io::JsonLoader` (IDs, references,
  types, confidence bounds, date sanity, span exactness). JSON parsing alone
  is never called validation.
- Gold annotations live in `data/gold/*.gold.json` using the same claim shape;
  they are **never** read by rule-based extraction (mock extractor loads them
  by design).

## 8. Report JSON schema (engine export → out/graph.json)

Root keys (fixed): `schema_version`, `metadata`, `nodes`, `edges`,
`documents`, `claims`, `conflicts`, `timeline`, `provenance`, `rankings`,
`ledger`, `dm`. Every UI count/visualization derives from this payload.
Node shape: `{"id","label","type","kind","meta"}`; edge shape:
`{"from","to","relation","weight","label"}`. Conflicts carry: `id`, `rule`,
`statement` (**always containing the exact phrase "Potential contradiction"**,
or "Potential temporal contradiction" for temporal findings), `claim_a`,
`claim_b`, `documents`, `excerpts` (exact quotes + spans), `derivation`
(ordered steps), `severity` (`low|medium|high`), `confidence`, and
`benign_explanation`.

## 9. Predicates (data/rules/predicates.json — frozen)

| Predicate | Domain → Range | Temporal meaning | Functional |
|---|---|---|---|
| `announced` | Organization/Product → Product/Event | statement date; NOT the availability date | no |
| `launched` | Organization/Product → Product/Event | availability date; distinct from announced | no |
| `has_ceo` | Organization → Person | holds during `valid_time` | yes |
| `located_at` | Organization/Person → Location | holds during `valid_time` | yes |
| `present_at` | Person → Location/Event | requires justified temporal overlap for conflict | no |
| `happened_before` | Event → Event | strict order; valid SPO only if acyclic | no |
| `cites` | Document → Document | citation edge | no |

## 10. Conflict semantics (frozen rules)

1. Compare **compatible predicates only**, canonical subjects, identical units,
   and overlapping validity intervals.
2. Month-vs-day overlap is **not** a contradiction (interval overlap logic).
3. Different `has_ceo` values with disjoint validity intervals are consistent.
4. Unknown polarity is preserved; unknown ≠ negation.
5. Severity is ranked separately from extraction confidence; every finding
   carries a possible benign explanation.
6. Temporal cycle: report "Potential temporal contradiction" + witness path;
   withhold Hasse/topological output for cyclic data.
7. Wording: findings always use "Potential contradiction" / "Potential temporal
   contradiction". The system never states truth verdicts.

## 11. Error hierarchy & CLI conventions

```cpp
class VerityError : public std::runtime_error;           // base
class ParseError : public VerityError;                   // schema/JSON/DSL
class GraphError : public VerityError;                   // graph domain
class DuplicateNodeError : public GraphError;
class MissingNodeError : public GraphError;
class ExtractionError : public VerityError;              // Python boundary (Phase 3)
class StorageError : public VerityError;                 // binary I/O (Phase 4)
class RuleError : public VerityError;                    // rule parsing (Phase 3)
```

CLI: exit 0 = success; exit 1 = user/validation error; exit 2 = internal
error. Diagnostics on **stderr**; `--json` switches machine-readable output.
Default graph path: `out/graph.vg` (Phase 4+); commands accept an explicit
graph path argument. `Logger` is the project's single Singleton (thread-safe
Meyers singleton, levels error/warn/info/debug, file+stderr sinks).

## 12. Handwritten-DS policy

Required structures are implemented by hand (`verity::ds`, `verity::dm`) and
used in real engine paths. The standard library appears only as test oracles
and ordinary utilities (e.g. `std::sort` inside an oracle test), never as a
replacement for a required implementation.

## 13. Date/wording conventions

- Fictional dataset companies: **Aster Vale Systems**, **Northstar Meridian
  Labs**; product: **Product X** (alias "Vale X" in one document); persons:
  John Smith (+ 3 alias variants), Maria Chen (CEO claims), David Okoro,
  Priya Raman; intermediary entity: **Elena Vasquez** (consultant).
- Dates ISO `YYYY | YYYY-MM | YYYY-MM-DD`; intervals inclusive; half-open
  spans; all docs padded IDs D001–D010.
