#pragma once
// VERITY GRAPH — DocumentNode: a source document in the graph.
//
// Stores the normalized extracted text so spans can be verified and excerpts
// rendered. Complexity: describe/toJson O(citations); text access O(1).

#include <optional>
#include <string>
#include <vector>

#include "verity/model/Enums.hpp"
#include "verity/model/Node.hpp"

namespace verity::model {

class DocumentNode : public Node {
public:
    DocumentNode(std::string filename, DocumentFormat format,
                 std::string normalizedText, std::vector<std::string> citations = {},
                 const std::optional<std::string>& id = std::nullopt)
        : Node(NodeId(id.has_value() ? *id : nextId("D")), std::move(filename)),
          format_(format),
          normalizedText_(std::move(normalizedText)),
          citations_(std::move(citations)) {}

    [[nodiscard]] const std::string& filename() const noexcept { return label(); }
    [[nodiscard]] DocumentFormat format() const noexcept { return format_; }
    [[nodiscard]] const std::string& normalizedText() const noexcept { return normalizedText_; }
    [[nodiscard]] const std::vector<std::string>& citations() const noexcept { return citations_; }
    void addCitation(std::string documentId) {
        if (!documentId.empty()) citations_.push_back(std::move(documentId));
    }

    [[nodiscard]] std::string type() const override { return "Document"; }

    [[nodiscard]] std::string describe() const override {
        return id().str() + " Document '" + label() + "' [" + to_string(format_) +
            "] " + std::to_string(normalizedText_.size()) + " bytes, " +
            std::to_string(citations_.size()) + " citations";
    }

    [[nodiscard]] nlohmann::json toJson() const override {
        nlohmann::json j;
        j["id"] = id().str();
        j["label"] = label();
        j["type"] = "Document";
        j["format"] = to_string(format_);
        j["text_length"] = normalizedText_.size();
        j["citations"] = citations_;
        return j;
    }

    [[nodiscard]] std::unique_ptr<Node> clone() const override {
        return std::make_unique<DocumentNode>(*this);
    }

private:
    DocumentFormat format_;
    std::string normalizedText_;
    std::vector<std::string> citations_;
};

} // namespace verity::model
