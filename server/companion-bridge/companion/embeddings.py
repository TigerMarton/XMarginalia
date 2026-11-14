"""Embedding provider abstraction for `companion-bridge`.

Supports:
  * `fastembed_cpu` (default in Docker): runs `BAAI/bge-small-en-v1.5` via ONNX
    on CPU RAM (~20ms per query, 0 MB VRAM) so LocalAI never unloads a large
    GPU chat model like `qwen3-14b`.
  * `localai`: calls LocalAI's `/v1/embeddings` HTTP endpoint.
  * `none`: disables dense vectors and uses pure BM25 + recency + first-mention
    retrieval (zero dependencies, ideal for lightweight host unit tests).
"""

from __future__ import annotations

import json
from typing import Callable
import urllib.request

EmbedFn = Callable[[list[str]], list[list[float]]]


def build_embedder(
    provider: str = "fastembed_cpu",
    model_name: str = "BAAI/bge-small-en-v1.5",
    localai_base_url: str = "http://127.0.0.1:8080/v1",
    api_key: str = "localai",
) -> EmbedFn | None:
    prov = (provider or "none").strip().lower()
    if prov in ("none", "bm25", "disabled", ""):
        return None

    if prov == "fastembed_cpu":
        try:
            import os

            from fastembed import TextEmbedding

            cache_dir = os.environ.get("FASTEMBED_CACHE_PATH", "/data/fastembed_cache")
            emb_model = TextEmbedding(model_name=model_name, cache_dir=cache_dir)

            def _embed_fast(texts: list[str]) -> list[list[float]]:
                return [vec.tolist() for vec in emb_model.embed(texts)]

            return _embed_fast
        except Exception as exc:
            import sys

            print(f"[companion] FastEmbed init failed ({exc}); falling back to BM25 retrieval.", file=sys.stderr)
            return None

    if prov in ("localai", "openai"):
        base = localai_base_url.rstrip("/")

        def _embed_http(texts: list[str]) -> list[list[float]]:
            payload = json.dumps({"model": model_name, "input": texts}).encode("utf-8")
            req = urllib.request.Request(
                f"{base}/embeddings",
                data=payload,
                headers={
                    "Content-Type": "application/json",
                    "Authorization": f"Bearer {api_key}",
                },
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=15.0) as resp:
                data = json.loads(resp.read().decode("utf-8"))
            items = sorted(data.get("data") or [], key=lambda x: x.get("index", 0))
            return [list(item.get("embedding") or []) for item in items]

        return _embed_http

    return None
