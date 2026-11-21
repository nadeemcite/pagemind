# Plan: "Ask AI" with Bring Your Own Cloud

This folder holds the implementation plan for the fork's first feature: **Ask AI on
selected text**, using an AI proxy that each user deploys to their own Google
Cloud project ("Bring Your Own Cloud", BYOC).

The hosted service (users sign up for an API key on our server) is **out of scope**
for this plan. The design keeps it possible later: the device only knows a server
URL and an API key, so a hosted server can be added without firmware changes.

## Goal

A reader selects text in a book, chooses **Ask AI**, picks a task (explain, define,
translate, ask a question) and reads a short answer on the device. The request goes
to the user's own Cloud Run proxy, which holds their OpenAI or Ollama key.

```
X4 Pro ──HTTPS, API key──▶ user's Cloud Run proxy ──provider key──▶ OpenAI / Ollama
  │                              (cloud/ai-proxy)
  └─ settings: server URL + API key (entered in the web UI, stored encrypted)
```

## Phases

| # | Phase | Main output | Status |
|---|---|---|---|
| 1 | [Fork identity, releases and OTA](phase-1-release-and-ota.md) | Fork builds and updates itself from its own GitHub releases | Complete |
| 2 | [BYOC server: deploy tooling, contract, evals](phase-2-byoc-server.md) | A user can deploy the proxy in minutes; API contract frozen; answer-quality evals | Complete |
| 3 | [Device settings and secure key storage](phase-3-device-settings.md) | Server URL and API key settings, key encrypted on the device | Complete |
| 4 | [Device AI client](phase-4-ai-client.md) | Firmware can call the proxy and parse answers, with no UI yet | Complete |
| 5 | [Ask AI user experience and public release](phase-5-ask-ai-ui.md) | Select text → Ask AI → answer screen; documented public release | Complete |

Why this order:

- **Phase 1 first:** before anyone else installs the fork, OTA must point to our
  releases. Otherwise "Update" in Settings silently replaces the fork with official
  CrossPoint and removes all fork features.
- **Phase 2 before the firmware client:** Phase 4 tests against a real deployed
  proxy and against contract fixtures that Phase 2 freezes.
- **Phase 3 before Phase 4:** the client needs the URL and key from settings.
- **Phase 5 last:** the UI only wires together pieces that are already tested.

## Phase gate rules

A phase is **complete** only when every check in its "Gate" section passes. The next
phase starts only after that.

1. **Automated checks** (host tests, server tests, builds, evals) are run by the
   agent, and the result is recorded with the commit SHA.
2. **Device checks** are run by the owner on a real X4 Pro. The agent never claims a
   device check passed.
3. Results go in the phase file's **Gate record** table: date, commit, result, and
   a link or path to the log.
4. The owner signs off by setting the phase status to **Complete** in the table above.
   This is the "promise for completion".
5. If a later phase finds a bug in an earlier phase, that earlier phase's gate checks
   are rerun after the fix, and its gate record is updated.
6. Scope changes during a phase are written in the phase file before the work is
   done, not after.

Status values: `Not started` → `In progress` → `Gate review` → `Complete`.

## Decisions

| Decision | Status | Needed by |
|---|---|---|
| Firmware name (repo name, release files, About screen, OTA) | **PageMind** (repo `nadeemcite/pagemind`); renamed from NCrossPoint on 2026-11-15 | Done |
| Google Cloud project and region for the owner's own proxy | `nadeem-personal`, `us-central1` | Done |
| First provider and model for evals | Ollama cloud, `glm-5.3-flash:cloud` (exact model name to confirm at first deploy) | Done |
| Target devices for public releases | X4 Pro only at first | Phase 1 |
| Upstream base | `upstream/develop` + PR #3787 (Indic) | Done |
| Proxy stack | Python FastAPI on Cloud Run, OpenAI + Ollama | Done |

## Backlog (not scheduled)

- **OTA certificate verification.** OTA downloads use HTTPS without checking the
  server certificate (inherited from CrossPoint; found in Phase 4). Fix: verify
  against GitHub's roots like the AI client does.
- **Demo capture tooling:** planned in [demo-capture.md](demo-capture.md).
- **Dev key cleanup.** Revoke the unused device key `pm_3tkphlY…` and delete
  `~/projects/xteink-research/setup/pagemind-ai.json` (owner: later, dev only).
- **"Set up from phone" QR button** in AI Assistant (idea from Phase 3).

## Rules that apply to every phase

- **Small upstream footprint:** new code goes in new files (`src/ai/`, `cloud/`,
  `planning/`). Existing upstream files get minimal hooks only, so upstream merges
  stay easy.
- **Memory:** the X4 Pro has 8 MB PSRAM, but internal RAM is still limited. Measure
  internal and PSRAM heap separately (`HalMemory`) before and after each new flow.
  No leaks across 20 repeated runs.
- **Privacy:** book text and answers are never logged, on the device or on the server.
- **Translations:** every user-facing string goes through I18n (`StrId`), with English
  at least.
- **Sync with upstream** at least once per phase, and rerun that phase's automated
  checks after the merge.
