#pragma once
// VERITY GRAPH — extraction data model (C++ mirror of
// ai/schema/extraction.schema.json, frozen in docs/CONTRACT.md §6–§7).
//
// Span semantics: half-open [start, end) in UNICODE CODE POINTS of the
// *normalized* extracted text (CRLF/CR normalized to LF, NFC, single trailing
// newline). `quote` must be the exact slice text[start, end) — validated at
// ingest by io::JsonLoader. Byte offsets are never mixed with code points.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "verity/model/Enums.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::model {

// ---------------------------------------------------------------------------
// Spans
// ---------------------------------------------------------------------------
struct Span {
    std::size_t start = 0;  // code point index into normalized text (inclusive)
    std::size_t end = 0;    // code point index (exclusive); end >= start
    std::string quote;      // exact substring; must equal text[start,end)

    [[nodiscard]] bool empty() const noexcept { return start == end; }
    [[nodiscard]] std::size_t length() const noexcept { return end - start; }
};

/// Number of Unicode code points in a UTF-8 string.
/// Complexity O(n bytes).
[[nodiscard]] inline std::size_t utf8CodePointCount(const std::string& text) noexcept {
    std::size_t count = 0;
    for (const unsigned char c : text) {
        if ((c & 0xC0) != 0x80) ++count;  // count non-continuation bytes
    }
    return count;
}

/// Byte offset of the code-point index `index` in UTF-8 `text`.
/// Throws ParseError when `index` exceeds the code-point count.
/// Complexity O(n bytes).
[[nodiscard]] inline std::size_t byteOffsetOfCodePoint(const std::string& text,
                                                       std::size_t index) {
    std::size_t cp = 0;
    std::size_t byte = 0;
    while (byte < text.size()) {
        if (cp == index) return byte;
        const unsigned char c = static_cast<unsigned char>(text[byte]);
        if ((c & 0xC0) != 0x80) ++cp;
        ++byte;
    }
    if (cp == index) return byte;
    throw util::ParseError("code point index " + std::to_string(index) +
                           " out of range (text has " + std::to_string(cp) + ")");
}

// ---------------------------------------------------------------------------
// Dates: precision-preserving intervals (frozen semantics, CONTRACT §6)
// ---------------------------------------------------------------------------
enum class DatePrecision { Year, Month, Day };

struct DatePoint {
    int year = 0;   // e.g. 2024
    int month = 0;  // 1..12, 0 = unspecified
    int day = 0;    // 1..31, 0 = unspecified
};

struct DateInterval {
    DatePrecision precision = DatePrecision::Day;
    DatePoint start;  // inclusive
    DatePoint end;    // inclusive

    /// Parse "YYYY" | "YYYY-MM" | "YYYY-MM-DD" into a precision-preserving
    /// point interval (start == end == parsed point).
    [[nodiscard]] static DateInterval fromString(const std::string& s) {
        DateInterval iv;
        iv.start = parsePoint(s);
        iv.end = iv.start;
        if (s.size() == 4) {
            iv.precision = DatePrecision::Year;
            iv.start.month = iv.start.day = 0;
            iv.end.month = iv.end.day = 0;
        } else if (s.size() == 7) {
            iv.precision = DatePrecision::Month;
            iv.start.day = iv.end.day = 0;
        } else {
            iv.precision = DatePrecision::Day;
        }
        return iv;
    }

    /// Parse a range object {"from": "...", "to": "..."} or a single string.
    [[nodiscard]] static DateInterval fromJson(const nlohmann::json& j) {
        if (j.is_string()) return fromString(j.get<std::string>());
        if (j.is_object() && j.contains("from") && j.contains("to")) {
            DateInterval a = fromString(j.at("from").get<std::string>());
            DateInterval b = fromString(j.at("to").get<std::string>());
            if (pointLess(b.end, a.start)) {
                throw util::ParseError("date interval end before start");
            }
            DateInterval iv;
            iv.precision = (a.precision == b.precision) ? a.precision
                          : (a.precision < b.precision ? a.precision : b.precision);
            iv.start = a.start;
            iv.end = b.end;
            return iv;
        }
        throw util::ParseError("date must be \"YYYY[-MM[-DD]]\" or {\"from\",\"to\"}");
    }

    /// Expand a point interval to its full calendar range:
    /// Year 2024 -> [2024-01-01, 2024-12-31]; Month 2024-03 -> [01,31].
    [[nodiscard]] static DateInterval fromPoint(const DatePoint& p) {
        DateInterval iv;
        if (p.month == 0) {
            iv.precision = DatePrecision::Year;
            iv.start = {p.year, 1, 1};
            iv.end = {p.year, 12, 31};
        } else if (p.day == 0) {
            iv.precision = DatePrecision::Month;
            iv.start = {p.year, p.month, 1};
            iv.end = {p.year, p.month, daysInMonth(p.year, p.month)};
        } else {
            iv.precision = DatePrecision::Day;
            iv.start = iv.end = p;
        }
        return iv;
    }

    /// The stored interval (with unspecified parts treated as covering the
    /// whole coarser unit) overlaps the other interval.
    [[nodiscard]] bool overlaps(const DateInterval& other) const noexcept {
        const DateInterval a = expanded();
        const DateInterval b = other.expanded();
        return !pointLess(a.end, b.start) && !pointLess(b.end, a.start);
    }

    /// Whole of *this* lies strictly before whole of *other*.
    [[nodiscard]] bool strictlyBefore(const DateInterval& other) const noexcept {
        const DateInterval a = expanded();
        const DateInterval b = other.expanded();
        return pointLess(a.end, b.start);
    }

    [[nodiscard]] std::string toString() const {
        char buf[16];
        if (precision == DatePrecision::Year) {
            std::snprintf(buf, sizeof(buf), "%04d", start.year);
            return std::string(buf);
        }
        std::snprintf(buf, sizeof(buf), "%04d-%02d", start.year, start.month);
        std::string out(buf);
        if (precision == DatePrecision::Day) {
            std::snprintf(buf, sizeof(buf), "-%02d", start.day);
            out += buf;
        }
        return out;
    }

    [[nodiscard]] bool operator==(const DateInterval& o) const noexcept {
        return precision == o.precision && start.year == o.start.year &&
               start.month == o.start.month && start.day == o.start.day;
    }

    [[nodiscard]] static int daysInMonth(int year, int month) {
        static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month < 1 || month > 12) return 0;
        if (month == 2 && isLeapYear(year)) return 29;
        return d[month - 1];
    }

    [[nodiscard]] static bool isLeapYear(int year) noexcept {
        return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    }

private:
    [[nodiscard]] DateInterval expanded() const noexcept {
        // Treat unspecified month/day as the full coarser unit.
        if (precision == DatePrecision::Year) return fromPoint(start);
        if (precision == DatePrecision::Month) return fromPoint(start);
        return *this;
    }

    [[nodiscard]] static bool pointLess(const DatePoint& a, const DatePoint& b) noexcept {
        if (a.year != b.year) return a.year < b.year;
        if (a.month != b.month) return a.month < b.month;
        return a.day < b.day;
    }

    [[nodiscard]] static DatePoint parsePoint(const std::string& s) {
        auto fail = [&s]() { throw util::ParseError("bad date: '" + s + "'"); };
        if (s.empty()) fail();
        DatePoint p{};
        std::size_t pos = 0;
        auto read_num = [&](std::size_t digits) -> int {
            if (pos + digits > s.size()) fail();
            int v = 0;
            for (std::size_t i = 0; i < digits; ++i) {
                const char c = s[pos + i];
                if (c < '0' || c > '9') fail();
                v = v * 10 + (c - '0');
            }
            pos += digits;
            return v;
        };
        p.year = read_num(4);
        if (pos < s.size()) {
            if (s[pos] != '-') fail();
            ++pos;
            p.month = read_num(2);
            if (p.month < 1 || p.month > 12) fail();
            if (pos < s.size()) {
                if (s[pos] != '-') fail();
                ++pos;
                p.day = read_num(2);
                if (p.day < 1 || p.day > daysInMonth(p.year, p.month)) fail();
            }
        }
        if (pos != s.size()) fail();
        return p;
    }
};

// ---------------------------------------------------------------------------
// Claim object values (typed object: entity ref | string | date | quantity)
// ---------------------------------------------------------------------------
struct ClaimObject {
    enum class Kind { EntityRef, StringValue, DateValue, QuantityValue };
    Kind kind = Kind::StringValue;
    std::string entity_id;      // Kind == EntityRef
    std::string text;           // Kind == StringValue
    DateInterval date{};        // Kind == DateValue
    double quantity = 0.0;      // Kind == QuantityValue
    std::string unit;           // required for QuantityValue

    [[nodiscard]] static ClaimObject fromJson(const nlohmann::json& j) {
        ClaimObject o;
        const std::string kind = j.at("kind").get<std::string>();
        if (kind == "EntityRef") {
            o.kind = Kind::EntityRef;
            o.entity_id = j.at("entity_id").get<std::string>();
        } else if (kind == "StringValue") {
            o.kind = Kind::StringValue;
            o.text = j.at("text").get<std::string>();
        } else if (kind == "DateValue") {
            o.kind = Kind::DateValue;
            o.date = DateInterval::fromJson(j.at("date"));
        } else if (kind == "QuantityValue") {
            o.kind = Kind::QuantityValue;
            o.quantity = j.at("quantity").get<double>();
            o.unit = j.at("unit").get<std::string>();
            if (o.unit.empty()) {
                throw util::ParseError("QuantityValue requires a unit");
            }
        } else {
            throw util::ParseError("Unknown ClaimObject kind: '" + kind + "'");
        }
        return o;
    }
};

// ---------------------------------------------------------------------------
// Extraction record structs (mirror of extraction.schema.json)
// ---------------------------------------------------------------------------
struct Mention {
    std::string document_id;
    Span span;
};

struct EntityRecord {
    std::string id;
    std::string name;
    std::string type;  // EntityType string
    std::vector<std::string> aliases;
    std::vector<Mention> mentions;
};

struct ClaimRecord {
    std::string id;
    std::string subject_id;
    std::string predicate;
    ClaimObject object;
    std::string polarity;   // Polarity string
    double confidence = 0.0;
    std::string document_id;
    Span span;
    std::optional<std::string> valid_from;  // date strings, optional
    std::optional<std::string> valid_to;
};

struct EventRecord {
    std::string id;
    std::string label;
    std::string entity_id;
    std::string date;  // YYYY[-MM[-DD]]
    std::string document_id;
    Span span;
};

struct DocumentRecord {
    std::string id;
    std::string filename;
    std::string format;  // txt | pdf | docx
    std::string normalized_text;
    nlohmann::json metadata = nlohmann::json::object();
    std::vector<std::string> citations;
};

struct CitationRecord {
    std::string citing;
    std::string cited;
    std::optional<Span> span;
};

struct ExtractionRecord {
    std::string schema_version;
    std::vector<DocumentRecord> documents;
    std::vector<EntityRecord> entities;
    std::vector<ClaimRecord> claims;
    std::vector<EventRecord> events;
    std::vector<CitationRecord> citations;
};

/// Normalize raw document text to the canonical form spans refer to:
/// CRLF and CR become LF; NFC composition is assumed upstream (input files
/// are prepared in NFC); trailing whitespace is reduced to a single "\n".
[[nodiscard]] inline std::string normalizeText(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\r') {
            if (i + 1 < raw.size() && raw[i + 1] == '\n') continue;  // skip CR of CRLF
            out.push_back('\n');
        } else {
            out.push_back(raw[i]);
        }
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == ' ' || out.back() == '\t')) {
        out.pop_back();
    }
    out.push_back('\n');
    return out;
}

} // namespace verity::model
