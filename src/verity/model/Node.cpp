#include "verity/model/Node.hpp"

#include <array>
#include <cctype>
#include <mutex>
#include <atomic>
#include <map>
#include <utility>

#include "verity/util/VerityError.hpp"

namespace verity::model {

namespace {

// Per-prefix counters. "E", "EV", "C", "D" are the frozen prefixes; the map
// also tolerates future ones without recompiling callers.
std::mutex& counterMutex() noexcept {
    static std::mutex m;
    return m;
}

std::map<std::string, long long>& counters() noexcept {
    static std::map<std::string, long long> c;
    return c;
}

std::atomic<int>& liveCount() noexcept {
    static std::atomic<int> n{0};
    return n;
}

} // namespace

Node::Node(NodeId id, std::string label)
    : id_(std::move(id)), label_(std::move(label)) {
    if (label_.empty()) {
        throw util::VerityError("Node label must be non-empty");
    }
    ++liveCount();
}

// Hand-written copy ctor (NOT defaulted): a defaulted one would skip the
// live-object increment and every clone would drift the counter negative.
Node::Node(const Node& other) : id_(other.id_), label_(other.label_) {
    ++liveCount();
}

Node::~Node() noexcept { --liveCount(); }

void Node::setLabel(std::string label) {
    if (label.empty()) {
        throw util::VerityError("Node label must be non-empty");
    }
    const auto non_space = label.find_first_not_of(" \t\r\n");
    if (non_space == std::string::npos) {
        throw util::VerityError("Node label must not be whitespace-only");
    }
    label_ = std::move(label);
}

std::string Node::nextId(const std::string& prefix) {
    if (prefix != "E" && prefix != "EV" && prefix != "C" && prefix != "D") {
        throw util::ParseError("nextId prefix must be E, EV, C or D: '" + prefix + "'");
    }
    std::lock_guard<std::mutex> lock(counterMutex());
    const long long n = ++counters()[prefix];
    std::string digits = std::to_string(n);
    while (digits.size() < 3) digits.insert(digits.begin(), '0');
    return prefix + digits;
}

void Node::observeId(const std::string& id) {
    std::pair<std::string, long long> parsed{"", 0};
    if (id.size() >= 3 && id[0] == 'E' && id[1] == 'V') {
        parsed.first = "EV";
        parsed.second = std::strtoll(id.c_str() + 2, nullptr, 10);
    } else if (!id.empty() && (id[0] == 'E' || id[0] == 'C' || id[0] == 'D')) {
        parsed.first = std::string(1, id[0]);
        parsed.second = std::strtoll(id.c_str() + 1, nullptr, 10);
    }
    if (parsed.first.empty() || parsed.second <= 0) {
        throw util::ParseError("observeId: malformed id '" + id + "'");
    }
    std::lock_guard<std::mutex> lock(counterMutex());
    long long& slot = counters()[parsed.first];
    if (parsed.second > slot) slot = parsed.second;
}

int Node::liveObjects() noexcept { return liveCount().load(); }

std::ostream& operator<<(std::ostream& os, const Node& node) {
    os << node.describe();
    return os;
}

} // namespace verity::model
