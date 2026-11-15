# Phase 4: Device AI client

**Status:** Gate review

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

## Decisions (2026-11-15, approved by owner before the work)

- **TLS verification (option A).** CrossPoint skips certificate checks on every
  HTTPS client (`setInsecure()`; the SDK has no CA bundle, see
  `freeink-sdk/libs/network/SecureNet/include/SecureHttpClient.h:69-71`). The AI
  client instead verifies against built-in roots (`src/ai/AiTrustedRoots.h`):
  GTS Root R1–R4 and GlobalSign Root CA (Cloud Run: `*.a.run.app` → WR2 → GTS Root
  R1, cross-signed by GlobalSign), ISRG Root X1/X2 (Let's Encrypt, custom domains).
  No server change. Redirects stay off, so the key never goes to another host.
- **OTA verification is a known gap** (same `setInsecure()`); tracked in the
  backlog in `planning/README.md`, not part of this phase.
- **Test entry point:** instead of a serial `CMD:AI_TEST`, a **Test connection**
  row in Settings → Network → AI Assistant (moved here from Phase 3). It joins
  Wi-Fi with the standard selection screen, calls `/v1/info` and one sample
  `/v1/ask`, and shows status, model, latency and the lowest free heap. Confirm
  repeats the ask. It follows `OtaUpdateActivity`: the request runs on the loop
  task after the "working" frame is painted, and leaving turns Wi-Fi off and does
  a silent restart to give back network heap. Phase 5 revisits this for the
  in-book flow.
- **Added check A6:** the root bundle verifies the live Cloud Run chain and
  rejects an unrelated chain (`openssl verify`, `scripts/check_ai_roots.py`).
- **Added check D8:** an `https` server whose certificate is not from these roots
  is rejected with a clear "certificate" error.

## Tests and evals

### Automated (agent)

| ID | Check | How |
|---|---|---|
| A1 | Request JSON matches contract | New host test `test/ai_client`: requests built for each Phase 2 request fixture validate against `ask-request.schema.json`; Hindi text and quotes/newlines escaped correctly |
| A2 | Response parsing | Every Phase 2 response and error fixture parses to the expected `AiAnswer` / `AiError` |
| A3 | Hostile input | Truncated JSON, wrong types, 1 MB body, missing fields, invalid UTF-8 → clean `AiError`, no crash (host test with sanitizers if available) |
| A4 | UTF-8-safe trimming | Trimming Devanagari/emoji text at many lengths never splits a character |
| A6 | Root bundle | `scripts/check_ai_roots.py`: bundle verifies the live `*.a.run.app` chain; an unrelated chain fails |
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
| D8 | Untrusted certificate | Server URL set to an `https` host with a certificate outside the bundle (agent picks one) → "could not connect" on screen, serial log shows the wolfSSL verify error; the request (and key) is never sent. The SDK exposes no TLS error code, so the screen cannot say "certificate" specifically |
| D7 | Power | Wi-Fi is off after each call; the device still auto-sleeps normally |

## Gate

The phase is complete when A1–A5 and D1–D7 pass and are recorded below.
Promise for later phases: **`AiClient::ask()` returns an answer or one of the defined
errors, never blocks the UI, and leaves Wi-Fi and memory as it found them.**

## Gate record

| Check | Date | Commit | Result | Log / notes |
|---|---|---|---|---|
| A1 | 2026-11-15 | 19e1f708 | Pass | `AiClientTest`: requests built from every contract request fixture match `ask-request.schema.json` (fields, enum, code-point lengths); quotes, newlines, backslash, Hindi round-trip |
| A2 | 2026-11-15 | 19e1f708 | Pass | Every response and error fixture maps to the expected answer or error (7 error codes) |
| A3 | 2026-11-15 | 19e1f708 | Pass | Connection failure, Google HTML 401/404, 503, wrong codes, 9 malformed 200 bodies, invalid UTF-8, 16 KB+ body → clean errors |
| A4 | 2026-11-15 | 19e1f708 | Pass | Trimming 1/2/3/4-byte UTF-8 at every length never splits a character; counts code points like the server |
| A5 | 2026-11-15 | 19e1f708 | Pass | `pagemind_x4pro` builds (RAM 34.1%, flash 84.5%, +21 KB vs Phase 3); host tests 1047/1047; clang-format clean | Re-run after the touch and restart fixes: release build ok (RAM 34.1%, flash 83.8%), 1047/1047 |
| A6 | 2026-11-15 | 19e1f708 | Pass | `scripts/check_ai_roots.py`: bundle (7 roots) verifies `ai-proxy-cyshxss5ta-uc.a.run.app`; `www.digicert.com` rejected |
| D1 | 2026-11-15 | bcb1c256 (0.0.3-dev) | Pass | Owner: Test connection shows Connected, ollama/glm-5.3-flash, answer. Serial: TLS 1.3 handshake with certificate verification ok (~488 ms); /v1/info 200 (4,975 ms cold start, ~900 ms warm); asks ~2.0 s. Touch fix: Ask again button added (button hints are hidden on touch boards) |
| D2 | 2026-11-15 | bcb1c256 (0.0.3-dev) | Pass | API key set to `wrong` on device: `/v1/info` → 401 → "API key rejected". Restored with the SD setup file: `[AISET] Imported AI settings`, then 200; server device id changed 72c4904fa78f → 3a17fed0a518 (setup-file key) |
| D3 | 2026-11-15 | bcb1c256 (0.0.3-dev) | Pass | Back on the Wi-Fi screen: no hang, no loop. It landed on Home (silent restart); changed to `silentRestartToSettings()` in the next build |
| D4 | 2026-11-15 | bcb1c256 (0.0.3-dev) | Pass | URL `https://ai-proxy-doesnotexist-uc.a.run.app`: TLS ok, `/v1/info` → 404 (Google front end, empty body) → "AI server unavailable"; no hang, no reboot. A DNS-failure URL was not tested separately |
| D5 | 2026-11-15 | bcb1c256 (0.0.3-dev) | Pass | Asks 1–10 → 200, asks 11–14 in the same minute → 429 `rate_limited` shown as "Too many requests"; later asks 200 again |
| D6 | 2026-11-15 | bcb1c256 (0.0.3-dev) | Pass | 25 asks (all 200, avg 2,075 ms) then ~4 min idle on the test screen: free 101,096 → 101,700 B, largest block 53,236 → 57,332 B, PSRAM 7,219,652 B both ends. Dip to 97,804 B during the run recovers in ~260 B steps (lwIP closed-connection state, ~2 min). Lowest free during a request 86,152 B (TLS floor 40,000 B). An earlier run without the idle wait was inconclusive and is superseded |
| D7 | 2026-11-15 | bcb1c256 (0.0.3-dev) | Pass | Each exit from the test screen does a silent restart (uptime resets), so Wi-Fi is off; the device auto-slept on its own during testing |
| D8 | 2026-11-15 | bcb1c256 (0.0.3-dev) | Pass | URL `https://www.digicert.com`: `wolfSSL_connect failed: -188` (ASN_NO_SIGNER_E) on TLS 1.3 and 1.2 retry; status 0, no HTTP request sent; screen "Could not connect to the AI server". Restored URL → 200 |
| Owner sign-off | | | | |
