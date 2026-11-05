#!/usr/bin/env python
"""Generates the phase-2 PDF and DOCX sample documents (D006/D007 are phase-3
content, so these fixtures carry D001/D002-equivalent overview content and
serve as the FORMAT-VARIANT originals for reader tests).

The PDF is built with reportlab, the DOCX with python-docx — both from the
same canonical text so their normalized extraction is comparable. Development
tooling, not pipeline code.

Usage: .venv/Scripts/python.exe -I scripts/make_phase2_fixtures.py
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "ai"))

SAMPLE = ROOT / "data" / "sample"

PDF_TEXT = (
    "Format Variant Overview\n\n"
    "Aster Vale Systems publishes this document as a PDF format variant.\n"
    "The content mirrors the neutral overview so readers can be tested.\n"
    "Maria Chen remains the chief executive officer.\n"
)

DOCX_TEXT = (
    "Format Variant Overview\n\n"
    "Northstar Meridian Labs publishes this document as a DOCX format variant.\n"
    "David Okoro remains the chief executive officer.\n"
    "The content mirrors the neutral overview so readers can be tested.\n"
)


def make_pdf(path: Path, text: str) -> None:
    try:
        from reportlab.lib.pagesizes import A4
        from reportlab.pdfgen import canvas
    except ImportError as exc:  # pragma: no cover
        raise SystemExit("reportlab is required for PDF fixture generation") from exc
    c = canvas.Canvas(str(path), pagesize=A4)
    y = 780
    for line in text.split("\n"):
        c.drawString(72, y, line)
        y -= 18
    c.save()


def make_docx(path: Path, text: str) -> None:
    try:
        import docx
    except ImportError as exc:  # pragma: no cover
        raise SystemExit("python-docx is required for DOCX fixture generation") from exc
    document = docx.Document()
    for line in text.split("\n"):
        document.add_paragraph(line)
    document.save(str(path))


def main() -> int:
    make_pdf(SAMPLE / "d004_format_variant.pdf", PDF_TEXT)
    make_docx(SAMPLE / "d005_format_variant.docx", DOCX_TEXT)
    print(f"wrote {SAMPLE / 'd004_format_variant.pdf'}")
    print(f"wrote {SAMPLE / 'd005_format_variant.docx'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
