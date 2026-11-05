#include "verity/io/GraphSerializer.hpp"

#include <cstdio>
#include <set>

#include "verity/model/ClaimNode.hpp"
#include "verity/model/DocumentNode.hpp"
#include "verity/model/Entity.hpp"
#include "verity/model/EventNode.hpp"

namespace verity::io {

namespace {
constexpr char kMagic[4] = {'V', 'G', 'V', 'G'};
constexpr std::uint32_t kVersion = 1;

void putU32(std::string& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}
void putU64(std::string& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}
std::uint32_t getU32(const std::string& in, std::size_t& pos) {
    if (pos + 4 > in.size()) throw util::StorageError("short read (u32)");
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        v |= static_cast<std::uint32_t>(static_cast<unsigned char>(in[pos++])) << (8 * i);
    }
    return v;
}
std::uint64_t getU64(const std::string& in, std::size_t& pos) {
    if (pos + 8 > in.size()) throw util::StorageError("short read (u64)");
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(static_cast<unsigned char>(in[pos++])) << (8 * i);
    }
    return v;
}
void putStr(std::string& out, const std::string& s) {
    putU32(out, static_cast<std::uint32_t>(s.size()));
    out.append(s);
}
std::string getStr(const std::string& in, std::size_t& pos, std::uint32_t maxLen = 64u * 1024u * 1024u) {
    const std::uint32_t len = getU32(in, pos);
    if (len > maxLen) {
        throw util::StorageError("string length out of range: " + std::to_string(len));
    }
    if (pos + len > in.size()) throw util::StorageError("short read (string)");
    std::string s = in.substr(pos, len);
    pos += len;
    return s;
}

[[noreturn]] void streamFailure(const std::string& what, const std::string& path) {
    throw util::StorageError(what + " (" + path + ")");
}
} // namespace

std::vector<SerializedRecord> GraphSerializer::buildRecords(
    const verity::model::NodeStore& store, const verity::engine::EvidenceGraph& graph) {
    std::vector<SerializedRecord> records;
    for (verity::model::Node* node : store.all()) {
        SerializedRecord r;
        const auto* entity = dynamic_cast<const verity::model::Entity*>(node);
        const auto* claim = dynamic_cast<const verity::model::ClaimNode*>(node);
        const auto* event = dynamic_cast<const verity::model::EventNode*>(node);
        const auto* document = dynamic_cast<const verity::model::DocumentNode*>(node);
        const char kUnitSep = static_cast<char>(31);
        if (entity != nullptr) {
            r.kind = 1;
            r.id = node->id().str();
            r.payload = std::string(verity::model::to_string(entity->entityType())) +
                        kUnitSep + node->label();
        } else if (claim != nullptr) {
            r.kind = 2;
            r.id = node->id().str();
            r.payload = claim->subjectId().str() + kUnitSep + claim->predicate() +
                        kUnitSep +
                        verity::model::ClaimNode::objectToString(claim->object()) +
                        kUnitSep +
                        std::string(verity::model::to_string(claim->polarity())) +
                        kUnitSep + std::to_string(claim->confidence()) + kUnitSep +
                        claim->documentId().str();
        } else if (event != nullptr) {
            r.kind = 3;
            r.id = node->id().str();
            r.payload = event->entityId().str() + kUnitSep + event->date().toString() +
                        kUnitSep + node->label() + kUnitSep + event->documentId().str();
        } else if (document != nullptr) {
            r.kind = 4;
            r.id = node->id().str();
            r.payload = std::string(verity::model::to_string(document->format())) +
                        kUnitSep + node->label();
        } else {
            continue;   // Author-style debug nodes are not persisted
        }
        records.push_back(std::move(r));
    }
    // edges: undirected pairs appear once via the canonical from<to key
    std::set<std::pair<std::string, std::string>> seen;
    const char kUnitSep = static_cast<char>(31);
    for (const auto& from : graph.nodes()) {
        for (const auto& [to, attr] : graph.neighbors(from)) {
            const std::string fromId = from.str();
            const std::string toId = to.str();
            const std::pair<std::string, std::string> canonical =
                fromId < toId ? std::make_pair(fromId, toId)
                              : std::make_pair(toId, fromId);
            if (!graph.directed() && !seen.insert(canonical).second) continue;
            if (graph.directed() && !seen.insert({fromId, toId}).second) continue;
            SerializedRecord r;
            r.kind = 5;
            r.id = fromId + ">" + toId;
            r.payload = fromId + kUnitSep + toId + kUnitSep +
                        std::string(verity::model::to_string(attr.relation)) +
                        kUnitSep + std::to_string(attr.weight);
            records.push_back(std::move(r));
        }
    }
    return records;
}

namespace {

void writeString(std::ostream& os, const std::string& s) {
    const std::uint32_t len = static_cast<std::uint32_t>(s.size());
    for (int i = 0; i < 4; ++i) {
        os.put(static_cast<char>((len >> (8 * i)) & 0xFF));
    }
    os.write(s.data(), static_cast<std::streamsize>(s.size()));
}

std::string readString(std::istream& is) {
    std::uint32_t len = 0;
    for (int i = 0; i < 4; ++i) {
        const int byte = is.get();
        if (byte == EOF) throw util::StorageError("short read in record string");
        len |= static_cast<std::uint32_t>(byte) << (8 * i);
    }
    if (len > 64u * 1024u * 1024u) {
        throw util::StorageError("record string length out of range");
    }
    std::string s(len, '\0');
    if (len > 0) {
        is.read(s.data(), static_cast<std::streamsize>(len));
        if (!is) throw util::StorageError("short read in record string body");
    }
    return s;
}

} // namespace

/// << writes the on-wire record form (kind byte + length-prefixed strings).
std::ostream& operator<<(std::ostream& os, const SerializedRecord& r) {
    os.put(static_cast<char>(r.kind));
    writeString(os, r.id);
    writeString(os, r.payload);
    return os;
}

/// >> reads exactly the form << writes; every failure becomes StorageError.
std::istream& operator>>(std::istream& is, SerializedRecord& r) {
    const int kind = is.get();
    if (kind == EOF) throw util::StorageError("short read in record kind");
    r.kind = static_cast<std::uint8_t>(kind);
    r.id = readString(is);
    r.payload = readString(is);
    return is;
}

void GraphSerializer::save(const std::vector<SerializedRecord>& records,
                           const std::string& path) {
    // build record section + index
    std::string recordSection;
    struct IndexEntryData {
        std::string id;
        std::uint8_t kind;
        std::uint64_t offset;
        std::uint32_t length;
    };
    std::vector<IndexEntryData> index;
    for (const SerializedRecord& r : records) {
        std::string record;
        record.push_back(static_cast<char>(r.kind));
        putStr(record, r.id);
        putStr(record, r.payload);
        // record offset = magic(4) + version(4) + bytes of previous records
        index.push_back({r.id, r.kind,
                         static_cast<std::uint64_t>(4 + 4 + recordSection.size()),
                         static_cast<std::uint32_t>(record.size())});
        recordSection += record;
    }
    std::string indexSection;
    putU32(indexSection, static_cast<std::uint32_t>(index.size()));
    for (const IndexEntryData& e : index) {
        putStr(indexSection, e.id);
        putU64(indexSection, e.offset);
        putU32(indexSection, e.length);
        indexSection.push_back(static_cast<char>(e.kind));
    }
    std::string trailer;
    const std::uint64_t indexOffset =
        static_cast<std::uint64_t>(4 + 4 + recordSection.size());
    putU64(trailer, indexOffset);

    const std::string tempPath = path + ".tmp";
    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out) streamFailure("cannot open for writing", tempPath);
        out.write(kMagic, 4);
        if (!out) streamFailure("magic write failed", tempPath);
        const std::uint32_t version = kVersion;
        out.write(reinterpret_cast<const char*>(&version), 4);
        if (!out) streamFailure("version write failed", tempPath);
        out.write(recordSection.data(), static_cast<std::streamsize>(recordSection.size()));
        if (!out) streamFailure("record write failed", tempPath);
        out.write(indexSection.data(), static_cast<std::streamsize>(indexSection.size()));
        if (!out) streamFailure("index write failed", tempPath);
        out.write(trailer.data(), static_cast<std::streamsize>(trailer.size()));
        out.flush();
        if (!out) streamFailure("trailer write failed", tempPath);
    }
    // atomic publish: remove a stale destination (best effort — its absence
    // is fine), then ALWAYS rename; only the rename result decides success.
    std::remove(path.c_str());
    if (std::rename(tempPath.c_str(), path.c_str()) != 0) {
        std::remove(tempPath.c_str());
        streamFailure("atomic rename failed", path);
    }
}

GraphFileIndex GraphSerializer::loadIndex(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) streamFailure("cannot open", path);
    in.seekg(0, std::ios::end);
    const std::streamsize fileSize = in.tellg();
    if (fileSize < 16) streamFailure("file too small", path);
    in.seekg(-8, std::ios::end);
    char trailerBytes[8];
    in.read(trailerBytes, 8);
    if (!in) streamFailure("trailer read failed", path);
    std::string trailer(trailerBytes, 8);
    std::size_t pos = 0;
    const std::uint64_t indexOffset = getU64(trailer, pos);
    if (indexOffset < 8 || indexOffset + 8 > static_cast<std::uint64_t>(fileSize)) {
        throw util::StorageError("index offset out of range (" + path + ")");
    }

    in.seekg(static_cast<std::streamoff>(indexOffset), std::ios::beg);
    std::string indexBytes(static_cast<std::size_t>(fileSize) - static_cast<std::size_t>(indexOffset) - 8, '\0');
    in.read(indexBytes.data(), static_cast<std::streamsize>(indexBytes.size()));
    if (!in) streamFailure("index read failed", path);

    pos = 0;
    const std::uint32_t count = getU32(indexBytes, pos);
    GraphFileIndex out;
    out.indexOffset = indexOffset;
    out.recordCount = count;
    for (std::uint32_t i = 0; i < count; ++i) {
        GraphFileIndex::Entry entry;
        const std::string id = getStr(indexBytes, pos);
        entry.offset = getU64(indexBytes, pos);
        entry.length = getU32(indexBytes, pos);
        if (pos >= indexBytes.size()) streamFailure("index truncated", path);
        entry.kind = static_cast<std::uint8_t>(indexBytes[pos++]);
        if (entry.length == 0 || entry.offset + entry.length > indexOffset) {
            throw util::StorageError("index entry out of range for id '" + id + "'");
        }
        out.byId[id] = entry;
    }
    return out;
}

SerializedRecord GraphSerializer::fetchRecord(const std::string& path,
                                              const std::string& id) {
    const GraphFileIndex index = loadIndex(path);
    const auto it = index.byId.find(id);
    if (it == index.byId.end()) {
        throw util::StorageError("record not found: '" + id + "' (" + path + ")");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) streamFailure("cannot open", path);
    in.seekg(static_cast<std::streamoff>(it->second.offset), std::ios::beg);
    std::string record(it->second.length, '\0');
    in.read(record.data(), static_cast<std::streamsize>(record.size()));
    if (!in) streamFailure("record read failed", path);

    std::size_t pos = 0;
    SerializedRecord out;
    out.kind = static_cast<std::uint8_t>(record[pos++]);
    out.id = getStr(record, pos);
    out.payload = getStr(record, pos);
    return out;
}

std::vector<SerializedRecord> GraphSerializer::load(const std::string& path) {
    const GraphFileIndex index = loadIndex(path);
    std::ifstream in(path, std::ios::binary);
    if (!in) streamFailure("cannot open", path);

    char magic[4];
    in.read(magic, 4);
    if (!in || magic[0] != 'V' || magic[1] != 'G' || magic[2] != 'V' || magic[3] != 'G') {
        throw util::StorageError("bad magic number (" + path + ")");
    }
    std::uint32_t version = 0;
    in.read(reinterpret_cast<char*>(&version), 4);
    if (!in) streamFailure("version read failed", path);
    if (version != kVersion) {
        throw util::StorageError("unsupported format version " + std::to_string(version));
    }

    std::vector<SerializedRecord> records;
    records.reserve(index.recordCount);
    for (const auto& [id, entry] : index.byId) {
        in.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
        std::string record(entry.length, '\0');
        in.read(record.data(), static_cast<std::streamsize>(record.size()));
        if (!in) streamFailure("record read failed", path);
        std::size_t pos = 0;
        SerializedRecord out;
        out.kind = static_cast<std::uint8_t>(record[pos++]);
        out.id = getStr(record, pos);
        out.payload = getStr(record, pos);
        records.push_back(std::move(out));
    }
    return records;
}

} // namespace verity::io
