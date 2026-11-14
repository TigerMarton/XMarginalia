"""LocalAI / OpenAI-compatible LLM Client with Dynamic Model Discovery.

Features:
  * Dynamic Active-Model Auto-Discovery (`LOCALAI_CHAT_MODEL=auto`): queries
    `/v1/models` on LocalAI and picks the first available model from
    `LOCALAI_MODEL_PREFERENCE` (e.g. `qwen3-14b,qwen2.5-9b-instruct,qwen3-8b`),
    or falls back to the first loaded chat model—allowing the user to swap
    between `qwen3-14b` and `9b` without restarting or reconfiguring!
  * Automatic `qwen3` `<think>` suppression (`chat_template_kwargs: {"enable_thinking": false}`
    + `/no_think` + regex stripping of `<think>...</think>` blocks).
  * Anti-Spoiler Prompt Framing + Optional Dynamic Entity Masking to eliminate
    parametric memory bleed on famous books.
  * Strict 480-character e-ink output formatter (strips markdown, truncates on
    a clean sentence boundary).
"""

from __future__ import annotations

import json
import re
import urllib.error
import urllib.request

from companion.epub_indexer import TextChunk
from companion.retriever import EntityMasker

MAX_ANSWER_CHARS = 1400

SYSTEM_PROMPT = """You are a spoiler-free reading companion embedded in an e-reader, helping the reader understand the book they are currently reading.
CRITICAL RULES:
1. STRICT CUTOFF GROUNDING: Rely ONLY on facts explicitly established in the provided [EXCERPT] blocks (which stop at the reader's exact current page). Never pull in outside knowledge about the book's future plot, twists, or character fates.
2. NO SPOILERS OR SPECULATION: Never guess, foreshadow, or hint at anything beyond the final excerpt.
3. E-INK FORMAT: Write clear, natural plain prose (NO markdown, NO asterisks, NO bullet points, NO headers). Separate distinct parts with a blank line when helpful. Keep answers concise and complete (under 1100 characters).
4. IF NOT YET REVEALED: If the provided excerpts do not contain enough information to answer, reply with: "Not yet revealed up to this point in the book."
"""


def is_reasoning_model(model_name: str) -> bool:
    low = (model_name or "").lower()
    return "qwen3" in low or "deepseek-r1" in low or "qwq" in low or "reasoning" in low


def strip_thinking_and_markdown(raw_text: str) -> str:
    """Remove `<think>...</think>` blocks and Markdown formatting while preserving paragraph breaks."""
    if not raw_text:
        return ""
    # Strip complete <think>...</think> blocks
    s = re.sub(r"<think>.*?</think>", " ", raw_text, flags=re.DOTALL | re.IGNORECASE)
    # If an unclosed <think> tag consumed the start of the output, remove it
    if "<think>" in s.lower():
        s = re.sub(r"<think>.*$", " ", s, flags=re.DOTALL | re.IGNORECASE)
    # Strip markdown bold/italics/code/headers
    s = re.sub(r"[*_`#]+", "", s)
    s = re.sub(r"^\s*[-•]\s+", "", s, flags=re.MULTILINE)
    # Normalize whitespace within each paragraph while preserving intentional paragraph breaks.
    s = s.replace("\r\n", "\n").replace("\r", "\n")
    paragraphs = [" ".join(part.split()) for part in re.split(r"\n\s*\n", s)]
    return "\n\n".join(p for p in paragraphs if p).strip()


def enforce_eink_length(text: str, max_chars: int = MAX_ANSWER_CHARS) -> str:
    """Ensure the response fits comfortably inside the e-reader's static bounding box."""
    clean = strip_thinking_and_markdown(text)
    if len(clean) <= max_chars:
        return clean
    window = clean[:max_chars]
    # Try to cut at the last sentence-ending punctuation mark (. ! ?)
    last_sent = -1
    for m in re.finditer(r'[.!?](?:["\'])?(?:\s|$)', window):
        if m.end() >= max_chars // 3:
            last_sent = m.end()
    if last_sent != -1:
        return window[:last_sent].strip()
    # Fall back to last word boundary + ellipsis
    last_space = window.rfind(" ", 0, max_chars - 3)
    if last_space > max_chars // 2:
        return window[:last_space].rstrip(" ,;:-") + "..."
    return window[: max_chars - 3] + "..."


def build_user_prompt(
    action_type: str,
    excerpts: list[TextChunk],
    selected_text: str = "",
    anchor_text: str = "",
    masker: EntityMasker | None = None,
    disable_think_suffix: bool = False,
    book_title: str = "",
    book_author: str = "",
    progress_label: str = "",
) -> str:
    blocks: list[str] = []
    for i, c in enumerate(excerpts, start=1):
        txt = masker.mask_text(c.text) if masker else c.text
        blocks.append(f"[EXCERPT {i} - {c.chapter_title}]\n{txt}")
    context_str = "\n\n".join(blocks) if blocks else "[NO EXCERPTS FOUND BEFORE CURRENT PAGE]"

    sel = masker.mask_text(selected_text) if (masker and selected_text) else selected_text
    anc = masker.mask_text(anchor_text) if (masker and anchor_text) else anchor_text

    meta_parts: list[str] = []
    if book_title:
        masked_title = masker.mask_text(book_title) if masker else book_title
        meta_parts.append(f"Book: {masked_title}")
    if book_author:
        masked_author = masker.mask_text(book_author) if masker else book_author
        meta_parts.append(f"Author: {masked_author}")
    if progress_label:
        meta_parts.append(f"Reader Position: {progress_label}")
    header = (" | ".join(meta_parts) + "\n\n") if meta_parts else ""

    action = (action_type or "who_is").strip().lower()
    if action == "who_is":
        task = (
            f"Based ONLY on the excerpts above, explain who '{sel}' is in this book. "
            "Write two brief paragraphs: first, a concise summary of who the character is and how they were introduced; "
            "second, their active role and current situation in the story up to the reader's current position."
        )
    elif action == "what_is":
        task = (
            f"Based ONLY on the excerpts above, explain what '{sel}' is in this book. "
            "Write two brief paragraphs: first, what it is and how it was introduced; "
            "second, why it matters at the reader's current point in the story."
        )
    elif action == "explain":
        target = sel or anc
        task = (
            f"Using ONLY the excerpts above, explain what is happening in this passage: \"{target}\". "
            "Write two brief paragraphs: first, clarify the immediate moment on the page; "
            "second, connect it to the relevant earlier events or motives revealed so far."
        )
    elif action == "recap_chapter":
        task = (
            "Summarize the events of the current chapter chronologically from its opening up to the final excerpt "
            "at the reader's current position, in 1-2 concise paragraphs."
        )
    elif action == "recap_story":
        task = (
            "Recap the story so far up to the final excerpt so the reader can pick the book back up. "
            "Write two concise paragraphs: first, the main plot arcs and key events established so far; "
            "second, where things stand right now at the reader's current position."
        )
    else:
        task = f"Explain '{sel or anc}' in the context of this book using ONLY the excerpts above."

    suffix = " /no_think" if disable_think_suffix else ""
    return f"{header}{context_str}\n\nQUESTION: {task}{suffix}"


class LocalAIClient:
    """OpenAI-compatible client tailored for LocalAI (supporting Qwen 9B, Qwen3-14B, etc.)."""

    EMBEDDING_IDENTIFIERS = ("embed", "bge-", "nomic-", "minilm", "e5-", "gte-")

    def __init__(
        self,
        base_url: str = "http://127.0.0.1:8080/v1",
        chat_model: str = "auto",
        model_preference: list[str] | None = None,
        api_key: str = "localai",
        timeout_s: float = 45.0,
        enable_entity_masking: bool = True,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.chat_model = chat_model.strip() or "auto"
        self.model_preference = [m.strip() for m in (model_preference or ["qwen3-14b", "qwen2.5-9b-instruct", "qwen3-8b"]) if m.strip()]
        self.api_key = api_key
        self.timeout_s = timeout_s
        self.enable_entity_masking = enable_entity_masking

    @classmethod
    def _is_embedding_model(cls, model_id: str) -> bool:
        low = model_id.lower()
        return any(tok in low for tok in cls.EMBEDDING_IDENTIFIERS)

    def list_available_models(self) -> list[str]:
        """Query LocalAI `/v1/models` and return non-embedding model IDs."""
        url = f"{self.base_url}/models"
        req = urllib.request.Request(
            url,
            headers={"Authorization": f"Bearer {self.api_key}", "Accept": "application/json"},
            method="GET",
        )
        try:
            with urllib.request.urlopen(req, timeout=5.0) as resp:
                data = json.loads(resp.read().decode("utf-8"))
                models = [
                    str(item.get("id", ""))
                    for item in data.get("data", [])
                    if isinstance(item, dict) and item.get("id")
                ]
                return [m for m in models if not self._is_embedding_model(m)]
        except Exception:
            return []

    def resolve_active_model(self) -> str:
        """Select the active chat model in LocalAI without triggering unnecessary model reloads."""
        if self.chat_model.lower() != "auto":
            return self.chat_model

        available = self.list_available_models()
        if not available:
            return self.model_preference[0] if self.model_preference else "qwen3-14b"

        # Match against user's preference order (case-insensitive substring or exact match)
        avail_lower = {m.lower(): m for m in available}
        for pref in self.model_preference:
            pl = pref.lower()
            if pl in avail_lower:
                return avail_lower[pl]
            for m in available:
                if pl in m.lower():
                    return m

        return available[0]

    def generate_companion_answer(
        self,
        action_type: str,
        excerpts: list[TextChunk],
        selected_text: str = "",
        anchor_text: str = "",
        book_title: str = "",
        book_author: str = "",
        progress_label: str = "",
    ) -> tuple[str, str]:
        """Return `(clean_answer, model_used)`."""
        if not excerpts:
            return "Not yet revealed up to this point in the book.", "none"

        model = self.resolve_active_model()
        reasoning = is_reasoning_model(model)

        masker: EntityMasker | None = None
        if self.enable_entity_masking and action_type in ("who_is", "what_is", "explain"):
            masker = EntityMasker(
                focus_entity=selected_text,
                extra_entities=[ w for w in (book_title, book_author) if w ],
            )
            masker.learn_from_texts([c.text for c in excerpts])

        user_prompt = build_user_prompt(
            action_type=action_type,
            excerpts=excerpts,
            selected_text=selected_text,
            anchor_text=anchor_text,
            masker=masker,
            disable_think_suffix=reasoning,
            book_title=book_title,
            book_author=book_author,
            progress_label=progress_label,
        )

        payload: dict[str, object] = {
            "model": model,
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": user_prompt},
            ],
            "temperature": 0.1,
            "top_p": 0.85,
            "max_tokens": 360,
        }
        if reasoning:
            # Supported by LocalAI / llama.cpp / vLLM Qwen3 templates to disable <think>
            payload["chat_template_kwargs"] = {"enable_thinking": False}

        raw_reply = self._post_chat_completions(payload)
        unmasked = masker.unmask_text(raw_reply) if masker else raw_reply
        final_answer = enforce_eink_length(unmasked, max_chars=MAX_ANSWER_CHARS)
        if not final_answer:
            final_answer = "Not yet revealed up to this point in the book."
        return final_answer, model

    def _post_chat_completions(self, payload: dict[str, object]) -> str:
        url = f"{self.base_url}/chat/completions"
        body = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            url,
            data=body,
            headers={
                "Content-Type": "application/json",
                "Authorization": f"Bearer {self.api_key}",
            },
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=self.timeout_s) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        choices = data.get("choices") or []
        if not choices:
            return ""
        msg = choices[0].get("message") or {}
        return str(msg.get("content") or "")
