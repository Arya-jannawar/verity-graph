#pragma once
// VERITY GRAPH — io::JsonLoader: extraction-JSON parsing + real validation.
//
// Real validation means STRUCTURAL (required keys, types, enums, bounds) AND
// SEMANTIC (id format, cross-references, span exactness against normalized
// text in code points, date sanity, duplicate detection) checks. Bare JSON
// parsing is never called validation.
//
// Complexity: parse O(bytes); validation O(total JSON) + O(quotes) for span
// verification (each span verification is O(quote + text-prefix) due to
// code-point offset mapping).

#include <string>

#include <nlohmann/json.hpp>

#include "verity/model/Extraction.hpp"

namespace verity::io {

class JsonLoader {
public:
    /// Reads + parses + validates an extraction JSON file.
    [[nodiscard]] static model::ExtractionRecord loadExtractionFile(const std::string& path);

    /// Parses + validates an extraction JSON string (tests / embedded use).
    [[nodiscard]] static model::ExtractionRecord parseExtraction(const std::string& jsonText);

    /// Full structural + semantic validation of an already-parsed root.
    /// Throws util::ParseError with a precise message on the first problem.
    static void validate(const nlohmann::json& root, const model::ExtractionRecord& parsed);
};

} // namespace verity::io
