#include "verity/io/JsonLoader.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "verity/model/Enums.hpp"
#include "verity/model/NodeId.hpp"
#include "verity/model/Predicates.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::io {

namespace {

[[noreturn]] void fail(const std::string& what) { throw util::ParseError(what); }

const nlohmann::json& requireField(const nlohmann::json& obj, const char* field,
                                   const std::string& context) {
    if (!obj.is_object() || !obj.contains(field)) {
        fail(context + ": missing required field '" + field + "'");
    }
    return obj.at(field);
}

std::string requireString(const nlohmann::json& obj, const char* field,
                          const std::string& context) {
    const auto& v = requireField(obj, field, context);
    if (!v.is_string()) fail(context + ": field '" + std::string(field) + "' must be a string");
    return v.get<std::string>();
}

/// Verifies a span against the document's normalized text: quote must equal
/// the exact code-point slice text[start,end).
void verifySpan(const model::Span& span, const std::string& normalizedText,
                const std::string& context) {
    if (span.end < span.start) fail(context + ": span end < start");
    if (span.quote.empty() && span.start == span.end) return;  // empty quote OK
    if (span.quote.empty()) fail(context + ": span with non-empty range but empty quote");
    const std::size_t byteStart =
        model::byteOffsetOfCodePoint(normalizedText, span.start);  // throws on range
    const std::size_t byteEnd =
        model::byteOffsetOfCodePoint(normalizedText, span.end);
    const std::string actual = normalizedText.substr(byteStart, byteEnd - byteStart);
    if (actual != span.quote) {
        fail(context + ": span quote mismatch — expected '" + actual + "' but quote is '" +
             span.quote + "'");
    }
}

model::Span parseSpan(const nlohmann::json& j, const std::string& context) {
    model::Span s;
    const auto& start = requireField(j, "start", context);
    const auto& end = requireField(j, "end", context);
    if (!start.is_number_unsigned() || !end.is_number_unsigned()) {
        fail(context + ": span start/end must be non-negative integers");
    }
    s.start = start.get<std::size_t>();
    s.end = end.get<std::size_t>();
    s.quote = requireString(j, "quote", context);
    return s;
}

} // namespace

model::ExtractionRecord JsonLoader::parseExtraction(const std::string& jsonText) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(jsonText);
    } catch (const nlohmann::json::parse_error& e) {
        fail(std::string("invalid JSON: ") + e.what());
    }
    if (!root.is_object()) fail("extraction root must be a JSON object");

    model::ExtractionRecord rec;
    const std::string version = requireString(root, "schema_version", "root");
    if (version != "1.0.0") fail("unsupported schema_version: '" + version + "'");
    rec.schema_version = version;

    // ---- documents --------------------------------------------------------
    const auto& docs = requireField(root, "documents", "root");
    if (!docs.is_array() || docs.empty()) fail("root.documents must be a non-empty array");
    std::set<std::string> docIds;
    for (const auto& d : docs) {
        model::DocumentRecord dr;
        dr.id = requireString(d, "id", "document");
        const model::NodeId validated(dr.id);   // throws on malformed format
        (void)validated;
        dr.filename = requireString(d, "filename", "document " + dr.id);
        const std::string fmt = requireString(d, "format", "document " + dr.id);
        if (fmt != "txt" && fmt != "pdf" && fmt != "docx") {
            fail("document " + dr.id + ": unknown format '" + fmt + "'");
        }
        dr.format = fmt;
        dr.normalized_text = requireString(d, "normalized_text", "document " + dr.id);
        if (dr.normalized_text.empty() || dr.normalized_text.back() != '\n') {
            fail("document " + dr.id + ": normalized_text must end with a newline");
        }
        if (dr.normalized_text.find('\r') != std::string::npos) {
            fail("document " + dr.id + ": normalized_text must not contain CR (normalize first)");
        }
        if (dr.normalized_text.size() >= 2 && dr.normalized_text[dr.normalized_text.size() - 2] == '\n') {
            fail("document " + dr.id + ": normalized_text must end with exactly ONE newline");
        }
        if (d.contains("metadata")) {
            if (!d.at("metadata").is_object()) fail("document " + dr.id + ": metadata must be an object");
            dr.metadata = d.at("metadata");
        }
        if (d.contains("citations")) {
            const auto& cites = d.at("citations");
            if (!cites.is_array()) fail("document " + dr.id + ": citations must be an array");
            for (const auto& c : cites) {
                if (!c.is_string()) fail("document " + dr.id + ": citation must be a string id");
                dr.citations.push_back(c.get<std::string>());
            }
        }
        if (!docIds.insert(dr.id).second) fail("duplicate document id: " + dr.id);
        rec.documents.push_back(std::move(dr));
    }
    // citation references must point at existing documents
    for (const auto& d : rec.documents) {
        for (const std::string& cited : d.citations) {
            if (!docIds.count(cited)) {
                fail("document " + d.id + ": citation references unknown document '" + cited + "'");
            }
        }
    }

    // ---- entities ---------------------------------------------------------
    const auto& ents = requireField(root, "entities", "root");
    if (!ents.is_array()) fail("root.entities must be an array");
    std::set<std::string> entityIds;
    for (const auto& e : ents) {
        model::EntityRecord er;
        er.id = requireString(e, "id", "entity");
        const model::NodeId validated(er.id);   // throws on malformed format
        (void)validated;
        if (er.id.empty() || er.id[0] != 'E') {
            fail("entity ids must use the E prefix: " + er.id);
        }
        er.name = requireString(e, "name", "entity " + er.id);
        if (er.name.empty()) fail("entity " + er.id + ": name must be non-empty");
        const std::string type = requireString(e, "type", "entity " + er.id);
        (void)model::entityTypeFromString(type);   // throws on unknown
        er.type = type;
        if (e.contains("aliases")) {
            const auto& aliases = e.at("aliases");
            if (!aliases.is_array()) fail("entity " + er.id + ": aliases must be an array");
            for (const auto& a : aliases) {
                if (!a.is_string()) fail("entity " + er.id + ": alias must be a string");
                er.aliases.push_back(a.get<std::string>());
            }
        }
        if (e.contains("mentions")) {
            const auto& mentions = e.at("mentions");
            if (!mentions.is_array()) fail("entity " + er.id + ": mentions must be an array");
            for (const auto& m : mentions) {
                model::Mention men;
                men.document_id = requireString(m, "document_id", "entity " + er.id + " mention");
                men.span = parseSpan(requireField(m, "span", "entity " + er.id + " mention"),
                                     "entity " + er.id + " mention");
                if (!docIds.count(men.document_id)) {
                    fail("entity " + er.id + ": mention references unknown document '" +
                         men.document_id + "'");
                }
                er.mentions.push_back(std::move(men));
            }
        }
        if (!entityIds.insert(er.id).second) fail("duplicate entity id: " + er.id);
        rec.entities.push_back(std::move(er));
    }

    // ---- claims -----------------------------------------------------------
    const auto& claims = requireField(root, "claims", "root");
    if (!claims.is_array()) fail("root.claims must be an array");
    // Pre-pass: happened_before claims bind EVENTS, so event ids must be
    // known while validating claims (full event parsing happens below).
    std::set<std::string> eventIds;
    if (root.contains("events") && root.at("events").is_array()) {
        for (const auto& ev : root.at("events")) {
            if (ev.is_object() && ev.contains("id") && ev.at("id").is_string()) {
                eventIds.insert(ev.at("id").get<std::string>());
            }
        }
    }
    std::set<std::string> claimIds;
    for (const auto& c : claims) {
        model::ClaimRecord cr;
        const std::string context = "claim";
        cr.id = requireString(c, "id", context);
        const model::NodeId validated(cr.id);   // throws on malformed format
        (void)validated;
        if (cr.id.empty() || cr.id[0] != 'C') fail("claim ids must use the C prefix: " + cr.id);
        cr.subject_id = requireString(c, "subject_id", "claim " + cr.id);
        cr.predicate = requireString(c, "predicate", "claim " + cr.id);
        if (!model::isKnownPredicate(cr.predicate)) {
            fail("claim " + cr.id + ": unknown predicate '" + cr.predicate + "'");
        }
        const auto& obj = requireField(c, "object", "claim " + cr.id);
        try {
            cr.object = model::ClaimObject::fromJson(obj);
        } catch (const util::VerityError&) {
            throw;
        } catch (const std::exception& e) {
            fail("claim " + cr.id + ": bad object — " + e.what());
        }
        // happened_before claims bind EVENTS (domain Event -> Event); all
        // other predicates bind entities.
        if (cr.predicate == "happened_before") {
            if (!eventIds.count(cr.subject_id)) {
                fail("claim " + cr.id + ": happened_before subject must be an event '" +
                     cr.subject_id + "'");
            }
            if (cr.object.kind != model::ClaimObject::Kind::EntityRef ||
                !eventIds.count(cr.object.entity_id)) {
                fail("claim " + cr.id + ": happened_before object must be an event reference");
            }
        } else {
            if (cr.object.kind == model::ClaimObject::Kind::EntityRef &&
                !entityIds.count(cr.object.entity_id)) {
                fail("claim " + cr.id + ": object references unknown entity '" +
                     cr.object.entity_id + "'");
            }
            if (!entityIds.count(cr.subject_id)) {
                fail("claim " + cr.id + ": subject references unknown entity '" +
                     cr.subject_id + "'");
            }
        }
        const std::string polarity = requireString(c, "polarity", "claim " + cr.id);
        (void)model::polarityFromString(polarity);
        cr.polarity = polarity;
        const auto& conf = requireField(c, "confidence", "claim " + cr.id);
        if (!conf.is_number()) fail("claim " + cr.id + ": confidence must be a number");
        cr.confidence = conf.get<double>();
        if (!(cr.confidence >= 0.0 && cr.confidence <= 1.0)) {
            fail("claim " + cr.id + ": confidence outside [0,1]");
        }
        cr.document_id = requireString(c, "document_id", "claim " + cr.id);
        if (!docIds.count(cr.document_id)) {
            fail("claim " + cr.id + ": references unknown document '" + cr.document_id + "'");
        }
        cr.span = parseSpan(requireField(c, "span", "claim " + cr.id), "claim " + cr.id);
        if (c.contains("valid_from") && !c.at("valid_from").is_null()) {
            if (!c.at("valid_from").is_string()) fail("claim " + cr.id + ": valid_from must be a date string or null");
            const std::string vf = c.at("valid_from").get<std::string>();
            (void)model::DateInterval::fromString(vf);   // validate parse
            cr.valid_from = vf;
        }
        if (c.contains("valid_to") && !c.at("valid_to").is_null()) {
            if (!c.at("valid_to").is_string()) fail("claim " + cr.id + ": valid_to must be a date string or null");
            const std::string vt = c.at("valid_to").get<std::string>();
            (void)model::DateInterval::fromString(vt);
            cr.valid_to = vt;
        }
        if (!claimIds.insert(cr.id).second) fail("duplicate claim id: " + cr.id);
        rec.claims.push_back(std::move(cr));
    }

    // ---- events -----------------------------------------------------------
    const auto& events = requireField(root, "events", "root");
    if (!events.is_array()) fail("root.events must be an array");
    std::set<std::string> parsedEventIds;   // distinct from the claims pre-pass set
    for (const auto& ev : events) {
        model::EventRecord er;
        er.id = requireString(ev, "id", "event");
        const model::NodeId validated(er.id);   // throws on malformed format
        (void)validated;
        if (er.id.size() < 2 || er.id.substr(0, 2) != "EV") {
            fail("event ids must use the EV prefix: " + er.id);
        }
        er.label = requireString(ev, "label", "event " + er.id);
        er.entity_id = requireString(ev, "entity_id", "event " + er.id);
        if (!entityIds.count(er.entity_id)) {
            fail("event " + er.id + ": references unknown entity '" + er.entity_id + "'");
        }
        er.date = requireString(ev, "date", "event " + er.id);
        (void)model::DateInterval::fromString(er.date);   // validate
        er.document_id = requireString(ev, "document_id", "event " + er.id);
        if (!docIds.count(er.document_id)) {
            fail("event " + er.id + ": references unknown document '" + er.document_id + "'");
        }
        er.span = parseSpan(requireField(ev, "span", "event " + er.id), "event " + er.id);
        if (!parsedEventIds.insert(er.id).second) fail("duplicate event id: " + er.id);
        rec.events.push_back(std::move(er));
    }

    // ---- citations (root-level) -------------------------------------------
    const auto& cites = requireField(root, "citations", "root");
    if (!cites.is_array()) fail("root.citations must be an array");
    for (const auto& ct : cites) {
        model::CitationRecord cr;
        cr.citing = requireString(ct, "citing", "citation");
        cr.cited = requireString(ct, "cited", "citation");
        if (!docIds.count(cr.citing)) fail("citation references unknown citing document '" + cr.citing + "'");
        if (!docIds.count(cr.cited)) fail("citation references unknown cited document '" + cr.cited + "'");
        if (ct.contains("span") && !ct.at("span").is_null()) {
            cr.span = parseSpan(requireField(ct, "span", "citation " + cr.citing + "->" + cr.cited),
                                "citation " + cr.citing + "->" + cr.cited);
        }
        rec.citations.push_back(std::move(cr));
    }

    validate(root, rec);
    return rec;
}

model::ExtractionRecord JsonLoader::loadExtractionFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) fail("cannot open extraction file: '" + path + "'");
    std::stringstream buffer;
    buffer << in.rdbuf();
    if (in.bad()) fail("failed reading extraction file: '" + path + "'");
    return parseExtraction(buffer.str());
}

void JsonLoader::validate(const nlohmann::json& root, const model::ExtractionRecord& parsed) {
    // Cross-checks that are cheaper to express on the parsed record. The
    // parser above already enforced structure; here we enforce span
    // exactness (needs the normalized texts) and global consistency.

    std::set<std::string> docIds;
    for (const auto& d : parsed.documents) docIds.insert(d.id);

    // every span in every record type must quote the normalized text exactly
    auto normalizedOf = [&](const std::string& docId) -> const std::string& {
        static const std::string empty;
        for (const auto& d : parsed.documents) {
            if (d.id == docId) return d.normalized_text;
        }
        return empty;
    };

    for (const auto& e : parsed.entities) {
        for (const auto& m : e.mentions) {
            verifySpan(m.span, normalizedOf(m.document_id),
                       "entity " + e.id + " mention @" + m.document_id);
        }
    }
    for (const auto& c : parsed.claims) {
        verifySpan(c.span, normalizedOf(c.document_id), "claim " + c.id);
    }
    for (const auto& ev : parsed.events) {
        verifySpan(ev.span, normalizedOf(ev.document_id), "event " + ev.id);
    }
    for (const auto& ct : parsed.citations) {
        if (ct.span) {
            verifySpan(*ct.span, normalizedOf(ct.citing), "citation " + ct.citing + "->" + ct.cited);
        }
    }

    // document citations duplicated at root level must be consistent sets
    std::set<std::pair<std::string, std::string>> docLevel;
    for (const auto& d : parsed.documents) {
        for (const std::string& cited : d.citations) docLevel.emplace(d.id, cited);
    }
    for (const auto& ct : parsed.citations) {
        if (!docLevel.count({ct.citing, ct.cited})) {
            fail("root citation " + ct.citing + "->" + ct.cited +
                 " is not declared by the citing document's citations list");
        }
    }

    // predicate-specific sanity: happened_before claims must carry DateValue
    // or reference events (validated later by TimelineBuilder); here we only
    // require that object entity refs exist (done above) and that the JSON
    // root is consistent with what we parsed (id sets agree).
    std::size_t jsonClaims = root.contains("claims") && root.at("claims").is_array()
                                 ? root.at("claims").size() : 0;
    if (jsonClaims != parsed.claims.size()) {
        fail("internal consistency: claim count mismatch after parsing");
    }
}

} // namespace verity::io
