# Anti-Spoiler E-Reader AI Companion (`companion-bridge`)

Self-contained backend service for XMarginalia's in-book AI Companion (`Ask AI` on selected words/passages and `Where was I?` / `Recap chapter so far` page actions).

## Key Features
1. **Strict Intra-Chapter Spoiler Gate (`CutoffResolver`)**:
   * **Tier 1 (`anchor_exact`)**: Locates the reader's visible page text (`anchor_text`) inside the indexed chunks and slices any straddling chunk immediately after that sentence—100% immune to Calibre splitting/merging HTML files or re-compressing EPUBs.
   * **Tier 2 (`chapter_progress`)**: Matches `chapter_title` / `spine_index` and scales by `chapter_progress` minus a `3%` safety margin.
   * **Tier 3 (`book_progress`)**: Maps whole-book progress minus a `2%` safety margin against cumulative book codepoints.
2. **Multi-Tier Fuzzy Book Matching (`BookResolver`)**:
   * Cascades from exact `<dc:identifier>` UUID / ISBN / ASIN / `ko_md5` $\rightarrow$ RapidFuzz normalized `title + author` / filename matching ($\ge 82\%$) $\rightarrow$ full-corpus `anchor_text` search when metadata is missing or mangled.
3. **Parametric Memory Bleed Guard (`EntityMasker`)**:
   * Masks proper nouns (`[ENTITY_1]`, `[ENTITY_2]`, ...) in retrieved chunks and the user prompt before calling the LLM and restores them in the response, preventing pre-trained knowledge leaks on famous books.
4. **Dynamic LocalAI Model Discovery (`qwen3-14b` $\leftrightarrow$ `qwen2.5-9b`)**:
   * Queries LocalAI `/v1/models` on each request (`LOCALAI_CHAT_MODEL=auto`) and selects whichever model from `LOCALAI_MODEL_PREFERENCE` you currently have loaded, automatically disabling `<think>` chains on `qwen3*` models.
   * Runs `BAAI/bge-small-en-v1.5` embeddings on CPU RAM (`EMBEDDING_PROVIDER=fastembed_cpu`, ~20ms/query) so LocalAI never unloads your 14B GPU model.

## Quick Start (Adding to Your LocalAI Docker Setup)

1. See [`compose.localai.yaml`](compose.localai.yaml) for the `ereader-companion` service block to drop into your LocalAI `docker-compose.yml`.
2. Configure your gateway using the snippets in [`deploy/`](deploy/):
   * [`deploy/authelia-access-control.snippet.yml`](deploy/authelia-access-control.snippet.yml)
   * [`deploy/Caddyfile.snippet`](deploy/Caddyfile.snippet)
   * [`deploy/localai-qwen3-14b.example.yaml`](deploy/localai-qwen3-14b.example.yaml)
3. Create `/.crosspoint/companion.json` on your e-reader's SD card:
   ```json
   {
     "host": "yourdomain.com",
     "api_key": "your-pre-shared-companion-api-key",
     "enabled": true
   }
   ```

## Running the Test Suite
```bash
python3 tests/test_companion.py
```
