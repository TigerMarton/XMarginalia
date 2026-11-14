#!/bin/bash
# Interactive Configuration & Deployment Snippet Generator for `companion-bridge`.
#
# READ-ONLY BY DESIGN:
#   This wizard NEVER modifies your existing `.env`, `docker-compose.yml`,
#   `Caddyfile`, or `configuration.yml` files. Instead, it prompts you for your
#   server and stack parameters (with sensible defaults) and prints tailored,
#   copy/paste-ready blocks for:
#     1. `rsync` command (with exclusions) to copy `server/companion-bridge/` to your server
#     2. `.env` variables for your LocalAI / Docker Compose directory
#     3. `docker-compose.yml` service block (`ereader-companion`)
#     4. `Caddyfile` route block (`/api/ereader/*` X-API-Key gate before Authelia)
#     5. Authelia `configuration.yml` bypass rule for `/api/ereader/*`
#     6. Cloudflare Tunnel (`cloudflared`) public hostname notes (if applicable)
#     7. Device SD card `/.crosspoint/companion.json` file contents
#     8. End-to-end `curl` verification commands
#
# Usage:
#   ./server/companion-bridge/scripts/generate-deploy-config.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
BRIDGE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BOLD="\033[1m"
CYAN="\033[1;36m"
GREEN="\033[1;32m"
YELLOW="\033[1;33m"
RESET="\033[0m"

prompt_val() {
  local var_name="$1"
  local prompt_text="$2"
  local default_val="$3"
  local input_val=""
  if [ -n "$default_val" ]; then
    read -r -p "$(printf "${BOLD}%s${RESET} [${CYAN}%s${RESET}]: " "$prompt_text" "$default_val")" input_val
    printf -v "$var_name" '%s' "${input_val:-$default_val}"
  else
    read -r -p "$(printf "${BOLD}%s${RESET}: " "$prompt_text")" input_val
    printf -v "$var_name" '%s' "$input_val"
  fi
}

generate_secret() {
  if command -v openssl >/dev/null 2>&1; then
    openssl rand -hex 32
  elif command -v python3 >/dev/null 2>&1; then
    python3 -c "import secrets; print(secrets.token_hex(32))"
  else
    head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n'
  fi
}

echo -e "${GREEN}======================================================================${RESET}"
echo -e "${GREEN}  XMarginalia Anti-Spoiler AI Companion — Deployment Snippet Wizard   ${RESET}"
echo -e "${GREEN}  (Read-only: prints copy/paste blocks without modifying your files)  ${RESET}"
echo -e "${GREEN}======================================================================${RESET}"
echo ""

echo -e "${YELLOW}--- 1. Remote Server & SSH / Rsync Target ---${RESET}"
prompt_val SSH_USER "Remote SSH username" "${USER:-your-user}"
prompt_val SSH_HOST "Remote server hostname or IP (e.g. homelab.local or 192.168.1.50)" "your-server.local"
prompt_val SSH_PORT "Remote SSH port" "22"
prompt_val REMOTE_STACK_DIR "Remote directory where your LocalAI docker-compose.yml lives" "/opt/localai"
REMOTE_BRIDGE_DIR="${REMOTE_STACK_DIR%/}/companion-bridge"
prompt_val REMOTE_BRIDGE_DIR "Remote destination directory for companion-bridge" "$REMOTE_BRIDGE_DIR"
echo ""

echo -e "${YELLOW}--- 2. Public Domain & Gateway (Caddy / Authelia / Cloudflared) ---${RESET}"
prompt_val PUBLIC_DOMAIN "Public domain name routed by Caddy/Cloudflared (no https://)" "companion.yourdomain.com"
DEFAULT_API_KEY="$(generate_secret)"
prompt_val COMPANION_API_KEY "Pre-shared X-API-Key (Enter to use newly generated 64-char hex secret)" "$DEFAULT_API_KEY"
echo ""

echo -e "${YELLOW}--- 3. LocalAI Service & Model Preferences ---${RESET}"
prompt_val LOCALAI_BASE_URL "LocalAI internal Docker URL (from inside Docker network)" "http://localai:8080/v1"
prompt_val LOCALAI_MODEL_PREFERENCE "Comma-separated model preference order (auto-detected via /v1/models)" "qwen3-14b,qwen2.5-9b-instruct,qwen3-8b"
prompt_val DOCKER_NETWORK "Existing Docker network name shared with LocalAI & Caddy (leave blank if in same compose file)" ""
echo ""

echo -e "${YELLOW}--- 4. Audiobookshelf / Book Library Path ---${RESET}"
prompt_val ABS_HOST_BOOKS_PATH "Host path to your EPUB/Audiobookshelf books folder on the server" "/srv/audiobookshelf/books"
prompt_val ABS_BASE_URL "Optional Audiobookshelf URL for live reading-progress floor (leave blank if mount-only)" ""
ABS_API_TOKEN=""
if [ -n "$ABS_BASE_URL" ]; then
  prompt_val ABS_API_TOKEN "Audiobookshelf API Token" ""
fi
echo ""

SSH_FLAG=""
if [ "$SSH_PORT" != "22" ]; then
  SSH_FLAG="-e 'ssh -p ${SSH_PORT}' "
fi

cat <<EOF

================================================================================
STEP 1: Copy \`server/companion-bridge/\` to ${SSH_USER}@${SSH_HOST} via rsync
================================================================================
Run this command from your local workstation:

rsync -avz --progress ${SSH_FLAG}\\
  --exclude '__pycache__/' \\
  --exclude '*.pyc' \\
  --exclude '.pytest_cache/' \\
  --exclude '.mypy_cache/' \\
  --exclude '.ruff_cache/' \\
  --exclude '.venv/' \\
  --exclude 'venv/' \\
  --exclude 'data/' \\
  --exclude '*.db' \\
  --exclude '*.sqlite*' \\
  --exclude '.env' \\
  --exclude '*.env' \\
  --exclude '.DS_Store' \\
  "${BRIDGE_DIR}/" \\
  "${SSH_USER}@${SSH_HOST}:${REMOTE_BRIDGE_DIR}/"

# Next, on ${SSH_USER}@${SSH_HOST}, create the persistent data directory owned by UID 10004:
ssh -p ${SSH_PORT} "${SSH_USER}@${SSH_HOST}" "mkdir -p '${REMOTE_BRIDGE_DIR}/data' && sudo chown -R 10004:10004 '${REMOTE_BRIDGE_DIR}/data'"

================================================================================
STEP 2: Create a dedicated \`marginalia.env\` file on your Server
        (at ${REMOTE_STACK_DIR}/marginalia.env — keeps your main .env untouched!)
================================================================================
# --- XMarginalia Anti-Spoiler AI Companion (marginalia.env) ---
COMPANION_API_KEY=${COMPANION_API_KEY}
LOCALAI_BASE_URL=${LOCALAI_BASE_URL}
LOCALAI_CHAT_MODEL=auto
LOCALAI_MODEL_PREFERENCE=${LOCALAI_MODEL_PREFERENCE}
EMBEDDING_PROVIDER=fastembed_cpu
EMBEDDING_MODEL=BAAI/bge-small-en-v1.5
COMPANION_ENTITY_MASKING=1
ABS_BOOKS_MOUNT=/abs_books
ABS_BASE_URL=${ABS_BASE_URL}
ABS_API_TOKEN=${ABS_API_TOKEN}
HOME=/data
FASTEMBED_CACHE_PATH=/data/fastembed_cache

================================================================================
STEP 3: Add this service to your \`docker-compose.yml\`
        (in ${REMOTE_STACK_DIR}/docker-compose.yml)
================================================================================
  ereader-companion:
    build:
      context: ./companion-bridge
    image: ereader-companion:local
    pull_policy: build
    container_name: ereader-companion
    restart: unless-stopped
    expose:
      - "8095"
    ports:
      - "127.0.0.1:8095:8095"
    env_file:
      - ./marginalia.env
    volumes:
      - ./companion-bridge/data:/data
      - ${ABS_HOST_BOOKS_PATH}:/abs_books:ro
    user: "10004:10004"
    cap_drop: [ALL]
    security_opt:
      - no-new-privileges:true
    mem_limit: 2g
    pids_limit: 128
    healthcheck:
      test: ["CMD", "python", "-c", "import urllib.request;urllib.request.urlopen('http://127.0.0.1:8095/healthz').read()"]
      interval: 60s
      timeout: 10s
      retries: 3
EOF

if [ -n "$DOCKER_NETWORK" ]; then
cat <<EOF
    networks:
      - ${DOCKER_NETWORK}
EOF
fi

cat <<EOF

# Note: If your \`caddy\` container is in \`docker-compose.yml\`, add \`./marginalia.env\`
# to its \`env_file:\` list as well so Caddy receives \`{\$COMPANION_API_KEY}\` without
# needing to put it in your main \`.env\` file:
#
#   caddy:
#     env_file:
#       - ./marginalia.env

================================================================================
STEP 4: Add to your \`Caddyfile\` (BEFORE your Authelia \`forward_auth\` block)
================================================================================
${PUBLIC_DOMAIN} {
    # 1. E-Reader Companion API Gate (Evaluated BEFORE Authelia SSO)
    handle /api/ereader/* {
        @valid_ereader_key header X-API-Key "{\$COMPANION_API_KEY}"
        handle @valid_ereader_key {
            reverse_proxy ereader-companion:8095
        }
        header Content-Type "application/json"
        respond \`{"error":"Unauthorized"}\` 401
    }

    # 2. Existing Web Traffic (Protected by Authelia as usual)
    # handle {
    #     forward_auth authelia:9091 { ... }
    # }
}

================================================================================
STEP 5: Add to Authelia \`configuration.yml\` (if Authelia wraps ${PUBLIC_DOMAIN})
================================================================================
# Place this rule ABOVE your catch-all one_factor / two_factor rules:
access_control:
  rules:
    - domain:
        - "${PUBLIC_DOMAIN}"
      resources:
        - "^/api/ereader(/.*)?$"
      policy: bypass

================================================================================
STEP 6: Create \`/.crosspoint/companion.json\` on your E-Reader SD Card
================================================================================
Save this file at \`/.crosspoint/companion.json\` on your X4 Pro / BOOX Picco SD card:

{
  "host": "${PUBLIC_DOMAIN}",
  "api_key": "${COMPANION_API_KEY}",
  "enabled": true
}

================================================================================
STEP 7: Build, Start & Verify End-to-End
================================================================================
# 1. On your server (${SSH_USER}@${SSH_HOST}):
cd "${REMOTE_STACK_DIR}"
docker compose up -d --build ereader-companion
docker compose restart caddy

# 2. Verify local container health & indexed book count:
curl -s http://127.0.0.1:8095/healthz | jq .

# 3. Verify external HTTPS + X-API-Key enforcement through Caddy/Cloudflared:
#    (a) Without key -> should return HTTP 401 Unauthorized:
curl -i "https://${PUBLIC_DOMAIN}/api/ereader/v1/sync" -X POST

#    (b) With valid key -> should return HTTP 200 and synced book count:
curl -s "https://${PUBLIC_DOMAIN}/api/ereader/v1/sync" \\
  -X POST \\
  -H "X-API-Key: ${COMPANION_API_KEY}" | jq .
================================================================================
EOF
