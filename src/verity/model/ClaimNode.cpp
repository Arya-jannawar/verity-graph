#include "verity/model/ClaimNode.hpp"

#include <cmath>

#include "verity/util/VerityError.hpp"

namespace verity::model {

namespace {
void validateConfidence(double c) {
    if (!(c >= 0.0 && c <= 1.0) || std::isnan(c)) {
        throw util::VerityError("ClaimNode confidence must be within [0,1], got " +
                                std::to_string(c));
    }
}
} // namespace

ClaimNode::ClaimNode(std::string label, NodeId subjectId, std::string predicate,
                     ClaimObject object, NodeId documentId, Span span,
                     Polarity polarity, double confidence,
                     const std::optional<std::string>& id,
                     std::optional<DateInterval> validTime)
    : Node(NodeId(id.has_value() ? *id : nextId("C")), std::move(label)),
      subjectId_(std::move(subjectId)),
      predicate_(std::move(predicate)),
      object_(std::move(object)),
      polarity_(polarity),
      confidence_(confidence),
      documentId_(std::move(documentId)),
      span_(std::move(span)),
      validTime_(std::move(validTime)) {
    validateConfidence(confidence_);
    if (predicate_.empty()) {
        throw util::VerityError("ClaimNode predicate must be non-empty");
    }
}

ClaimNode::ClaimNode(std::string label, NodeId subjectId, std::string predicate,
                     std::string objectText, NodeId documentId, Span span,
                     Polarity polarity, double confidence,
                     const std::optional<std::string>& id)
    : ClaimNode(std::move(label), std::move(subjectId), std::move(predicate),
                ClaimObject{ClaimObject::Kind::StringValue, "", std::move(objectText),
                            DateInterval{}, 0.0, ""},
                std::move(documentId), std::move(span), polarity, confidence,
                id, std::nullopt) {}

void ClaimNode::setConfidence(double confidence) {
    validateConfidence(confidence);
    confidence_ = confidence;
}

ClaimNode& ClaimNode::withConfidence(double c) {
    setConfidence(c);
    return *this;
}

ClaimNode& ClaimNode::withPolarity(Polarity p) noexcept {
    polarity_ = p;
    return *this;
}

ClaimNode& ClaimNode::withValidTime(DateInterval iv) {
    validTime_ = std::move(iv);
    return *this;
}

std::string ClaimNode::objectToString(const ClaimObject& o) {
    switch (o.kind) {
        case ClaimObject::Kind::EntityRef:    return o.entity_id;
        case ClaimObject::Kind::StringValue:  return "\"" + o.text + "\"";
        case ClaimObject::Kind::DateValue:    return o.date.toString();
        case ClaimObject::Kind::QuantityValue:
            return std::to_string(o.quantity) + " " + o.unit;
    }
    return "";
}

std::string ClaimNode::describe() const {
    return id().str() + " Claim: " + subjectId_.str() + " " + predicate_ + " " +
           objectToString(object_) + " [" + to_string(polarity_) + ", " +
           std::to_string(confidence_) + "] @" + documentId_.str();
}

nlohmann::json ClaimNode::toJson() const {
    nlohmann::json j;
    j["id"] = id().str();
    j["label"] = label();
    j["type"] = "Claim";
    j["subject_id"] = subjectId_.str();
    j["predicate"] = predicate_;
    j["object"] = objectToString(object_);
    j["polarity"] = to_string(polarity_);
    j["confidence"] = confidence_;
    j["document_id"] = documentId_.str();
    j["span"] = {{"start", span_.start}, {"end", span_.end}, {"quote", span_.quote}};
    if (validTime_) j["valid_time"] = validTime_->toString();
    return j;
}

} // namespace verity::model
