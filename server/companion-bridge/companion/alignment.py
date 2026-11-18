"""Multi-tier Fuzzy Book Resolver and Spoiler Cutoff Resolver.

Handles real-world misalignments between the EPUB on the e-reader and the
canonical EPUB indexed from Audiobookshelf:
  * Re-compressed, Calibre-polished, or renamed files where `ko_md5` and
    `<dc:identifier>` differ;
  * Split or merged spine chapters where `spine_index` or HTML byte offsets drift.
"""

from __future__ import annotations

from dataclasses import dataclass
from difflib import SequenceMatcher
import math
import re

from companion.epub_indexer import ParsedBook, TextChunk

try:
    from rapidfuzz import fuzz as _rapidfuzz
except ImportError:
    _rapidfuzz = None


def normalize_book_label(raw: str) -> str:
    """Normalize a book title, author, or filename for fuzzy matching."""
    if not raw:
        return ""
    s = raw.lower()
    # Remove file extensions
    s = re.sub(r"\.(epub|kepub|azw3|mobi|pdf)$", "", s)
    # Remove bracketed/parenthesized edition/compression tags: (v2), [Retail], (Unabridged)
    s = re.sub(r"\[[^\]]*\]|\([^)]*\)", " ", s)
    # Remove common series numbering prefixes like "01 - ", "Book 2: "
    s = re.sub(r"^\s*(?:book|vol(?:ume)?|part)?\s*\d+\s*[-_.:]\s*", "", s)
    # Strip non-alphanumeric characters
    s = re.sub(r"[^a-z0-9]+", " ", s)
    return " ".join(s.split())


def token_similarity(a: str, b: str) -> float:
    """Return a [0.0, 100.0] token-set/token-sort similarity score.

    Uses RapidFuzz when installed, with a pure-stdlib fallback so host unit
    tests work in any environment.
    """
    na = normalize_book_label(a)
    nb = normalize_book_label(b)
    if not na or not nb:
        return 0.0
    if na == nb:
        return 100.0
    if _rapidfuzz is not None:
        return float(max(_rapidfuzz.token_set_ratio(na, nb), _rapidfuzz.token_sort_ratio(na, nb)))

    tokens_a = set(na.split())
    tokens_b = set(nb.split())
    common = tokens_a & tokens_b
    if common:
        # Mirror RapidFuzz token_set_ratio: compare intersection against each sorted token string
        inter_s = " ".join(sorted(common))
        rem_a = " ".join(sorted(common | tokens_a))
        rem_b = " ".join(sorted(common | tokens_b))
        scores = [
            SequenceMatcher(None, inter_s, rem_a).ratio(),
            SequenceMatcher(None, inter_s, rem_b).ratio(),
            SequenceMatcher(None, rem_a, rem_b).ratio(),
        ]
        # Only boost with pure subset score when the shorter string has at least 2 tokens
        # or >= 6 chars so single stop-words don't falsely match.
        if len(common) >= 2 or len(inter_s) >= 6:
            return max(scores) * 100.0
    return SequenceMatcher(None, na, nb).ratio() * 100.0


@dataclass
class BookQueryIdentity:
    abs_item_id: str = ""
    book_uuid: str = ""
    isbn: str = ""
    asin: str = ""
    ko_md5: str = ""
    title: str = ""
    author: str = ""
    filename: str = ""
    anchor_text: str = ""
    selected_text: str = ""


@dataclass
class CutoffResolution:
    tier: str  # "anchor_exact", "anchor_fuzzy", "chapter_progress", "book_progress"
    cutoff_spine_index: int
    cutoff_char_offset: int
    cutoff_global_char: int
    matched_chapter_title: str
    progress_label: str


class BookResolver:
    """Resolves a device's book query metadata to a canonical indexed book."""

    FUZZY_THRESHOLD = 82.0

    def __init__(self) -> None:
        self.books: dict[str, ParsedBook] = {}
        self._alias_cache: dict[str, str] = {}

    def register_book(self, book: ParsedBook) -> None:
        self.books[book.canonical_book_id] = book
        all_ids = [book.canonical_book_id, book.package_id, book.isbn, book.asin, book.ko_md5] + list(book.identifiers)
        for ident in all_ids:
            if not ident:
                continue
            k = ident.strip().lower()
            self._alias_cache[k] = book.canonical_book_id
            if k.startswith("urn:uuid:"):
                self._alias_cache[k[9:].strip()] = book.canonical_book_id
            elif k.startswith("uuid:"):
                self._alias_cache[k[5:].strip()] = book.canonical_book_id

    def resolve(self, query: BookQueryIdentity) -> tuple[ParsedBook | None, str]:
        """Return `(matched_book, resolution_method)` or `(None, 'not_found')`."""
        # Tier 1: Exact ID / Hash / Alias cache match (ABS item ID first)
        for key in (query.abs_item_id, query.book_uuid, query.isbn, query.asin, query.ko_md5):
            k = (key or "").strip().lower()
            if k and k in self._alias_cache:
                cid = self._alias_cache[k]
                if cid in self.books:
                    return self.books[cid], "exact_id"
            # Also check stripped urn:uuid: prefix
            if k.startswith("urn:uuid:"):
                bare = k[9:].strip()
                if bare in self._alias_cache:
                    return self.books[self._alias_cache[bare]], "exact_id"

        # Tier 2: Normalized Title + Author / Filename Fuzzy Match
        best_book: ParsedBook | None = None
        best_score = 0.0
        q_title = query.title or query.filename
        q_author = query.author

        if q_title:
            for book in self.books.values():
                title_score = token_similarity(q_title, book.title)
                fname_score = token_similarity(query.filename, book.filename) if query.filename else 0.0
                combined_q = f"{q_title} {q_author}".strip()
                combined_b = f"{book.title} {book.author}".strip()
                combined_score = token_similarity(combined_q, combined_b)

                # If both have non-empty authors that completely disagree, penalize slightly
                # unless the title itself is a near-exact match.
                if q_author and book.author:
                    auth_sim = token_similarity(q_author, book.author)
                    score = max(combined_score, title_score * 0.8 + auth_sim * 0.2, fname_score)
                else:
                    score = max(title_score, combined_score, fname_score)

                if score > best_score:
                    best_score = score
                    best_book = book

        if best_book is not None and best_score >= self.FUZZY_THRESHOLD:
            if query.ko_md5:
                self._alias_cache[query.ko_md5.strip().lower()] = best_book.canonical_book_id
            return best_book, f"fuzzy_metadata({best_score:.0f}%)"

        # Tier 3: Corpus Anchor-Text Search Fallback (when metadata is completely mangled)
        anchor = (query.anchor_text or "").strip()
        if len(anchor) >= 24:
            norm_anchor = _normalize_prose_for_search(anchor)
            anchor_words = norm_anchor.split()
            if len(anchor_words) >= 5:
                best_corpus_book: ParsedBook | None = None
                best_overlap = 0.0
                for book in self.books.values():
                    for chunk in book.chunks:
                        norm_chunk = _normalize_prose_for_search(chunk.text)
                        if norm_anchor in norm_chunk:
                            return book, "anchor_corpus_exact"
                        # Sliding 6-gram overlap check
                        overlap = _ngram_containment(anchor_words, norm_chunk.split(), n=5)
                        if overlap > best_overlap:
                            best_overlap = overlap
                            best_corpus_book = book
                if best_corpus_book is not None and best_overlap >= 0.65:
                    return best_corpus_book, f"anchor_corpus_fuzzy({best_overlap:.0%})"

        return None, "not_found"


def _normalize_prose_for_search(text: str) -> str:
    return " ".join(re.findall(r"[a-z0-9]+", text.lower()))


def _ngram_containment(query_tokens: list[str], target_tokens: list[str], n: int = 5) -> float:
    if len(query_tokens) < n or len(target_tokens) < n:
        q_set = set(query_tokens)
        t_set = set(target_tokens)
        return len(q_set & t_set) / max(1, len(q_set))
    q_grams = {tuple(query_tokens[i : i + n]) for i in range(len(query_tokens) - n + 1)}
    t_grams = {tuple(target_tokens[i : i + n]) for i in range(len(target_tokens) - n + 1)}
    if not q_grams:
        return 0.0
    return len(q_grams & t_grams) / len(q_grams)


def locate_anchor_in_text(chunk_text: str, anchor_text: str) -> int | None:
    """Locate the end character offset of `anchor_text` inside `chunk_text`.

    Returns the character index in `chunk_text` immediately after `anchor_text`
    (so any text after `anchor_text` in the same chunk can be truncated), or
    `None` if `anchor_text` is not in `chunk_text`.
    """
    anchor_clean = " ".join(anchor_text.split()).strip()
    if len(anchor_clean) < 12:
        return None

    # 1. Case-insensitive direct substring match
    idx = chunk_text.lower().find(anchor_clean.lower())
    if idx != -1:
        return idx + len(anchor_clean)

    # 2. Match suffix window of anchor_text (last 5 to 8 words) with punctuation-agnostic regex
    words = re.findall(r"[A-Za-z0-9']+", anchor_clean)
    if len(words) < 4:
        return None
    for tail_len in (min(8, len(words)), min(5, len(words)), 4):
        tail_words = words[-tail_len:]
        pattern = r"\b" + r"[\W_]+".join(re.escape(w) for w in tail_words) + r"\b"
        m = re.search(pattern, chunk_text, flags=re.IGNORECASE)
        if m:
            return m.end()
    return None


class CutoffResolver:
    """Determines the exact spoiler-safe reading cutoff across potentially misaligned EPUBs."""

    CHAPTER_SAFETY_BUFFER = 0.03  # 3% conservative intra-chapter margin on fuzzy fallback
    BOOK_SAFETY_BUFFER = 0.02     # 2% conservative whole-book margin on last-resort fallback

    @classmethod
    def resolve_cutoff(
        cls,
        book: ParsedBook,
        spine_index: int = 0,
        chapter_title: str = "",
        visible_offset: int | None = None,
        chapter_progress: float | None = None,
        book_progress: float | None = None,
        anchor_text: str = "",
    ) -> CutoffResolution:
        # -------------------------------------------------------------------
        # Tier 1: Anchor-Text Locator (immune to Calibre splitting/formatting)
        # -------------------------------------------------------------------
        if anchor_text and len(anchor_text.strip()) >= 15:
            # Check the reported spine_index first, then expand outward
            ordered_chunks = sorted(
                book.chunks,
                key=lambda c: (0 if c.spine_index == spine_index else 1, abs(c.spine_index - spine_index), c.chunk_index),
            )
            for chunk in ordered_chunks:
                end_in_chunk = locate_anchor_in_text(chunk.text, anchor_text)
                if end_in_chunk is not None:
                    exact_char = chunk.char_start + end_in_chunk
                    exact_global = chunk.global_char_start + end_in_chunk
                    pct = int(round(100.0 * exact_global / max(1, book.total_chars)))
                    return CutoffResolution(
                        tier="anchor_exact",
                        cutoff_spine_index=chunk.spine_index,
                        cutoff_char_offset=exact_char,
                        cutoff_global_char=exact_global,
                        matched_chapter_title=chunk.chapter_title,
                        progress_label=f"Up to {chunk.chapter_title} ({pct}%)",
                    )

        # -------------------------------------------------------------------
        # Tier 2: Chapter Title / Spine Index + Intra-Chapter Offset/Progress
        # -------------------------------------------------------------------
        matched_spine = None
        if chapter_title:
            best_ch_score = 0.0
            for sdoc in book.spine_docs:
                sc = token_similarity(chapter_title, sdoc.chapter_title)
                if sc > best_ch_score:
                    best_ch_score = sc
                    matched_spine = sdoc
            if best_ch_score < 78.0:
                matched_spine = None

        if matched_spine is None and 0 <= spine_index < len(book.spine_docs):
            # If no chapter_title was given, or spine count looks compatible, use spine_index
            if not chapter_title or len(book.spine_docs) > spine_index:
                matched_spine = book.spine_docs[spine_index]

        if matched_spine is not None:
            ch_len = len(matched_spine.visible_text)
            if visible_offset is not None and visible_offset >= 0 and (
                matched_spine.spine_index == spine_index and visible_offset <= ch_len + 200
            ):
                cutoff_char = min(ch_len, visible_offset)
                tier_name = "spine_char_offset"
            elif chapter_progress is not None and 0.0 <= chapter_progress <= 1.0:
                safe_prog = max(0.0, chapter_progress - cls.CHAPTER_SAFETY_BUFFER)
                cutoff_char = int(math.floor(ch_len * safe_prog))
                tier_name = "chapter_progress"
            else:
                cutoff_char = ch_len
                tier_name = "chapter_end"

            global_base = sum(len(s.visible_text) for s in book.spine_docs[: matched_spine.spine_index])
            cutoff_global = global_base + cutoff_char
            pct = int(round(100.0 * cutoff_global / max(1, book.total_chars)))
            return CutoffResolution(
                tier=tier_name,
                cutoff_spine_index=matched_spine.spine_index,
                cutoff_char_offset=cutoff_char,
                cutoff_global_char=cutoff_global,
                matched_chapter_title=matched_spine.chapter_title,
                progress_label=f"Up to {matched_spine.chapter_title} ({pct}%)",
            )

        # -------------------------------------------------------------------
        # Tier 3: Whole-Book Progress Percentage (Conservative -2% Buffer)
        # -------------------------------------------------------------------
        prog = book_progress if (book_progress is not None and 0.0 <= book_progress <= 1.0) else 0.0
        safe_book_prog = max(0.0, prog - cls.BOOK_SAFETY_BUFFER)
        cutoff_global = int(math.floor(book.total_chars * safe_book_prog))

        running = 0
        chosen_spine = book.spine_docs[0] if book.spine_docs else None
        cutoff_char = 0
        for sdoc in book.spine_docs:
            slen = len(sdoc.visible_text)
            if running + slen >= cutoff_global:
                chosen_spine = sdoc
                cutoff_char = cutoff_global - running
                break
            running += slen

        ch_title = chosen_spine.chapter_title if chosen_spine else "Chapter 1"
        s_idx = chosen_spine.spine_index if chosen_spine else 0
        pct = int(round(100.0 * cutoff_global / max(1, book.total_chars)))
        return CutoffResolution(
            tier="book_progress",
            cutoff_spine_index=s_idx,
            cutoff_char_offset=cutoff_char,
            cutoff_global_char=cutoff_global,
            matched_chapter_title=ch_title,
            progress_label=f"Up to {ch_title} ({pct}%)",
        )


def filter_and_truncate_chunks(book: ParsedBook, cutoff: CutoffResolution) -> list[TextChunk]:
    """Return ONLY chunks up to `cutoff`, slicing any straddling chunk at the exact cutoff offset!

    Because overlapping chunks in the current chapter can start before `cutoff`
    and extend past `cutoff`, EVERY chunk in `cutoff_spine_index` whose `char_end`
    exceeds `cutoff_char_offset` has its text sliced cleanly at `cutoff_char_offset`.
    """
    eligible: list[TextChunk] = []
    for chunk in book.chunks:
        if chunk.spine_index > cutoff.cutoff_spine_index:
            continue
        if chunk.spine_index == cutoff.cutoff_spine_index:
            if chunk.char_start > cutoff.cutoff_char_offset:
                continue
            if chunk.char_end > cutoff.cutoff_char_offset:
                keep_len = max(0, cutoff.cutoff_char_offset - chunk.char_start)
                sliced_text = chunk.text[:keep_len].strip()
                if not sliced_text:
                    continue
                eligible.append(
                    TextChunk(
                        canonical_book_id=chunk.canonical_book_id,
                        spine_index=chunk.spine_index,
                        spine_href=chunk.spine_href,
                        chapter_title=chunk.chapter_title,
                        chunk_index=chunk.chunk_index,
                        char_start=chunk.char_start,
                        char_end=chunk.char_start + len(sliced_text),
                        chapter_total_chars=chunk.chapter_total_chars,
                        global_char_start=chunk.global_char_start,
                        global_char_end=chunk.global_char_start + len(sliced_text),
                        book_total_chars=chunk.book_total_chars,
                        text=sliced_text,
                    )
                )
                continue
        eligible.append(chunk)
    return eligible
