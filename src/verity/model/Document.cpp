#include "verity/model/Document.hpp"

#include <cctype>

#include "verity/util/VerityError.hpp"

namespace verity::model {

void Document::buildFromText(const std::string& normalizedText) {
    sections_.clear();
    // Split the normalized text into blocks on blank lines; each block is a
    // Section whose heading is its first line (or "Section N"). Sentences end
    // at '.', '!', '?' followed by whitespace or end-of-block.
    std::size_t pos = 0;
    int sectionIndex = 0;
    const std::size_t textLen = normalizedText.size();

    auto isBlank = [&](std::size_t p) {
        return p >= textLen || normalizedText[p] == '\n';
    };

    while (pos < textLen) {
        // skip blank lines
        while (pos < textLen && isBlank(pos)) ++pos;
        if (pos >= textLen) break;

        // find the block: until blank line or EOF
        const std::size_t blockStart = pos;
        std::size_t blockEnd = pos;
        while (blockEnd < textLen && normalizedText[blockEnd] != '\n') ++blockEnd;
        // extend through consecutive non-blank lines
        std::size_t scan = blockEnd;
        while (scan < textLen) {
            std::size_t lineStart = scan + 1;
            std::size_t lineEnd = lineStart;
            while (lineEnd < textLen && normalizedText[lineEnd] != '\n') ++lineEnd;
            if (lineStart >= lineEnd) break;   // blank line ends the block
            blockEnd = lineEnd;
            scan = lineEnd;
        }

        Section& section = [&]() -> Section& {
            sections_.push_back(std::make_unique<Section>("Section " + std::to_string(++sectionIndex)));
            return *sections_.back();
        }();
        Paragraph& paragraph = section.addParagraph();

        // Sentence split inside [blockStart, blockEnd): a sentence ends at
        // '.', '!' or '?' followed by whitespace, or at the end of the block.
        // The terminating punctuation is part of the sentence text.
        std::size_t sentStart = blockStart;
        for (std::size_t i = blockStart; i <= blockEnd; ++i) {
            const bool atBlockEnd = i == blockEnd;
            const bool atPunctuation =
                !atBlockEnd && (normalizedText[i] == '.' || normalizedText[i] == '!' ||
                                normalizedText[i] == '?') &&
                (i + 1 < textLen && (normalizedText[i + 1] == ' ' ||
                                     normalizedText[i + 1] == '\n'));
            if (!atBlockEnd && !atPunctuation) continue;

            const std::size_t sentEnd = atPunctuation ? i + 1 : i;
            std::string sentence = normalizedText.substr(sentStart, sentEnd - sentStart);
            const auto first = sentence.find_first_not_of(" \t");
            if (first == std::string::npos) {
                sentStart = sentEnd;
                continue;
            }
            const auto last = sentence.find_last_not_of(" \t");
            sentence = sentence.substr(first, last - first + 1);
            Sentence& s = paragraph.addSentence(sentence);
            s.setOffsets(sentStart + first, sentStart + last + 1);
            sentStart = sentEnd;   // the next sentence starts after the delimiter
        }
        section.setOffsets(blockStart, blockEnd);
        pos = blockEnd;
    }
}

std::unique_ptr<Document> DocumentFactory::create(DocumentFormat format, std::string filename,
                                                  std::string normalizedText) {
    switch (format) {
        case DocumentFormat::Txt:
            return std::make_unique<TextDocument>(std::move(filename), std::move(normalizedText));
        case DocumentFormat::Pdf:
            return std::make_unique<PdfDocument>(std::move(filename), std::move(normalizedText));
        case DocumentFormat::Docx:
            return std::make_unique<DocxDocument>(std::move(filename), std::move(normalizedText));
    }
    throw util::VerityError("unknown DocumentFormat");
}

} // namespace verity::model
