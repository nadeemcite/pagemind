# PageMind AI proxy

A small server that the e-reader calls for "Ask AI". The device sends the selected
text; the proxy adds the prompt, calls the AI provider, and returns a short
plain-text answer.

```
e-reader ──HTTPS + device token──▶ AI proxy (Cloud Run) ──API key──▶ OpenAI or Ollama
```

Why a proxy:

- **Provider keys stay on the server** (Secret Manager). The device holds only a
  device token, which you can revoke without touching any provider account.
- **Prompts and models change without reflashing** the device.
- **One tiny API for the device**, whatever provider is behind it.

## API

Contract v1, with JSON schemas and example files: [`contract/README.md`](contract/README.md).

| Endpoint | Auth | Returns |
|---|---|---|
| `GET /health` | none | `{"ok": true}` |
| `GET /v1/info` | `Authorization: Bearer <api key>` | server limits, provider and model (also a key check) |
| `POST /v1/ask` | `Authorization: Bearer <api key>` | `{"answer": "...", "model": "...", "truncated": false}` |

`/v1/ask` body: `task` (`explain`, `define`, `translate`, `ask`), `text` (required),
and optional `context`, `question`, `language`, `book_title`, `book_author`.
Errors are `{"error": "<code>"}`; the codes are listed in the contract.

## Configuration (environment variables)

| Variable | Default | Meaning |
|---|---|---|
| `PROVIDER` | `openai` | `openai` or `ollama` |
| `DEVICE_TOKENS` | — | **Required.** Comma-separated device tokens |
| `OPENAI_API_KEY` | — | API key for OpenAI or your OpenAI-compatible router (secret) |
| `OPENAI_MODEL` | — | **Required for OpenAI.** Model name from your OpenAI account |
| `OPENAI_BASE_URL` | — | Any OpenAI-compatible endpoint (router, gateway, self-hosted), e.g. `https://router.example.com/v1/` |
| `OLLAMA_HOST` | `https://ollama.com` | Ollama cloud, or your own Ollama server URL |
| `OLLAMA_API_KEY` | — | Ollama cloud key (secret); not needed for your own server |
| `OLLAMA_MODEL` | — | **Required for Ollama.** Model name available on that host |
| `OLLAMA_THINK` | `false` | Let thinking models reason first (slower; uses the output budget) |
| `MAX_INPUT_CHARS` | `4000` | Longest selected text |
| `MAX_CONTEXT_CHARS` | `2000` | Longest surrounding text |
| `MAX_OUTPUT_TOKENS` | `400` | Answer length cap |
| `SUMMARIZE_MAX_OUTPUT_TOKENS` | `900` | Answer budget for Summarize so far (reasoning models spend part of it thinking) |
| `MAX_WINDOW_CHARS` | `3000` | Longest `text_before` / `text_after` for Summarize so far |
| `REQUESTS_PER_MINUTE` | `10` | Per device token |
| `MAX_BODY_BYTES` | `32768` | Largest request body, checked before parsing |
| `PROVIDER_TIMEOUT_S` | `30` | Provider request timeout |

The service refuses to start if required settings are missing.

The rate limit is kept in memory, so deploy with **one instance** (`--max-instances 1`).
That is plenty for personal use.

Logs never contain book text or answers: only a short hash of the device
token, the task, sizes, and timing.

## Run locally

```bash
cd cloud/ai-proxy
uv sync
uv run pytest -q

PROVIDER=openai OPENAI_API_KEY=sk-... OPENAI_MODEL=<model> DEVICE_TOKENS=dev123 \
  uv run uvicorn app.main:create_app --factory --port 8080

curl -s localhost:8080/v1/ask -H 'Authorization: Bearer dev123' \
  -H 'Content-Type: application/json' -d '{"task":"define","text":"serendipity"}'
```

## Deploy to Google Cloud Run

You need a Google Cloud project with billing turned on, `gcloud` installed and
logged in (`gcloud auth login`), and an API key from your provider.

```bash
cd cloud/ai-proxy
./deploy.sh
```

The script asks for the project, provider (`ollama` or `openai`) and model, then:

1. enables Cloud Run, Cloud Build, Artifact Registry and Secret Manager;
2. stores your provider key in Secret Manager (you paste it; it is never shown),
   or reuses an existing secret (`--key-secret NAME`);
3. creates a device API key and stores it in secret `ai-proxy-device-keys`;
4. creates service account `ai-proxy-runtime`, allowed to read only those two secrets;
5. deploys the service, prints the **server URL**, and writes the e-reader setup
   file **`pagemind-ai.json`** (server URL + device API key).

### Set up the e-reader

Easiest first:

1. **Setup file:** copy `pagemind-ai.json` to the root of the SD card (USB Drive
   mode, or upload it in File Transfer) and restart the reader. PageMind imports
   it, encrypts the key, turns Ask AI on and deletes the file. Then delete your
   computer's copy: it holds the key in plain text.
2. **Web settings page:** start File Transfer, open the reader's address from your
   phone or computer, go to **Settings → AI Assistant**, and paste the URL and key.
3. **On the reader:** Settings → Network → AI Assistant (on-screen keyboard).

Run it again after changes; it reuses what exists. `./deploy.sh --dry-run`
prints every command without changing anything. All options: `./deploy.sh --help`.

Example (OpenAI-compatible router, key already in secret `router_api_key`):

```bash
./deploy.sh --project my-project --region us-central1 \
  --provider openai --base-url https://router.example.com/v1/ \
  --model my-model --key-secret router_api_key
```

### Device API keys

```bash
./rotate-key.sh list            # key ids (first characters only)
./rotate-key.sh add             # new key + a new pagemind-ai.json
./rotate-key.sh revoke pm_AbCdEf  # remove a key (lost or old device)
```

`--allow-unauthenticated` lets the e-reader reach the URL without a Google
sign-in. Every `/v1/*` request still needs a device API key.

## Evals

Answer-quality checks against a deployed proxy (33 cases: English and Hindi,
all four tasks, spoiler and prompt-injection cases):

```bash
AI_PROXY_KEY=<device key> uv run python evals/run_evals.py --url https://ai-proxy-....run.app
```

Each answer is scored for plain text, length, answer language and expected or
forbidden words. Reports go to `evals/reports/`. The Phase 2 gate needs at least
90% passing and p95 latency of at most 10 seconds.

### Cost and safety

- With `--min-instances 0` the service sleeps when idle, so personal use
  normally stays within Cloud Run's free tier. The first request after a pause
  waits a few seconds for a cold start.
- Set a **spending limit** in your OpenAI/Ollama account and a **budget alert**
  in Google Cloud Billing.
- To revoke a device: `./rotate-key.sh revoke <key id>`.
