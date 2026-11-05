"""BaseExtractor: the abstract extraction interface.

Contract:
* An extractor consumes ``{filename: normalized_text}`` and returns the
  extraction root defined by ``ai/schema/extraction.schema.json``.
* Rule-based extractors MUST inspect source text only; they never read
  ``data/gold``. The one deliberate exception is :class:`MockExtractor`,
  which replays hand-written gold annotations BY DESIGN and is labeled as
  such everywhere it appears.
"""

from __future__ import annotations

from abc import ABC, abstractmethod


class BaseExtractor(ABC):
    """Abstract base for all extraction strategies."""

    def __init__(self, documents: dict[str, str]) -> None:
        """``documents`` maps filename -> normalized text."""
        self.documents = documents
        super().__init__()

    @abstractmethod
    def extract(self) -> dict:
        """Returns a schema-valid extraction root dictionary."""
        raise NotImplementedError
