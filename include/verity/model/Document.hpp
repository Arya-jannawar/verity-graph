#pragma once
// VERITY GRAPH — model/Document: abstract Document family + Composite text
// tree (OOP Unit 3, Phase 2).
//
// Family: Document (abstract) ← TextDocument / PdfDocument / DocxDocument,
// created through DocumentFactory::create(format, filename, text).
// Composite: Document is the root of a TextUnit tree
//   Document → Section → Paragraph → Sentence (leaf).
// Pure virtuals: Document::sections, TextUnit::wordCount; virtual destructors
// everywhere; override used throughout. PDF/DOCX text is produced by the
// Python readers; the C++ family models the STRUCTURE of extracted text
// (sections/paragraphs with code-point offsets), not binary parsing.
//
// Complexity: building the tree O(text); traversals O(nodes).

#include <memory>
#include <string>
#include <vector>

#include "verity/model/Enums.hpp"
#include "verity/model/Node.hpp"

namespace verity::model {

// --- Composite: TextUnit tree ----------------------------------------------

/// Abstract component of the text tree.
class TextUnit {
public:
    virtual ~TextUnit() noexcept = default;
    /// Number of whitespace-separated words in this unit.
    [[nodiscard]] virtual int wordCount() const = 0;
    /// Full plain text of this unit (with newlines between children).
    [[nodiscard]] virtual std::string text() const = 0;
    /// Code-point offset range [start,end) in the DOCUMENT's normalized text.
    [[nodiscard]] std::pair<std::size_t, std::size_t> offsetRange() const noexcept {
        return {offsetStart_, offsetEnd_};
    }
    void setOffsets(std::size_t start, std::size_t end) noexcept {
        offsetStart_ = start;
        offsetEnd_ = end;
    }

protected:
    std::size_t offsetStart_ = 0;
    std::size_t offsetEnd_ = 0;
};

/// Leaf: a single sentence.
class Sentence final : public TextUnit {
public:
    explicit Sentence(std::string text) : text_(std::move(text)) {}
    [[nodiscard]] int wordCount() const override {
        int words = 0;
        bool inWord = false;
        for (const char c : text_) {
            if (c == ' ' || c == '\n' || c == '\t') inWord = false;
            else if (!inWord) { inWord = true; ++words; }
        }
        return words;
    }
    [[nodiscard]] std::string text() const override { return text_; }

private:
    std::string text_;
};

/// Composite: a paragraph holding sentences.
class Paragraph final : public TextUnit {
public:
    Sentence& addSentence(std::string text) {
        auto sentence = std::make_unique<Sentence>(std::move(text));
        Sentence& ref = *sentence;
        children_.push_back(std::move(sentence));
        return ref;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<Sentence>>& sentences() const noexcept {
        return children_;
    }
    [[nodiscard]] int wordCount() const override {
        int total = 0;
        for (const auto& s : children_) total += s->wordCount();
        return total;
    }
    [[nodiscard]] std::string text() const override {
        std::string out;
        for (std::size_t i = 0; i < children_.size(); ++i) {
            if (i > 0) out.push_back(' ');
            out += children_[i]->text();
        }
        return out;
    }

private:
    std::vector<std::unique_ptr<Sentence>> children_;
};

/// Composite: a section holding paragraphs.
class Section final : public TextUnit {
public:
    explicit Section(std::string heading) : heading_(std::move(heading)) {}
    [[nodiscard]] const std::string& heading() const noexcept { return heading_; }
    Paragraph& addParagraph() {
        children_.push_back(std::make_unique<Paragraph>());
        return *children_.back();
    }
    [[nodiscard]] const std::vector<std::unique_ptr<Paragraph>>& paragraphs() const noexcept {
        return children_;
    }
    [[nodiscard]] int wordCount() const override {
        int total = 0;
        for (const auto& p : children_) total += p->wordCount();
        return total;
    }
    [[nodiscard]] std::string text() const override {
        std::string out;
        for (const auto& p : children_) {
            out += p->text();
            out.push_back('\n');
        }
        return out;
    }

private:
    std::string heading_;
    std::vector<std::unique_ptr<Paragraph>> children_;
};

// --- Abstract Document + concrete formats ------------------------------------

/// Abstract document: root of the composite tree AND the polymorphic family.
class Document {
public:
    Document(std::string filename, DocumentFormat format)
        : filename_(std::move(filename)), format_(format) {}
    virtual ~Document() noexcept = default;

    [[nodiscard]] const std::string& filename() const noexcept { return filename_; }
    [[nodiscard]] DocumentFormat format() const noexcept { return format_; }

    /// PURE VIRTUAL: sections of this document (composite children).
    [[nodiscard]] virtual const std::vector<std::unique_ptr<Section>>& sections() const = 0;
    /// Structure name for describe()/printing ("TextDocument", …).
    [[nodiscard]] virtual std::string kindName() const = 0;

    /// Builds sections from normalized text: blocks split by blank lines
    /// become sections, sentences split by ". " become sentences. Offsets are
    /// code-point ranges into the normalized text.
    void buildFromText(const std::string& normalizedText);

    [[nodiscard]] std::string describe() const {
        int words = 0;
        for (const auto& s : sections_) words += s->wordCount();
        return kindName() + " '" + filename_ + "': " +
               std::to_string(sections_.size()) + " sections, " +
               std::to_string(words) + " words";
    }

protected:
    std::vector<std::unique_ptr<Section>> sections_;

private:
    std::string filename_;
    DocumentFormat format_;
};

class TextDocument final : public Document {
public:
    TextDocument(std::string filename, std::string normalizedText)
        : Document(std::move(filename), DocumentFormat::Txt),
          normalizedText_(std::move(normalizedText)) {
        buildFromText(normalizedText_);
    }
    [[nodiscard]] const std::string& normalizedText() const noexcept { return normalizedText_; }
    [[nodiscard]] const std::vector<std::unique_ptr<Section>>& sections() const override {
        return sections_;
    }
    [[nodiscard]] std::string kindName() const override { return "TextDocument"; }

private:
    std::string normalizedText_;
};

class PdfDocument final : public Document {
public:
    PdfDocument(std::string filename, std::string normalizedText)
        : Document(std::move(filename), DocumentFormat::Pdf),
          normalizedText_(std::move(normalizedText)) {
        buildFromText(normalizedText_);
    }
    [[nodiscard]] const std::string& normalizedText() const noexcept { return normalizedText_; }
    [[nodiscard]] const std::vector<std::unique_ptr<Section>>& sections() const override {
        return sections_;
    }
    [[nodiscard]] std::string kindName() const override { return "PdfDocument"; }

private:
    std::string normalizedText_;
};

class DocxDocument final : public Document {
public:
    DocxDocument(std::string filename, std::string normalizedText)
        : Document(std::move(filename), DocumentFormat::Docx),
          normalizedText_(std::move(normalizedText)) {
        buildFromText(normalizedText_);
    }
    [[nodiscard]] const std::string& normalizedText() const noexcept { return normalizedText_; }
    [[nodiscard]] const std::vector<std::unique_ptr<Section>>& sections() const override {
        return sections_;
    }
    [[nodiscard]] std::string kindName() const override { return "DocxDocument"; }

private:
    std::string normalizedText_;
};

/// DocumentFactory: the single creation point for the Document family.
class DocumentFactory {
public:
    /// Creates the matching concrete Document for `format` from normalized text.
    [[nodiscard]] static std::unique_ptr<Document> create(DocumentFormat format,
                                                          std::string filename,
                                                          std::string normalizedText);
};

} // namespace verity::model
