# Anti-Spoiler E-Reader AI Companion (`companion-bridge`)

Self-contained backend service for XMarginalia's in-book AI Companion (`Who is this?`, `What is this?`, `Explain` on selected text, and `Where was I?` / `Recap chapter so far` in the reader menu). See [docs/companion.md](../../docs/companion.md) for the full architecture and setup guide.

## Key Features
1. **Strict Intra-Chapter Spoiler Gate (`CutoffResolver`)**:
   * **Tier 1 (`anchor_exact`)**: Locates the reader's visible page text (`anchor_text`) inside the indexed chunks and slices any straddling chunk immediately after that passage—immune to Calibre splitting/merging HTML files or re-compressing EPUBs.
   * **Tier 2 (`chapter_progress`)**: Matches `chapter_title` / `spine_index` and scales by `chapter_progress` minus a `3%` safety margin.
   * **Tier 3 (`book_progress`)**: Maps whole-book progress minus a `2%` safety margin against cumulative book codepoints.
2. **Multi-Tier Fuzzy Book Matching (`BookResolver`)**:
   * Cascades from exact `<dc:identifier>` UUID / ISBN / ASIN / `ko_md5` -> RapidFuzz normalized `title + author` / filename matching (`>= 82%`) -> full-corpus `anchor_text` search when metadata is missing or mangled.
3. **Book-Focused Retrieval & Memory Bleed Guard (`SpoilerFreeRetriever`, `EntityMasker`)**:
   * `who_is` / `what_is` combine first mention, recent mentions before the cutoff, current-page context, and hybrid BM25 + dense vector matches; `recap_chapter` and `recap_story` sample across the chapter or earlier spine chapters up to the cutoff.
   * Optional proper-noun masking (`[ENTITY_1]`, `[ENTITY_2]`, ...) prevents pre-trained knowledge leaks on famous books.
4. **Dynamic OpenAI-Compatible / LocalAI Model Discovery**:
   * Queries `/v1/models` on each request (`LOCALAI_CHAT_MODEL=auto`) and selects whichever model from `LOCALAI_MODEL_PREFERENCE` is loaded, automatically disabling `<think>` chains on `qwen3*` models.
   * Runs `BAAI/bge-small-en-v1.5` embeddings on CPU RAM (`EMBEDDING_PROVIDER=fastembed_cpu`) or pure BM25 (`EMBEDDING_PROVIDER=none`).

## Quick Start (Docker / Caddy)

1. Run `./scripts/generate-deploy-config.sh` or add the `ereader-companion` service to your `docker-compose.yml` alongside an OpenAI-compatible backend (e.g., LocalAI) and your read-only books mount (`ABS_BOOKS_MOUNT=/abs_books`).
2. *(Optional)* If using **Caddy** as a reverse proxy (for TLS or alongside an SSO gateway like Authelia), see [`deploy/Caddyfile.snippet`](deploy/Caddyfile.snippet) to route `/api/ereader/*` via `X-API-Key` before interactive `forward_auth`.
3. Create `/.crosspoint/companion.json` on your e-reader's SD card:
   ```json
   {
     "host": "companion.example.com",
     "api_key": "your-pre-shared-companion-api-key",
     "enabled": true
   }
   ```
   *(For plain HTTP on a trusted LAN, add `"use_http": true` and include the port, e.g., `"192.168.1.50:8095"`.)*

> **Security Warning**: Exposing `companion-bridge` externally to the public internet is risky—even with an `X-API-Key`, it increases attack surface on your Python/LLM stack and risks unauthorized inference or library metadata exposure. Prefer keeping it on your LAN or behind a private VPN (WireGuard/Tailscale), and if exposed through Caddy, always enforce HTTPS, a strong random API key, and firewall/IP restrictions.

## Running the Test Suite
```bash
python3 -m pytest -q tests
```
