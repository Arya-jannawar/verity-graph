#pragma once
// VERITY GRAPH — model/Author: the VIRTUAL-inheritance diamond (OOP Unit 3).
//
//        Node (virtual base)
//        /              \
//   Person            SourceNode      (both : public virtual Node)
//        \              /
//         Author : public Person, public SourceNode
//
// Because Person and SourceNode inherit Node VIRTUALLY, an Author has EXACTLY
// ONE Node subobject (test: the Node* seen through both paths is the same
// address, and a direct conversion compiles — ambiguous conversions would not
// compile with non-virtual inheritance). As the most-derived class, Author
// initializes the virtual base directly.
//
// Use in Verity: authors of documents are persons that also act as citation
// sources — one identity, one Node id.

#include <string>

#include "verity/model/Node.hpp"

namespace verity::model {

class Person : public virtual Node {
public:
    Person(NodeId id, std::string name, std::string affiliation)
        : Node(std::move(id), std::move(name)), affiliation_(std::move(affiliation)) {}

    [[nodiscard]] const std::string& affiliation() const noexcept { return affiliation_; }
    void setAffiliation(std::string affiliation) { affiliation_ = std::move(affiliation); }

private:
    std::string affiliation_;
};

class SourceNode : public virtual Node {
public:
    SourceNode(NodeId id, std::string name, std::string url)
        : Node(std::move(id), std::move(name)), url_(std::move(url)) {}

    [[nodiscard]] const std::string& url() const noexcept { return url_; }
    void setUrl(std::string url) { url_ = std::move(url); }

private:
    std::string url_;
};

class Author final : public Person, public SourceNode {
public:
    /// Most-derived class: initializes the VIRTUAL BASE Node itself.
    Author(NodeId id, std::string name, std::string affiliation, std::string url)
        : Node(std::move(id), std::move(name)),
          Person(id, name, std::move(affiliation)),
          SourceNode(id, name, std::move(url)) {}

    [[nodiscard]] std::string describe() const {
        return id().str() + " Author '" + label() + "' <" + url() + "> @ " + affiliation();
    }
};

} // namespace verity::model
