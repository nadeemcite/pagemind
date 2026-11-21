# AI proxy API contract v1

The contract between the PageMind firmware and the AI proxy. The server tests
(`tests/test_contract.py`) and the firmware host tests (Phase 4) both read the
files in this folder, so the two sides cannot drift apart.

**Status: frozen (Phase 2 complete, 2026-11-15).** Changing it afterwards needs a new
version (`/v2/...`, `"api": 2`) and a note in `planning/phase-2-byoc-server.md`.

## Endpoints

| Method and path | Auth | Success | Schema |
|---|---|---|---|
| `GET /health` | none | `200 {"ok": true}` | — |
| `GET /v1/info` | `Authorization: Bearer <api key>` | `200` server limits | `info-response.schema.json` |
| `POST /v1/ask` | `Authorization: Bearer <api key>` | `200` answer | request `ask-request.schema.json`, response `ask-response.schema.json` |

The device calls `/v1/info` for "Test connection": a `200` proves the URL and key
are right, and gives the limits it must trim text to.

Not `/healthz`: Cloud Run reserves some paths ending in `z` and answers them
with its own 404.

## Requests

- Body is UTF-8 JSON, at most 32,768 bytes (server setting `MAX_BODY_BYTES`).
- `text` is required for every task except `summarize`. Lengths are counted in Unicode characters.
- Unknown fields are rejected by the schema; the server ignores them, so the
  device must not send any.

## Summarize (additive, 2026-11-21)

`task: "summarize"` is added to v1 without a version change: existing requests, fixtures and
devices are unaffected. It takes `book_title`, `book_author`, `chapter`, `text_before` (required,
about 200 words ending at the reading position) and `text_after` (about 200 words after it, for
locating only), plus `language`. Both windows are limited by `max_window_chars` from `/v1/info`
(3000); over the limit is `413 context_too_long`. Proxies older than this change do not list
`summarize` in `tasks`, and the device must say the proxy needs an update. When the model does not
know the book, the proxy answers `200` with a fixed "not well known" message. Design and gates:
`planning/summarize-so-far.md`.

## Errors

Every error is `{"error": "<code>"}`:

| Status | Code | Device message (meaning) |
|---|---|---|
| 401 | `unauthorized` | Wrong or revoked API key |
| 413 | `body_too_large` | Request too big |
| 413 | `text_too_long` | Selection too long |
| 413 | `context_too_long` | Context too long (device trimming bug) |
| 422 | `invalid_request` | Malformed request (device bug) |
| 429 | `rate_limited` | Too many requests, wait a minute |
| 502 | `provider_error` | AI provider failed, try again |

Any other status (for example a Cloud Run 503 during a cold start, or 5xx from
Google's front end) has no guaranteed body; the device treats it as
"server unavailable".

Only a JSON body with an `error` code above comes from the proxy. Google's front
end can also answer with an HTML page, for example `401` while a new service's
public access is still being applied, or `404` for a reserved path. The device
must treat any response without a valid JSON `error` body as "server
unavailable", never as "wrong API key".

## Fixtures

`fixtures/requests/*.json` are valid requests, `fixtures/responses/*.json` valid
`200` bodies, and `fixtures/errors/<status>-<code>.json` every error body.
