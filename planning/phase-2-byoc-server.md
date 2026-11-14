# Phase 2: BYOC server, deploy tooling, API contract and evals

**Status:** Complete

## Goal

Anyone can deploy their own AI proxy to Google Cloud Run with one script, the
API between device and server is frozen as a versioned contract, and answer
quality is measured with repeatable evals.

## Entry criteria

- Phase 1 complete.
- `cloud/ai-proxy` exists with passing tests (done: commit `9958a187`).

## Work

1. **API contract v1** (`cloud/ai-proxy/contract/`)
   - `ask-request.schema.json`, `ask-response.schema.json`, `error.schema.json`.
   - Fixture files: valid requests (English, Hindi, long text at the limit), valid
     responses, every error case (401, 413, 422, 429, 502). Phase 4 firmware host
     tests reuse these same files, so device and server cannot drift apart.
   - Add `GET /v1/info` → `{"api": 1, "max_input_chars": ..., "max_context_chars": ...}`
     so the device can read the server's limits and show "connected" in settings.
2. **Deploy tooling**
   - `cloud/ai-proxy/deploy.sh`: interactive. It checks `gcloud` login, enables APIs,
     creates secrets (prompts for values, never echoes them), grants secret access,
     deploys, prints the URL and the device API key. Safe to run again (idempotent).
   - `cloud/ai-proxy/rotate-key.sh`: add or revoke a device API key.
   - Optional: "Run on Google Cloud" button in the README.
3. **Server hardening**
   - Request body size cap at the HTTP layer (not only after JSON parsing).
   - Reject unknown `task` values with a clear 422 (already done by the model; add a test).
   - Structured JSON logs for Cloud Logging (severity, device hash, task, sizes, latency).
4. **CI**
   - `.github/workflows/ai-proxy.yml` (new file): runs `uv run pytest` on changes
     under `cloud/ai-proxy/`.
5. **Evals** (`cloud/ai-proxy/evals/`)
   - `cases.jsonl`: at least 30 cases across tasks and languages: English and Hindi
     passages, Devanagari words for `define`, English→Hindi and Hindi→English
     `translate`, a spoiler-bait `ask`, a prompt-injection passage ("ignore your
     instructions..."), and very short and maximum-length inputs.
   - `run_evals.py --url <proxy> --key <key>`: calls the deployed proxy and scores
     each answer with rule-based checks:
     - non-empty, no Markdown (`#`, `*`, `-` list markers, backticks), no emoji
     - length within the word limit
     - answer language: for `language: Hindi`, at least 60% of letters are Devanagari
     - injection case: answer still follows the task
     - latency recorded (p50, p95)
   - Output: a summary table and a JSON report saved under `cloud/ai-proxy/evals/reports/`.

## Tests and evals

### Automated (agent)

| ID | Check | How |
|---|---|---|
| A1 | Server unit tests | `uv run pytest -q` all pass (current 10 + new tests for `/v1/info`, body-size cap, contract fixtures) |
| A2 | Contract fixtures validate | Every fixture validates against its JSON schema; server responses in tests validate against the response schemas |
| A3 | Container builds | `docker build` succeeds (or the Cloud Build step during deploy) |
| A4 | Proxy CI | `ai-proxy.yml` passes on the fork |
| A5 | Deploy script dry run | `deploy.sh --dry-run` prints the exact gcloud commands; `shellcheck` reports no errors |

### Live (owner's Cloud project; agent runs, owner provides project access)

| ID | Check | Expected |
|---|---|---|
| L1 | Fresh deploy with `deploy.sh` into an empty project | Service URL printed; `/healthz` OK in under 15 minutes total |
| L2 | Re-run `deploy.sh` | No errors; no duplicate secrets |
| L3 | Auth | Wrong key → 401; revoked key (via `rotate-key.sh`) → 401 after redeploy |
| L4 | Limits | 11 requests in one minute → the 11th is 429; text over the limit → 413 |
| L5 | Evals, OpenAI | `run_evals.py` pass rate ≥ 90% on rule checks; p95 latency ≤ 10 s |
| L6 | Evals, Ollama | Same thresholds with the chosen Ollama model |
| L7 | Logs | Cloud Logging shows no book text or answers |

## Gate

The phase is complete when A1–A5 and L1–L7 pass and are recorded below.
Promise for later phases: **API contract v1 is frozen.** Any change after this
gate needs a new contract version and a note in this file.

## Gate record

| Check | Date | Commit | Result | Log / notes |
|---|---|---|---|---|
| A1 | 2026-11-15 | e61c5f3a | Pass | `uv run pytest`: 46/46 (API, contract, evals scorer) |
| A2 | 2026-11-15 | e61c5f3a | Pass | All fixtures validate; server responses and every error code match fixtures (`tests/test_contract.py`) |
| A3 | 2026-11-15 | e61c5f3a | Pass | Container built by CI `docker build` and by Cloud Build during deploy |
| A4 | 2026-11-15 | e61c5f3a | Pass | `ai-proxy.yml` run 43640161099 success |
| A5 | 2026-11-15 | e61c5f3a | Pass | `deploy.sh --dry-run` lists all 6 changing commands; shellcheck clean (also in CI) |
| L1 | 2026-11-15 | e61c5f3a | Pass | Fresh deploy in `nadeem-personal`/`us-central1` after removing all proxy resources: 111 s, `/health` OK. First attempt found 3 bugs, fixed: new service account not ready (retry), `/healthz` reserved by Cloud Run (now `/health`), public access delay (health retry) |
| L2 | 2026-11-15 | e61c5f3a | Pass | Second run reused secrets and service account (1 secret version, 1 account), new revision, no errors |
| L3 | 2026-11-15 | e61c5f3a | Pass | Wrong key and no key → 401; `rotate-key.sh add` key → 200; after `revoke` → 401; original key still 200 |
| L4 | 2026-11-15 | e61c5f3a | Pass | 4001-char text → 413 `text_too_long`; 40 KB body → 413 `body_too_large`; requests 11–12 in one minute → 429 `rate_limited` |
| L5 | 2026-11-15 | cc8f9167 | Skipped | Owner decision: OpenAI not configured. OpenAI code path covered only by automated tests; run `evals/run_evals.py` against an OpenAI deploy before relying on it |
| L6 | 2026-11-15 | e61c5f3a | Pass | Ollama `glm-5.3-flash`: 33/33 (100%), p50 1.1 s, p95 2.1 s; spoiler and injection answers checked by hand. Report `cloud/ai-proxy/evals/reports/20261114T211034Z-ollama-glm-5.3-flash.json` |
| L7 | 2026-11-15 | e61c5f3a | Pass | 191 Cloud Logging entries: 42 structured `ask` entries with sizes/timing only; no book text, answers, keys or `Bearer` found |
| Owner sign-off | 2026-11-15 | cc8f9167 | Complete | Owner approved; API contract v1 frozen |
