#pragma once
// VERITY GRAPH — EventNode: a dated occurrence attached to an entity.
// Phase-2 TimelineBuilder turns these into a partial order with Hasse output.
//
// Complexity: all methods O(1).

#include <optional>
#include <string>

#include "verity/model/Extraction.hpp"   // DateInterval, Span
#include "verity/model/Node.hpp"

namespace verity::model {

class EventNode : public Node {
public:
    EventNode(std::string label, NodeId entityId, DateInterval date,
              NodeId documentId, Span span,
              const std::optional<std::string>& id = std::nullopt)
        : Node(NodeId(id.has_value() ? *id : nextId("EV")), std::move(label)),
          entityId_(std::move(entityId)),
          date_(std::move(date)),
          documentId_(std::move(documentId)),
          span_(std::move(span)) {}

    [[nodiscard]] const NodeId& entityId() const noexcept { return entityId_; }
    [[nodiscard]] const DateInterval& date() const noexcept { return date_; }
    [[nodiscard]] const NodeId& documentId() const noexcept { return documentId_; }
    [[nodiscard]] const Span& span() const noexcept { return span_; }
    void setDate(DateInterval d) { date_ = std::move(d); }

    EventNode& onDate(DateInterval d) {   // chaining
        date_ = std::move(d);
        return *this;
    }

    [[nodiscard]] std::string type() const override { return "Event"; }

    [[nodiscard]] std::string describe() const override {
        return id().str() + " Event '" + label() + "' on " + date_.toString() +
               " (" + entityId_.str() + " @" + documentId_.str() + ")";
    }

    [[nodiscard]] nlohmann::json toJson() const override {
        nlohmann::json j;
        j["id"] = id().str();
        j["label"] = label();
        j["type"] = "Event";
        j["entity_id"] = entityId_.str();
        j["date"] = date_.toString();
        j["date_precision"] = date_.precision == DatePrecision::Year  ? "Year"
                              : date_.precision == DatePrecision::Month ? "Month"
                                                                        : "Day";
        j["document_id"] = documentId_.str();
        j["span"] = {{"start", span_.start}, {"end", span_.end}, {"quote", span_.quote}};
        return j;
    }

    [[nodiscard]] std::unique_ptr<Node> clone() const override {
        return std::make_unique<EventNode>(*this);
    }

private:
    NodeId entityId_;
    DateInterval date_;
    NodeId documentId_;
    Span span_;
};

} // namespace verity::model
