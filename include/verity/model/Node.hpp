#pragma once
// VERITY GRAPH — Node: abstract polymorphic base of the evidence-graph model.
//
// OOP coverage (Phase 1):
//   * abstract virtual base (type/describe/toJson/clone are pure virtual)
//   * encapsulation (private members, validated mutation via subclasses)
//   * static members: nextId() counters, observeId() (resume after load),
//     liveObjects() live-instance counter
//   * friend operator<< and friend GraphInspector
//   * const correctness everywhere; copy is PUBLICLY disabled and PROTECTED
//     so derived classes can implement clone()-based polymorphic copying
//     without slicing (copy through Node* is impossible; through Derived& OK)
//
// Complexity: all accessors O(1); nextId/observeId O(1) amortized.

#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include <nlohmann/json.hpp>

#include "verity/model/NodeId.hpp"

namespace verity::model {

class GraphInspector;   // friend (defined in GraphInspector.hpp)

class Node {
public:
    virtual ~Node() noexcept;

    [[nodiscard]] const NodeId& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    void setLabel(std::string label);   // validated: rejects empty/whitespace-only

    [[nodiscard]] virtual std::string type() const = 0;      // "Entity" | "Claim" | ...
    [[nodiscard]] virtual std::string describe() const = 0;  // human-readable one-liner
    [[nodiscard]] virtual nlohmann::json toJson() const = 0;
    [[nodiscard]] virtual std::unique_ptr<Node> clone() const = 0;  // polymorphic copy

    // --- static ID generation & live-object tracking -----------------------
    /// Next id for prefix "E" | "EV" | "C" | "D": zero-padded (E001, EV001...).
    /// Counters resume from the highest observed id (see observeId) so saved
    /// and re-loaded graphs never collide.
    [[nodiscard]] static std::string nextId(const std::string& prefix);

    /// Feed an existing id (e.g. from a loaded document) into the counters.
    static void observeId(const std::string& id);

    /// Live Node instances across the process (ctor/dtor balanced).
    [[nodiscard]] static int liveObjects() noexcept;

    friend std::ostream& operator<<(std::ostream& os, const Node& node);
    friend class GraphInspector;

protected:
    /// Subclasses generate or receive the id; Node itself never invents one.
    Node(NodeId id, std::string label);

    // Copy is PROTECTED (usable only by derived classes' clone()/implicit
    // copies): external code cannot slice a Node because the copy ctor is
    // inaccessible outside derived-class copy construction. The copy ctor is
    // DEFINED (not defaulted) in Node.cpp so the live-object counter stays
    // balanced for clones. Public move is likewise absent.
    Node(const Node& other);
    Node& operator=(const Node& other) = default;

private:
    NodeId id_;
    std::string label_;
};

/// Friend operator<<: dispatches through the virtual describe().
std::ostream& operator<<(std::ostream& os, const Node& node);

} // namespace verity::model
