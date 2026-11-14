#!/usr/bin/env bash
# Add, list or revoke device API keys of a deployed PageMind AI proxy.
#
#   ./rotate-key.sh list               # show key ids (first characters only)
#   ./rotate-key.sh add                # new key; writes ./pagemind-ai.json for the e-reader
#   ./rotate-key.sh revoke <key-id>    # remove keys starting with <key-id>
#
# Options: --project ID  --region REGION  --service NAME  --tokens-secret NAME
#          --setup-file PATH (default ./pagemind-ai.json)
# Changes take effect on a new Cloud Run revision, which this script starts.

set -euo pipefail

PROJECT="${GCP_PROJECT:-}"
REGION="${GCP_REGION:-us-central1}"
SERVICE="ai-proxy"
TOKENS_SECRET="ai-proxy-device-keys"
SETUP_FILE="$PWD/pagemind-ai.json"
CMD=""
ARG=""

die() { echo "error: $*" >&2; exit 1; }

# Writes the e-reader setup file (server URL + API key). Readable only by you.
write_setup_file() {  # write_setup_file PATH URL KEY
  local path="$1" url="$2" key="$3"
  (umask 077 && python3 -c 'import json,sys; json.dump({"serverUrl": sys.argv[1], "apiKey": sys.argv[2]}, open(sys.argv[3], "w"))' "$url" "$key" "$path")
  echo "Setup file written: $path"
  echo "  Copy it to the root of the e-reader's SD card and restart the reader."
  echo "  PageMind imports it, encrypts the key and deletes the file."
  echo "  Then delete this copy on your computer: it contains the key in plain text."
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project) PROJECT="$2"; shift 2 ;;
    --region) REGION="$2"; shift 2 ;;
    --service) SERVICE="$2"; shift 2 ;;
    --tokens-secret) TOKENS_SECRET="$2"; shift 2 ;;
    --setup-file) SETUP_FILE="$2"; shift 2 ;;
    -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
    list|add|revoke) CMD="$1"; shift ;;
    *)
      if [[ -n "$CMD" && -z "$ARG" ]]; then ARG="$1"; shift; else die "unknown argument: $1"; fi
      ;;
  esac
done

[[ -n "$CMD" ]] || die "usage: ./rotate-key.sh list|add|revoke <key-id>"
[[ -n "$PROJECT" ]] || PROJECT="$(gcloud config get-value project 2>/dev/null || true)"
[[ -n "$PROJECT" ]] || die "no project; pass --project"
G=(gcloud --project "$PROJECT" --quiet)

current="$("${G[@]}" secrets versions access latest --secret "$TOKENS_SECRET")"
IFS=',' read -r -a keys <<< "$current"

key_id() { printf '%s' "${1:0:10}"; }

save_and_restart() {
  local joined
  joined="$(IFS=','; printf '%s' "$*")"
  [[ -n "$joined" ]] || die "refusing to remove the last key (the service would not start)"
  printf '%s' "$joined" | "${G[@]}" secrets versions add "$TOKENS_SECRET" --data-file=- >/dev/null
  echo "Saved. Starting a new revision so the change takes effect..."
  "${G[@]}" run services update "$SERVICE" --region "$REGION" \
    --update-env-vars "KEYS_UPDATED_AT=$(date -u +%Y%m%dT%H%M%SZ)" >/dev/null
  echo "Done."
}

case "$CMD" in
  list)
    for k in "${keys[@]}"; do [[ -n "$k" ]] && echo "$(key_id "$k")…"; done
    ;;
  add)
    new="pm_$(python3 -c 'import secrets; print(secrets.token_urlsafe(32))')"
    save_and_restart "${keys[@]}" "$new"
    url="$("${G[@]}" run services describe "$SERVICE" --region "$REGION" --format 'value(status.url)')"
    write_setup_file "$SETUP_FILE" "$url" "$new"
    echo "Or enter it in the reader's web settings page (shown once): $new"
    ;;
  revoke)
    [[ ${#ARG} -ge 6 ]] || die "give at least 6 characters of the key id (see: list)"
    kept=()
    for k in "${keys[@]}"; do [[ -n "$k" && "$k" != "$ARG"* ]] && kept+=("$k"); done
    [[ ${#kept[@]} -lt ${#keys[@]} ]] || die "no key starts with '$ARG'"
    save_and_restart "${kept[@]}"
    ;;
esac
