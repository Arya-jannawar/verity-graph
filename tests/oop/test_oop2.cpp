// VERITY GRAPH — tests/oop/test_oop2.cpp
// OOP Unit 3: Document family + factory, Composite tree, Evidence siblings,
// Author virtual-inheritance diamond (single Node subobject), IScorable/
// IExportable interfaces.

#include "doctest/doctest.h"

#include <sstream>
#include <string>

#include "verity/model/Author.hpp"
#include "verity/model/ClaimNode.hpp"
#include "verity/model/Document.hpp"
#include "verity/model/Evidence.hpp"
#include "verity/model/Node.hpp"
#include "verity/model/NodeId.hpp"
#include "verity/util/VerityError.hpp"

using namespace verity::model;

TEST_CASE("DocumentFactory creates the right concrete family member") {
    auto txt = DocumentFactory::create(DocumentFormat::Txt, "a.txt", "Hello world.\n");
    CHECK(txt->kindName() == "TextDocument");
    auto pdf = DocumentFactory::create(DocumentFormat::Pdf, "b.pdf", "Second section.\n");
    CHECK(pdf->kindName() == "PdfDocument");
    auto docx = DocumentFactory::create(DocumentFormat::Docx, "c.docx", "Third one.\n");
    CHECK(docx->kindName() == "DocxDocument");
    // polymorphic dispatch through the abstract base
    const Document* base = txt.get();
    CHECK(base->kindName() == "TextDocument");
    CHECK(base->format() == DocumentFormat::Txt);
}

TEST_CASE("Composite tree: Document > Section > Paragraph > Sentence") {
    const std::string text =
        "First heading line.\n\n"
        "Alpha sentence one. Beta sentence two.\n\n"
        "Gamma sentence three.\n";
    TextDocument doc("note.txt", text);
    const auto& sections = doc.sections();
    CHECK_EQ(sections.size(), 3u);
    // section 2 has one paragraph with two sentences
    REQUIRE_EQ(sections[1]->paragraphs().size(), 1u);
    CHECK_EQ(sections[1]->paragraphs()[0]->sentences().size(), 2u);
    CHECK_EQ(sections[1]->paragraphs()[0]->sentences()[0]->text(), "Alpha sentence one.");
    CHECK_EQ(sections[1]->wordCount(), 6);   // alpha sentence one beta sentence two
    // offsets: sentence 1 of section 2 slices exactly from the normalized text
    const auto [start, end] = sections[1]->paragraphs()[0]->sentences()[0]->offsetRange();
    CHECK_EQ(text.substr(start, end - start), "Alpha sentence one.");
    // word counts aggregate up the tree
    int total = 0;
    for (const auto& s : sections) total += s->wordCount();
    CHECK_EQ(total, 10);   // 2 + 6 + 2
}

TEST_CASE("Evidence siblings: distinct kinds, == and <, interfaces") {
    SupportingEvidence s1(NodeId("C001"), NodeId("D001"), 0.9);
    ContradictingEvidence c1(NodeId("C001"), NodeId("D002"), 0.7);
    NeutralEvidence n1(NodeId("C001"), NodeId("D003"), 0.5);
    // SIBLINGS: all three convert to Evidence* and differ only by kind
    std::vector<const Evidence*> family{&s1, &c1, &n1};
    CHECK_EQ(family[0]->kind(), EvidenceKind::Supporting);
    CHECK_EQ(family[1]->kind(), EvidenceKind::Contradicting);
    CHECK_EQ(family[2]->kind(), EvidenceKind::Neutral);
    CHECK(family[0]->kindName() == "Supporting");

    // == compares every field; < orders deterministically
    SupportingEvidence s2(NodeId("C001"), NodeId("D001"), 0.9);
    CHECK(s1 == s2);
    SupportingEvidence s3(NodeId("C001"), NodeId("D001"), 0.8);
    CHECK_FALSE(s1 == s3);
    CHECK(s3 < s1);   // lower score sorts first within same claim/doc/kind

    // IScorable + IExportable through interface pointers
    const IScorable* scorable = &s1;
    CHECK_EQ(scorable->score(), doctest::Approx(0.9));
    const IExportable* exportable = &c1;
    CHECK(exportable->toJson().find("\"kind\": \"Contradicting\"") != std::string::npos);
    CHECK(exportable->toDot().find("\"D002\" -> \"C001\"") != std::string::npos);
}

TEST_CASE("ClaimNode implements IScorable (score == confidence)") {
    ClaimNode claim("p", NodeId("E001"), "has_ceo", std::string("X"), NodeId("D001"),
                    Span{0, 3, "abc"}, Polarity::Positive, 0.75, "C100");
    const IScorable* scorable = &claim;
    CHECK_EQ(scorable->score(), doctest::Approx(0.75));
}

TEST_CASE("Author diamond: exactly ONE Node subobject") {
    Author author(NodeId("E050"), "Jane Doe", "Bellhaven Press", "https://example.org/jane");
    // The Node seen through the Person path and the SourceNode path must be
    // THE SAME subobject — the defining property of virtual inheritance.
    Person* asPerson = &author;
    SourceNode* asSource = &author;
    Node* nodeViaPerson = asPerson;
    Node* nodeViaSource = asSource;
    CHECK(nodeViaPerson == nodeViaSource);
    CHECK(nodeViaPerson == &author);

    // one identity, one id
    CHECK(author.id().str() == "E050");
    CHECK(author.affiliation() == "Bellhaven Press");
    CHECK(author.url() == "https://example.org/jane");
    CHECK(author.describe().find("Jane Doe") != std::string::npos);
    CHECK(author.describe().find("Bellhaven Press") != std::string::npos);
}
