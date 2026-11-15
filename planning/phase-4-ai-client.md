# Phase 4: Device AI client

**Status:** In progress

## Goal

The firmware can send an Ask request to the proxy over HTTPS and turn the reply
into either an answer or a clear error, with bounded memory and correct Wi-Fi
handling. No reader UI yet: it is triggered from a debug entry point.

## Entry criteria

- Phase 3 complete (settings give the URL and decrypted key).
- Contract v1 fixtures available from Phase 2.

## Work

1. **HTTP POST with JSON**
   - `src/network/HttpDownloader.h` already has `postForm` and `postToFile` with headers
     and TLS heap checks (`MIN_TLS_FREE_HEAP = 40000`, `MIN_TLS_MAX_ALLOC = 20000`).
     Add a small `postJson(url, body, headers, outResponse, maxResponseBytes)` there, or
     in a new `src/ai/AiHttp.cpp` if that keeps upstream files untouched.
   - Response size cap (for example 16 KB); a larger response is an error, not a crash.
   - TLS certificate checking stays on (use the same CA handling as OTA).
2. **Client** (`src/ai/AiClient.{h,cpp}`, new files)
   - `AiClient::ask(const AiRequest&, AiAnswer& out) → AiError`.
   - Builds the request JSON from contract v1 (escape text correctly; trim `text` and
     `context` to the server limits read from `/v1/info`, cutting only at UTF-8
     character boundaries).
   - Parses the response with ArduinoJson using a bounded document.
   - Error mapping: no Wi-Fi, DNS/TLS failure, timeout, 401 bad key, 413 too long,
     429 rate limited, 502 provider failed, malformed reply. Each maps to one
     translated message.
3. **Wi-Fi lifecycle**
   - Reuse the existing Wi-Fi flow (`WifiSelectionActivity`, saved networks).
   - Connect on demand; release Wi-Fi and power locks when done (rule in
     `.agents/rules/ui-activities.md`, "Network flows release WiFi and power locks").
   - The HTTPS call runs in a worker task, not in the render task; the activity
     deletes the task in `onExit()`.
4. **Debug entry point**
   - Serial command `CMD:AI_TEST` (next to `CMD:SCREENSHOT` in `src/main.cpp`), only in
     builds with `ENABLE_SERIAL_LOG`: sends a fixed request and logs status, latency,
     heap before/after. Never logs the key or the answer text in release builds.

## Tests and evals

### Automated (agent)

| ID | Check | How |
|---|---|---|
| A1 | Request JSON matches contract | New host test `test/ai_client`: requests built for each Phase 2 request fixture validate against `ask-request.schema.json`; Hindi text and quotes/newlines escaped correctly |
| A2 | Response parsing | Every Phase 2 response and error fixture parses to the expected `AiAnswer` / `AiError` |
| A3 | Hostile input | Truncated JSON, wrong types, 1 MB body, missing fields, invalid UTF-8 → clean `AiError`, no crash (host test with sanitizers if available) |
| A4 | UTF-8-safe trimming | Trimming Devanagari/emoji text at many lengths never splits a character |
| A5 | Builds and existing tests | `pio run -e x4pro`; all host tests pass; clang-format clean |

### Device (owner, with agent-prepared steps)

| ID | Check | Expected |
|---|---|---|
| D1 | `CMD:AI_TEST` with valid settings | Answer received; latency logged (warm < 10 s) |
| D2 | Wrong key | Maps to "bad API key" error |
| D3 | Wi-Fi off / no saved network | Maps to "no Wi-Fi" error within 15 s, no hang |
| D4 | Server stopped (scale to zero, wrong URL) | Timeout/connection error, no hang, no reboot |
| D5 | Rate limit (11 quick calls) | 11th maps to "too many requests" |
| D6 | Memory | 20 calls in a row: internal heap and largest block return to the starting value (±1 KB); no PSRAM growth |
| D7 | Power | Wi-Fi is off after each call; the device still auto-sleeps normally |

## Gate

The phase is complete when A1–A5 and D1–D7 pass and are recorded below.
Promise for later phases: **`AiClient::ask()` returns an answer or one of the defined
errors, never blocks the UI, and leaves Wi-Fi and memory as it found them.**

## Gate record

| Check | Date | Commit | Result | Log / notes |
|---|---|---|---|---|
| A1 | | | | |
| A2 | | | | |
| A3 | | | | |
| A4 | | | | |
| A5 | | | | |
| D1 | | | | |
| D2 | | | | |
| D3 | | | | |
| D4 | | | | |
| D5 | | | | |
| D6 | | | | |
| D7 | | | | |
| Owner sign-off | | | | |
