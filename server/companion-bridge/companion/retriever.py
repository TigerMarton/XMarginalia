"""Anti-Spoiler Retrieval Engine and Parametric Memory Bleed Guard.

Combines:
  1. Strict position-gated chunk filtering + straddling-chunk truncation;
  2. Dual-mode retrieval:
     - Local recency window (last N chunks right before the cutoff) for
       `recap_chapter`, `recap_story`, and `explain`;
     - First-mention (character introduction) + semantic/BM25 top-K for
       `who_is` and `what_is`;
  3. Dynamic Entity Aliasing (`EntityMasker`): optionally replaces proper nouns
     in retrieved chunks and the user's query with synthetic placeholders
     (`[ENTITY_1]`, `[ENTITY_2]`, ...) before sending to the LLM, then restores
     the real names in the answer—mathematically preventing the LLM from
     recognizing famous characters (e.g., Paul Atreides, Gollum, Kaladin) and
     leaking pre-trained spoilers!
"""

from __future__ import annotations

from dataclasses import dataclass
import math
import re
import threading
from typing import Callable

from companion.alignment import CutoffResolution, filter_and_truncate_chunks
from companion.epub_indexer import ParsedBook, TextChunk

EmbedFn = Callable[[list[str]], list[list[float]]]


@dataclass
class RetrievalResult:
    cutoff: CutoffResolution
    selected_chunks: list[TextChunk]
    first_mention_chunk: TextChunk | None


def _tokenize_bm25(text: str) -> list[str]:
    return re.findall(r"[a-z0-9']+", text.lower())


def _cosine_sim(a: list[float], b: list[float]) -> float:
    if not a or not b or len(a) != len(b):
        return 0.0
    dot = sum(x * y for x, y in zip(a, b))
    na = math.sqrt(sum(x * x for x in a))
    nb = math.sqrt(sum(y * y for y in b))
    if na == 0.0 or nb == 0.0:
        return 0.0
    return dot / (na * nb)


def _sample_evenly(chunks: list[TextChunk], limit: int) -> list[TextChunk]:
    """Select up to `limit` chunks evenly across `chunks` while preserving order."""
    if limit <= 0 or not chunks:
        return []
    if len(chunks) <= limit:
        return list(chunks)
    if limit == 1:
        return [chunks[0]]
    indices: list[int] = []
    seen: set[int] = set()
    for i in range(limit):
        idx = round(i * (len(chunks) - 1) / (limit - 1))
        if idx not in seen:
            seen.add(idx)
            indices.append(idx)
    return [chunks[i] for i in indices]


def _pick_unique(
    candidates: list[TextChunk],
    chosen: list[TextChunk],
    seen: set[tuple[int, int]],
    cap: int,
) -> None:
    """Append chunks from `candidates` into `chosen` until `len(chosen) >= cap`."""
    for c in candidates:
        if len(chosen) >= cap:
            break
        key = (c.spine_index, c.chunk_index)
        if key not in seen:
            seen.add(key)
            chosen.append(c)


class SpoilerFreeRetriever:
    """Retrieves spoiler-gated chunks using hybrid lexical (BM25) + dense vector similarity."""

    def __init__(self, embed_fn: EmbedFn | None = None) -> None:
        self.embed_fn = embed_fn
        self._chunk_vectors: dict[tuple[str, int, int], list[float]] = {}
        self._vectored: set[str] = set()
        self._vec_lock = threading.Lock()

    def index_book_vectors(self, book: ParsedBook) -> None:
        if self.embed_fn is None or not book.chunks:
            return
        texts = [c.text for c in book.chunks]
        batch_size = 32
        local: dict[tuple[str, int, int], list[float]] = {}
        for i in range(0, len(texts), batch_size):
            batch = texts[i : i + batch_size]
            vecs = self.embed_fn(batch)
            for chunk, vec in zip(book.chunks[i : i + batch_size], vecs):
                local[(book.canonical_book_id, chunk.spine_index, chunk.chunk_index)] = vec
        # Publish only once the whole book is embedded so ranking never mixes
        # a half-vectored book.
        with self._vec_lock:
            self._chunk_vectors.update(local)
            self._vectored.add(book.canonical_book_id)

    def has_vectors(self, book_id: str) -> bool:
        with self._vec_lock:
            return book_id in self._vectored

    def drop_book_vectors(self, book_id: str) -> None:
        with self._vec_lock:
            self._vectored.discard(book_id)
            for k in [k for k in self._chunk_vectors if k[0] == book_id]:
                del self._chunk_vectors[k]

    def retrieve(
        self,
        book: ParsedBook,
        cutoff: CutoffResolution,
        action_type: str,
        selected_text: str = "",
        anchor_text: str = "",
        top_k: int = 8,
    ) -> RetrievalResult:
        eligible = filter_and_truncate_chunks(book, cutoff)
        if not eligible:
            return RetrievalResult(cutoff=cutoff, selected_chunks=[], first_mention_chunk=None)

        action = (action_type or "who_is").strip().lower()

        # Recap current chapter: sample across the entire current chapter up to the cutoff
        # (opening + middle progression + trailing chunks at the reader's current page).
        if action == "recap_chapter":
            in_ch = [c for c in eligible if c.spine_index == cutoff.cutoff_spine_index]
            pool = in_ch if in_ch else eligible
            cap = max(top_k, 10)
            if len(pool) <= cap:
                chosen = list(pool)
            else:
                tail = pool[-3:]
                head_pool = pool[:-3]
                chosen = _sample_evenly(head_pool, cap - len(tail)) + tail
            return RetrievalResult(cutoff=cutoff, selected_chunks=chosen, first_mention_chunk=None)

        # Recap story so far: sample across earlier chapters + recent progression + current page.
        if action == "recap_story":
            cap = max(top_k, 12)
            if len(eligible) <= cap:
                chosen = list(eligible)
            else:
                tail = eligible[-3:]
                earlier = eligible[:-3]
                # Pick one representative chunk per spine chapter first, then fill evenly if needed.
                by_spine: dict[int, list[TextChunk]] = {}
                for c in earlier:
                    by_spine.setdefault(c.spine_index, []).append(c)
                spine_reps = [grp[len(grp) // 2] for _sp, grp in sorted(by_spine.items())]
                sampled_earlier = _sample_evenly(spine_reps, cap - len(tail))
                if len(sampled_earlier) < cap - len(tail):
                    sampled_earlier = _sample_evenly(earlier, cap - len(tail))
                chosen = sampled_earlier + tail
            chosen.sort(key=lambda c: (c.spine_index, c.chunk_index))
            return RetrievalResult(cutoff=cutoff, selected_chunks=chosen, first_mention_chunk=None)

        if action == "explain":
            # Immediate local scene (last 3 chunks) + top semantic/lexical matches from earlier context
            cap = max(top_k, 8)
            local_tail = eligible[-3:]
            remaining = eligible[:-3]
            ranked = self._rank_chunks(book.canonical_book_id, remaining, f"{selected_text} {anchor_text}".strip())
            chosen: list[TextChunk] = []
            seen: set[tuple[int, int]] = set()
            _pick_unique(local_tail, chosen, seen, cap)
            _pick_unique(ranked, chosen, seen, cap)
            chosen.sort(key=lambda c: (c.spine_index, c.chunk_index))
            return RetrievalResult(cutoff=cutoff, selected_chunks=chosen, first_mention_chunk=None)

        # Entity / concept lookup (`who_is`, `what_is`):
        # Combine: (1) first mention (introduction), (2) most recent mentions of target
        # (active role right now), (3) immediate current-page chunk, and (4) top ranked matches.
        cap = max(top_k, 8)
        first_mention: TextChunk | None = None
        recent_mentions: list[TextChunk] = []
        target = selected_text.strip()
        if target:
            pattern = re.compile(r"\b" + re.escape(target) + r"\b", flags=re.IGNORECASE)
            matching = [c for c in eligible if pattern.search(c.text)]
            if matching:
                first_mention = matching[0]
                recent_mentions = list(reversed(matching[-3:]))

        query_str = f"{selected_text} {anchor_text}".strip() or selected_text or anchor_text
        ranked = self._rank_chunks(book.canonical_book_id, eligible, query_str, focus_term=target)

        chosen_list: list[TextChunk] = []
        seen_keys: set[tuple[int, int]] = set()

        if first_mention is not None:
            _pick_unique([first_mention], chosen_list, seen_keys, cap)
        # Always include the immediate current-page chunk so the context of the tap is present
        _pick_unique([eligible[-1]], chosen_list, seen_keys, cap)
        # Include recent mentions of the entity to capture their active role up to this page
        _pick_unique(recent_mentions, chosen_list, seen_keys, min(cap, len(chosen_list) + 2))
        # Fill remaining slots with top semantic + BM25 matches
        _pick_unique(ranked, chosen_list, seen_keys, cap)

        # Sort chronologically so the LLM sees the character's progression in story order
        chosen_list.sort(key=lambda c: (c.spine_index, c.chunk_index))
        return RetrievalResult(cutoff=cutoff, selected_chunks=chosen_list, first_mention_chunk=first_mention)

    def _rank_chunks(
        self,
        book_id: str,
        chunks: list[TextChunk],
        query: str,
        focus_term: str = "",
    ) -> list[TextChunk]:
        if not chunks:
            return []
        q_tokens = _tokenize_bm25(query)
        focus_tokens = _tokenize_bm25(focus_term) if focus_term else []

        # Compute IDF across eligible chunks
        n_docs = len(chunks)
        doc_Tokens = [_tokenize_bm25(c.text) for c in chunks]
        avg_dl = sum(len(t) for t in doc_Tokens) / max(1, n_docs)
        df: dict[str, int] = {}
        for toks in doc_Tokens:
            for w in set(toks):
                df[w] = df.get(w, 0) + 1

        q_vec: list[float] | None = None
        if self.embed_fn is not None and query:
            try:
                q_vec = self.embed_fn([query])[0]
            except Exception:
                q_vec = None

        scored: list[tuple[float, TextChunk]] = []
        k1 = 1.5
        b = 0.75
        for idx, (chunk, toks) in enumerate(zip(chunks, doc_Tokens)):
            tf_map: dict[str, int] = {}
            for w in toks:
                tf_map[w] = tf_map.get(w, 0) + 1
            dl = max(1, len(toks))

            bm25 = 0.0
            for qt in q_tokens:
                f = tf_map.get(qt, 0)
                if f == 0:
                    continue
                n_q = df.get(qt, 0)
                idf = math.log(1.0 + (n_docs - n_q + 0.5) / (n_q + 0.5))
                bm25 += idf * (f * (k1 + 1.0)) / (f + k1 * (1.0 - b + b * dl / avg_dl))

            # Boost exact focus_term matches
            if focus_tokens:
                focus_hits = sum(1 for ft in focus_tokens if tf_map.get(ft, 0) > 0)
                if focus_hits == len(focus_tokens):
                    bm25 += 4.0

            vec_score = 0.0
            if q_vec is not None:
                with self._vec_lock:
                    c_vec = self._chunk_vectors.get((book_id, chunk.spine_index, chunk.chunk_index))
                if c_vec is not None:
                    vec_score = _cosine_sim(q_vec, c_vec) * 5.0

            # Slight recency tie-breaker (more recent eligible chunks score marginally higher)
            recency = 0.25 * (idx / max(1, n_docs))
            scored.append((bm25 + vec_score + recency, chunk))

        scored.sort(key=lambda pair: pair[0], reverse=True)
        return [c for _s, c in scored]


class EntityMasker:
    """Masks proper nouns in retrieved chunks & query to prevent LLM parametric memory bleed."""

    STOP_CAPITALS = frozenset(
        {
            "The",
            "And",
            "But",
            "For",
            "Nor",
            "Or",
            "Yet",
            "So",
            "In",
            "On",
            "At",
            "To",
            "From",
            "By",
            "With",
            "As",
            "Into",
            "Like",
            "Through",
            "After",
            "Over",
            "Between",
            "Out",
            "Against",
            "During",
            "Without",
            "Before",
            "Under",
            "Around",
            "Among",
            "He",
            "She",
            "They",
            "We",
            "You",
            "It",
            "His",
            "Her",
            "Their",
            "Our",
            "Your",
            "Its",
            "Who",
            "What",
            "Where",
            "When",
            "Why",
            "How",
            "This",
            "That",
            "These",
            "Those",
            "There",
            "Here",
            "Then",
            "Now",
            "Chapter",
            "Part",
            "Book",
            "Prologue",
            "Epilogue",
            "One",
            "Two",
            "Three",
        }
    )

    def __init__(self, focus_entity: str = "", extra_entities: list[str] | None = None) -> None:
        self.forward_map: dict[str, str] = {}
        self.reverse_map: dict[str, str] = {}
        self._counter = 0

        if focus_entity and len(focus_entity.strip()) >= 2:
            self._register(focus_entity.strip())
        for ent in extra_entities or []:
            if ent and len(ent.strip()) >= 2:
                self._register(ent.strip())

    def _register(self, name: str) -> str:
        clean = " ".join(name.split())
        low = clean.lower()
        for existing, token in self.forward_map.items():
            if existing.lower() == low:
                return token
        self._counter += 1
        token = f"ENTITY_{self._counter}"
        self.forward_map[clean] = token
        self.reverse_map[token] = clean
        # If multi-word name (e.g., "Paul Atreides"), also register individual capitalized parts
        parts = [p for p in re.findall(r"[A-Z][a-z]{2,}", clean) if p not in self.STOP_CAPITALS]
        if len(parts) > 1:
            for p in parts:
                if p.lower() not in {k.lower() for k in self.forward_map}:
                    self._counter += 1
                    ptok = f"ENTITY_{self._counter}"
                    self.forward_map[p] = ptok
                    self.reverse_map[ptok] = p
        return token

    def learn_from_texts(self, texts: list[str], max_entities: int = 16) -> None:
        """Auto-detect recurring capitalized proper nouns in the retrieved excerpts."""
        counts: dict[str, int] = {}
        for t in texts:
            for m in re.finditer(r"\b([A-Z][a-z]{2,}(?:\s+[A-Z][a-z]{2,})?)\b", t):
                cand = m.group(1)
                if cand in self.STOP_CAPITALS:
                    continue
                first_w = cand.split()[0]
                if first_w in self.STOP_CAPITALS:
                    cand = " ".join(cand.split()[1:])
                if len(cand) >= 3 and cand not in self.STOP_CAPITALS:
                    counts[cand] = counts.get(cand, 0) + 1

        # Mask proper nouns that appear at least twice (or once if multi-word)
        sorted_cands = sorted(counts.items(), key=lambda kv: (kv[1], len(kv[0])), reverse=True)
        for cand, cnt in sorted_cands:
            if len(self.forward_map) >= max_entities:
                break
            if cnt >= 2 or " " in cand:
                self._register(cand)

    def mask_text(self, text: str) -> str:
        if not text or not self.forward_map:
            return text
        out = text
        # Replace longest names first so "Paul Atreides" is replaced before "Paul"
        for name in sorted(self.forward_map.keys(), key=len, reverse=True):
            token = self.forward_map[name]
            out = re.sub(r"\b" + re.escape(name) + r"\b", token, out, flags=re.IGNORECASE)
        return out

    def unmask_text(self, text: str) -> str:
        if not text or not self.reverse_map:
            return text
        out = text
        for token in sorted(self.reverse_map.keys(), key=len, reverse=True):
            real_name = self.reverse_map[token]
            out = re.sub(r"\[?\b" + re.escape(token) + r"\b\]?", real_name, out, flags=re.IGNORECASE)
        return out
