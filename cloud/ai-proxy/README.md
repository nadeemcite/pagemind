# CrossPoint AI proxy

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

`POST /v1/ask` with `Authorization: Bearer <device token>`

```json
{
  "task": "explain",            // explain | define | translate | ask
  "text": "selected text",      // required, up to MAX_INPUT_CHARS
  "context": "nearby text",     // optional, up to MAX_CONTEXT_CHARS
  "question": "why?",           // only used by task "ask"
  "language": "Hindi",          // language of the answer (optional)
  "book_title": "...", "book_author": "..."   // optional
}
```

Response `200`:

```json
{ "answer": "plain text", "model": "model-name", "truncated": false }
```

Errors return `{"error": "..."}` with status `401` (bad token), `413` (text too
long), `422` (bad request body), `429` (rate limit), or `502` (provider failed).

`GET /healthz` returns `{"ok": true}` without a token.

## Configuration (environment variables)

| Variable | Default | Meaning |
|---|---|---|
| `PROVIDER` | `openai` | `openai` or `ollama` |
| `DEVICE_TOKENS` | — | **Required.** Comma-separated device tokens |
| `OPENAI_API_KEY` | — | OpenAI key (secret) |
| `OPENAI_MODEL` | — | **Required for OpenAI.** Model name from your OpenAI account |
| `OPENAI_BASE_URL` | — | Optional OpenAI-compatible endpoint instead of OpenAI |
| `OLLAMA_HOST` | `https://ollama.com` | Ollama cloud, or your own Ollama server URL |
| `OLLAMA_API_KEY` | — | Ollama cloud key (secret); not needed for your own server |
| `OLLAMA_MODEL` | — | **Required for Ollama.** Model name available on that host |
| `MAX_INPUT_CHARS` | `4000` | Longest selected text |
| `MAX_CONTEXT_CHARS` | `2000` | Longest surrounding text |
| `MAX_OUTPUT_TOKENS` | `400` | Answer length cap |
| `REQUESTS_PER_MINUTE` | `10` | Per device token |
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

One-time setup (replace `PROJECT_ID`; pick a region near you, e.g. `asia-south1`):

```bash
gcloud auth login
gcloud config set project PROJECT_ID
gcloud services enable run.googleapis.com cloudbuild.googleapis.com \
  artifactregistry.googleapis.com secretmanager.googleapis.com

# A random device token (keep it; you will enter it on the e-reader)
python3 -c "import secrets; print(secrets.token_urlsafe(32))"

# Store secrets (you will be prompted to paste each value; nothing goes in shell history)
gcloud secrets create device-tokens --data-file=-
gcloud secrets create openai-api-key --data-file=-     # if using OpenAI
gcloud secrets create ollama-api-key --data-file=-     # if using Ollama cloud

# Let Cloud Run's runtime account read them
PROJECT_NUMBER=$(gcloud projects describe PROJECT_ID --format='value(projectNumber)')
for s in device-tokens openai-api-key; do
  gcloud secrets add-iam-policy-binding $s \
    --member="serviceAccount:${PROJECT_NUMBER}-compute@developer.gserviceaccount.com" \
    --role=roles/secretmanager.secretAccessor
done
```

Deploy (from the repo root; run again after any change):

```bash
gcloud run deploy ai-proxy \
  --source cloud/ai-proxy \
  --region asia-south1 \
  --allow-unauthenticated \
  --max-instances 1 --min-instances 0 \
  --memory 512Mi --cpu 1 --timeout 60 \
  --set-env-vars PROVIDER=openai,OPENAI_MODEL=<model> \
  --set-secrets DEVICE_TOKENS=device-tokens:latest,OPENAI_API_KEY=openai-api-key:latest
```

For Ollama, use
`--set-env-vars PROVIDER=ollama,OLLAMA_MODEL=<model>` and
`--set-secrets DEVICE_TOKENS=device-tokens:latest,OLLAMA_API_KEY=ollama-api-key:latest`.

`--allow-unauthenticated` lets the device reach the URL without Google sign-in
(an e-reader cannot do that). Every `/v1/ask` request still needs a valid
device token.

The command prints the service URL (`https://ai-proxy-....run.app`). Test it:

```bash
curl -s https://ai-proxy-....run.app/v1/ask -H "Authorization: Bearer <device token>" \
  -H 'Content-Type: application/json' -d '{"task":"explain","text":"Hello world"}'
```

### Cost and safety

- With `--min-instances 0` the service sleeps when idle, so personal use
  normally stays within Cloud Run's free tier. The first request after a pause
  waits a few seconds for a cold start.
- Set a **spending limit** in your OpenAI/Ollama account and a **budget alert**
  in Google Cloud Billing.
- To revoke a device: add a new version of the `device-tokens` secret without
  its token, then redeploy.
