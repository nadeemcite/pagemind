#!/usr/bin/env bash
# Deploy the PageMind AI proxy to Google Cloud Run in your own project.
# Safe to run again: existing secrets and the service account are reused.
#
#   ./deploy.sh                       # asks for anything missing
#   ./deploy.sh --project my-proj --provider ollama --model glm-5.3-flash
#   ./deploy.sh --dry-run ...         # print the commands, change nothing
#
# Options (or environment variables in brackets):
#   --project ID          Google Cloud project            [GCP_PROJECT]
#   --region REGION       Cloud Run region, default us-central1 [GCP_REGION]
#   --service NAME        Cloud Run service, default ai-proxy
#   --provider NAME       ollama or openai                [AI_PROVIDER]
#   --model NAME          model name at the provider      [AI_MODEL]
#   --key-secret NAME     secret holding the provider API key
#                         (default: ollama-api-key or openai-api-key)
#   --tokens-secret NAME  secret holding device API keys, default ai-proxy-device-keys
#   --setup-file PATH     where to write the e-reader setup file, default ./pagemind-ai.json
#   --dry-run             print what would run
#   --yes                 do not ask for confirmation

set -euo pipefail

PROJECT="${GCP_PROJECT:-}"
REGION="${GCP_REGION:-us-central1}"
SERVICE="ai-proxy"
PROVIDER="${AI_PROVIDER:-}"
MODEL="${AI_MODEL:-}"
KEY_SECRET=""
TOKENS_SECRET="ai-proxy-device-keys"
SETUP_FILE="$PWD/pagemind-ai.json"
DRY_RUN=0
ASSUME_YES=0
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_SA_NAME="ai-proxy-runtime"

die() { echo "error: $*" >&2; exit 1; }
info() { echo "==> $*"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project) PROJECT="$2"; shift 2 ;;
    --region) REGION="$2"; shift 2 ;;
    --service) SERVICE="$2"; shift 2 ;;
    --provider) PROVIDER="$2"; shift 2 ;;
    --model) MODEL="$2"; shift 2 ;;
    --key-secret) KEY_SECRET="$2"; shift 2 ;;
    --tokens-secret) TOKENS_SECRET="$2"; shift 2 ;;
    --setup-file) SETUP_FILE="$2"; shift 2 ;;
    --dry-run) DRY_RUN=1; shift ;;
    --yes|-y) ASSUME_YES=1; shift ;;
    -h|--help) sed -n '2,23p' "$0"; exit 0 ;;
    *) die "unknown option: $1 (see --help)" ;;
  esac
done

# Run a command that changes something; in dry-run mode only print it.
run() {
  if [[ $DRY_RUN -eq 1 ]]; then
    printf '[dry-run]'; printf ' %q' "$@"; printf '\n'
  else
    "$@"
  fi
}

# Same, but hide the command's normal output (errors still show).
run_quiet() {
  if [[ $DRY_RUN -eq 1 ]]; then run "$@"; else "$@" >/dev/null; fi
}

ask() {  # ask VAR "Prompt" [default]
  local var="$1" prompt="$2" default="${3:-}" reply
  if [[ -n "$default" ]]; then prompt="$prompt [$default]"; fi
  read -r -p "$prompt: " reply
  printf -v "$var" '%s' "${reply:-$default}"
}

# Writes the e-reader setup file (server URL + API key). Readable only by you.
write_setup_file() {  # write_setup_file PATH URL KEY
  local path="$1" url="$2" key="$3"
  (umask 077 && python3 -c 'import json,sys; json.dump({"serverUrl": sys.argv[1], "apiKey": sys.argv[2]}, open(sys.argv[3], "w"))' "$url" "$key" "$path")
  echo "Setup file written: $path"
  echo "  Copy it to the root of the e-reader's SD card and restart the reader."
  echo "  PageMind imports it, encrypts the key and deletes the file."
  echo "  Then delete this copy on your computer: it contains the key in plain text."
}

command -v gcloud >/dev/null || die "gcloud is not installed: https://cloud.google.com/sdk/docs/install"

LOGGED_IN=1
if ! gcloud auth list --filter=status:ACTIVE --format='value(account)' 2>/dev/null | grep -q .; then
  [[ $DRY_RUN -eq 1 ]] || die "not logged in; run: gcloud auth login"
  LOGGED_IN=0
fi

[[ -n "$PROJECT" ]] || PROJECT="$(gcloud config get-value project 2>/dev/null || true)"
[[ -n "$PROJECT" ]] || ask PROJECT "Google Cloud project ID"
[[ -n "$PROVIDER" ]] || ask PROVIDER "Provider (ollama or openai)" "ollama"
case "$PROVIDER" in ollama|openai) ;; *) die "provider must be ollama or openai" ;; esac
[[ -n "$MODEL" ]] || ask MODEL "Model name at $PROVIDER"
[[ -n "$MODEL" ]] || die "a model name is required"
[[ -n "$KEY_SECRET" ]] || KEY_SECRET="${PROVIDER}-api-key"

if [[ "$PROVIDER" == "ollama" ]]; then
  MODEL_VAR="OLLAMA_MODEL"; KEY_VAR="OLLAMA_API_KEY"
else
  MODEL_VAR="OPENAI_MODEL"; KEY_VAR="OPENAI_API_KEY"
fi

echo
echo "Project:        $PROJECT"
echo "Region:         $REGION"
echo "Service:        $SERVICE"
echo "Provider/model: $PROVIDER / $MODEL"
echo "Key secret:     $KEY_SECRET"
echo "Device keys:    $TOKENS_SECRET"
[[ $DRY_RUN -eq 1 ]] && echo "Mode:           dry run (nothing will change)"
echo
if [[ $ASSUME_YES -eq 0 && $DRY_RUN -eq 0 ]]; then
  read -r -p "Continue? [y/N] " ok
  [[ "$ok" == "y" || "$ok" == "Y" ]] || die "cancelled"
fi

G=(gcloud --project "$PROJECT" --quiet)

secret_exists() {
  [[ $LOGGED_IN -eq 1 ]] && "${G[@]}" secrets describe "$1" >/dev/null 2>&1
}

info "Enabling APIs (Cloud Run, Cloud Build, Artifact Registry, Secret Manager)"
run "${G[@]}" services enable run.googleapis.com cloudbuild.googleapis.com \
  artifactregistry.googleapis.com secretmanager.googleapis.com

info "Provider API key secret: $KEY_SECRET"
if secret_exists "$KEY_SECRET"; then
  echo "    exists, reusing it"
else
  if [[ $DRY_RUN -eq 1 ]]; then
    run "${G[@]}" secrets create "$KEY_SECRET" --data-file=-
  else
    read -r -s -p "    Paste your $PROVIDER API key (input hidden): " PROVIDER_KEY; echo
    [[ -n "$PROVIDER_KEY" ]] || die "empty API key"
    printf '%s' "$PROVIDER_KEY" | "${G[@]}" secrets create "$KEY_SECRET" --data-file=-
    unset PROVIDER_KEY
  fi
fi

NEW_DEVICE_KEY=""
info "Device API keys secret: $TOKENS_SECRET"
if secret_exists "$TOKENS_SECRET"; then
  echo "    exists, reusing it (use ./rotate-key.sh to add or revoke keys)"
else
  NEW_DEVICE_KEY="pm_$(python3 -c 'import secrets; print(secrets.token_urlsafe(32))')"
  if [[ $DRY_RUN -eq 1 ]]; then
    run "${G[@]}" secrets create "$TOKENS_SECRET" --data-file=-
  else
    printf '%s' "$NEW_DEVICE_KEY" | "${G[@]}" secrets create "$TOKENS_SECRET" --data-file=-
  fi
fi

RUNTIME_SA="${RUNTIME_SA_NAME}@${PROJECT}.iam.gserviceaccount.com"
info "Runtime service account: $RUNTIME_SA (can read only these two secrets)"
if [[ $LOGGED_IN -eq 1 ]] && "${G[@]}" iam service-accounts describe "$RUNTIME_SA" >/dev/null 2>&1; then
  echo "    exists, reusing it"
else
  run "${G[@]}" iam service-accounts create "$RUNTIME_SA_NAME" --display-name "PageMind AI proxy runtime"
fi
# A new service account can take up to a minute to become usable for IAM bindings.
grant_secret_access() {
  local secret="$1" attempt
  for attempt in 1 2 3 4 5 6 7 8; do
    if run_quiet "${G[@]}" secrets add-iam-policy-binding "$secret" \
         --member "serviceAccount:$RUNTIME_SA" --role roles/secretmanager.secretAccessor 2>/dev/null; then
      return 0
    fi
    echo "    service account not ready yet, retrying in 10s ($attempt/8)"
    sleep 10
  done
  die "could not grant $RUNTIME_SA access to secret $secret"
}
for s in "$KEY_SECRET" "$TOKENS_SECRET"; do
  grant_secret_access "$s"
done

info "Deploying to Cloud Run (builds the container with Cloud Build; takes a few minutes)"
run "${G[@]}" run deploy "$SERVICE" \
  --source "$SOURCE_DIR" \
  --region "$REGION" \
  --service-account "$RUNTIME_SA" \
  --allow-unauthenticated \
  --max-instances 1 --min-instances 0 \
  --memory 512Mi --cpu 1 --timeout 60 \
  --set-env-vars "PROVIDER=$PROVIDER,$MODEL_VAR=$MODEL" \
  --set-secrets "DEVICE_TOKENS=$TOKENS_SECRET:latest,$KEY_VAR=$KEY_SECRET:latest"

if [[ $DRY_RUN -eq 1 ]]; then
  info "Dry run finished; nothing was changed."
  exit 0
fi

URL="$("${G[@]}" run services describe "$SERVICE" --region "$REGION" --format 'value(status.url)')"
# A new service's public access can take a minute to apply; retry before reporting a failure.
info "Checking $URL/health"
for attempt in 1 2 3 4 5 6 7 8 9; do
  if curl -fsS --max-time 30 "$URL/health"; then echo; break; fi
  [[ $attempt -lt 9 ]] || die "service did not answer on $URL/health; check: gcloud run services logs read $SERVICE --region $REGION --project $PROJECT"
  echo "    not reachable yet, retrying in 10s ($attempt/8)"
  sleep 10
done

echo
echo "Done. Server URL: $URL"
if [[ -n "$NEW_DEVICE_KEY" ]]; then
  write_setup_file "$SETUP_FILE" "$URL" "$NEW_DEVICE_KEY"
  echo "Or enter the URL and this key in the reader's web settings page (shown once):"
  echo "  API key: $NEW_DEVICE_KEY"
else
  echo "Existing device keys were kept. For a new key and setup file: ./rotate-key.sh add"
fi
