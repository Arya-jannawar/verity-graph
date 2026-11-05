#pragma once
// VERITY GRAPH — full exception hierarchy (frozen in docs/CONTRACT.md §11).
//
// Hierarchy:
//   VerityError                        (base, std::runtime_error)
//   ├── ParseError                     schema/JSON/rule-DSL/date parsing
//   ├── GraphError                     graph domain
//   │   ├── DuplicateNodeError
//   │   └── MissingNodeError
//   ├── ExtractionError                Python/AI boundary failures
//   ├── StorageError                   binary save/load / ledger failures
//   └── RuleError                      conflict-rule parsing/grounding
//
// All errors carry an actionable message. Tests catch by both base and
// derived types; destructors never throw.

#include <stdexcept>
#include <string>

namespace verity::util {

class VerityError : public std::runtime_error {
public:
    explicit VerityError(const std::string& message) : std::runtime_error(message) {}
};

class ParseError : public VerityError {
public:
    explicit ParseError(const std::string& message)
        : VerityError("ParseError: " + message) {}
};

class GraphError : public VerityError {
public:
    explicit GraphError(const std::string& message)
        : VerityError("GraphError: " + message) {}
};

class DuplicateNodeError : public GraphError {
public:
    explicit DuplicateNodeError(const std::string& id)
        : GraphError("DuplicateNodeError: node id already exists: " + id) {}
};

class MissingNodeError : public GraphError {
public:
    explicit MissingNodeError(const std::string& id)
        : GraphError("MissingNodeError: node id not found: " + id) {}
};

class ExtractionError : public VerityError {
public:
    explicit ExtractionError(const std::string& message)
        : VerityError("ExtractionError: " + message) {}
};

class StorageError : public VerityError {
public:
    explicit StorageError(const std::string& message)
        : VerityError("StorageError: " + message) {}
};

class RuleError : public VerityError {
public:
    explicit RuleError(const std::string& message)
        : VerityError("RuleError: " + message) {}
};

} // namespace verity::util
