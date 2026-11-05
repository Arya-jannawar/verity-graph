#pragma once
// VERITY GRAPH — io/GraphSerializer: versioned BINARY graph persistence with
// an offset index for random access (OOP Unit 5, Phase 4).
//
// File format (frozen, little-endian):
//   bytes 0-3    magic "VGVG"
//   bytes 4-7    format version (u32, currently 1)
//   then RECORDS: a record = kind u8 | idLen u32 | id bytes | payloadLen u32
//                 | payload bytes
//   then INDEX:   count u32; entries { idLen u32, id bytes, offset u64,
//                 kind u8 }
//   then TRAILER: indexOffset u64 (byte position of the INDEX section)
//
// Random access: loadIndex reads ONLY the trailer + index, then fetchRecord
// seeks (seekg) straight to the record — the whole record section is never
// parsed for a single lookup.
//
// Error policy: every stream failure (fail/eof/bad), short read, bad magic,
// unknown version, out-of-range offset or length is converted to
// StorageError. Bounds are validated BEFORE any seek or allocation. Saving is
// atomic: temp file + rename.
//
// Overloaded << and >> stream operators serialize the record structs
// (OOP Unit 5 requirement).
//
// The format is documented as NOT a raw C++ object dump: no pointers, no
// padding-dependent layout — every field is explicitly encoded.

#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "verity/ds/Graph.hpp"
#include "verity/engine/GraphBuilder.hpp"
#include "verity/model/NodeStore.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::io {

struct SerializedRecord {
    std::uint8_t kind = 0;       // 1=entity 2=claim 3=event 4=document 5=edge
    std::string id;
    std::string payload;         // kind-specific encoded fields
};

struct GraphFileIndex {
    struct Entry {
        std::uint8_t kind = 0;
        std::uint64_t offset = 0;   // byte offset of the record header
        std::uint32_t length = 0;   // total record bytes
    };
    std::map<std::string, Entry> byId;
    std::uint64_t indexOffset = 0;
    std::uint32_t version = 0;
    std::size_t recordCount = 0;
};

class GraphSerializer {
public:
    /// Serializes the engine state (store nodes + evidence-graph edges).
    [[nodiscard]] static std::vector<SerializedRecord> buildRecords(
        const verity::model::NodeStore& store, const verity::engine::EvidenceGraph& graph);

    /// Atomic save (temp + rename). Throws StorageError on any stream failure.
    static void save(const std::vector<SerializedRecord>& records, const std::string& path);

    /// Reads ONLY the trailer + index (random-access support).
    [[nodiscard]] static GraphFileIndex loadIndex(const std::string& path);

    /// Fetches ONE record by id via seekg without scanning the record section.
    [[nodiscard]] static SerializedRecord fetchRecord(const std::string& path,
                                                      const std::string& id);

    /// Full load (records + validation); used by `verity load`.
    [[nodiscard]] static std::vector<SerializedRecord> load(const std::string& path);

    // --- record stream operators (OOP Unit 5) -------------------------------
    friend std::ostream& operator<<(std::ostream& os, const SerializedRecord& r);
    friend std::istream& operator>>(std::istream& is, SerializedRecord& r);
};

} // namespace verity::io
