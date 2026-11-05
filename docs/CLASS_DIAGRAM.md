# CLASS_DIAGRAM.md — matches the real code

```mermaid
classDiagram
    class Node {
        <<abstract>>
        -NodeId id_
        -string label_
        +type()*
        +describe()*
        +toJson()*
        +clone()*
        +nextId(prefix)$
        +observeId(id)$
        +liveObjects()$
    }
    class Entity {
        -EntityType entityType_
        -vector~string~ aliases_
        +operator==(Entity)
        +operator<(Entity)
        +operator+(Entity)
    }
    class ClaimNode {
        -NodeId subjectId_
        -string predicate_
        -ClaimObject object_
        -Polarity polarity_
        -double confidence_
        +setConfidence(double) validates [0,1]
        +withConfidence(c) chaining
        +operator>>(istream)
        +score() IScorable
    }
    class EventNode {
        -DateInterval date_
        +onDate(d) chaining
    }
    class DocumentNode {
        -DocumentFormat format_
        -string normalizedText_
    }
    class NodeStore {
        -vector~Node*~ raw owned
        -HashTable index_
        +add(unique_ptr~Node~)
        +find(NodeId)
        Rule of Five: copy/move/dtor hand-written
    }
    class NodeId {
        -string raw_
        canonicalizes to padded form
    }
    class Document {
        <<abstract>>
        +sections()*
        +buildFromText(text)
    }
    class TextDocument
    class PdfDocument
    class DocxDocument
    class DocumentFactory {
        +create(format, filename, text)$
    }
    class TextUnit {
        <<abstract>>
        +wordCount()*
    }
    class Section
    class Paragraph
    class Sentence
    class Evidence {
        <<abstract>>
        -NodeId claimId_
        -NodeId documentId_
        +kind()*
        +score() IScorable
        +toJson/toDot IExportable
    }
    class SupportingEvidence
    class ContradictingEvidence
    class NeutralEvidence
    class Person {
        <<virtual Node>>
    }
    class SourceNode {
        <<virtual Node>>
    }
    class Author {
        single Node subobject (virtual inheritance)
    }
    class AliasSet {
        monoid: union with empty identity
    }
    class IScorable {
        <<interface>>
        +score()*
    }
    class IExportable {
        <<interface>>
        +toJson()*
        +toDot()*
    }

    Node <|-- Entity
    Node <|-- ClaimNode
    Node <|-- EventNode
    Node <|-- DocumentNode
    Node <|-- Person
    Node <|-- SourceNode
    Person <|-- Author
    SourceNode <|-- Author
    Node o-- NodeId
    NodeStore o-- Node : owns
    Entity *-- AliasSet : alias domain
    ClaimNode ..|> IScorable
    Evidence ..|> IScorable
    Evidence ..|> IExportable
    Evidence <|-- SupportingEvidence
    Evidence <|-- ContradictingEvidence
    Evidence <|-- NeutralEvidence
    Document <|-- TextDocument
    Document <|-- PdfDocument
    Document <|-- DocxDocument
    Document *-- Section : composite
    TextUnit <|-- Section
    TextUnit <|-- Paragraph
    TextUnit <|-- Sentence
    Section *-- Paragraph
    Paragraph *-- Sentence
    DocumentFactory ..> Document : creates
```

Key structural facts (each verified by a named test):

| Fact | Test |
|---|---|
| exactly one virtual Node base in the Author diamond | tests/oop/test_oop2.cpp "Author diamond" |
| NodeStore deep copy + self-assignment + move-from validity | tests/oop/test_nodestore.cpp |
| Evidence siblings (no chain) | tests/oop/test_oop2.cpp "Evidence siblings" |
| validated setter throws outside [0,1] | tests/oop/test_model.cpp "encapsulation" |
| AliasSet is a commutative monoid (empty identity) | tests/dm/test_algebra.cpp |
| Composite offsets slice exactly | tests/oop/test_oop2.cpp "Composite tree" |
