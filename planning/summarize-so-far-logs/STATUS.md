# Summarize so far: live status

Plan: [../summarize-so-far.md](../summarize-so-far.md). This file is the resume point:
if a session stops (for example the token quota runs out), start a new session and say
"continue Summarize so far from planning/summarize-so-far-logs/STATUS.md".

## Rules for whoever works on this (agent or human)

1. Update this file after **every step**, not at the end of a phase.
2. Save raw output (test runs, eval reports, serial logs, build summaries) in
   `phase-N/` next to this file, one file per run, named `YYYYMMDD-HHMM-what.txt`.
3. Commit after each step with the status update, so the git history is the audit trail.
   Commits and pushes need the owner's approval; the owner approved this plan on 2026-11-21.
4. Record failures too, with the cause and what was changed.
5. Never put keys, device tokens or setup files in logs.

## Current position

- **Phase:** 5 closed by owner decision (2026-11-22): release `0.2.0` built from the merge of upstream `develop`; owner installs it over Wi-Fi
- **Done:** backup, flash of the summarize build, D1-D4 (owner), merge of upstream `develop` (PR #3787 now upstream), host 1092/1092, release build ok, `0.2.0` tag
- **Not run (accepted, owner asked to release now):** D5 Hindi, D6 errors, D7 orientation/input, D8 memory over many runs, D9 server log. The merged build has not run on the device before the release.
- **Next action:** none now. Remaining device checks are a to-do for when tokens are back: [planning/todo/summarize-device-verification.md](../todo/summarize-device-verification.md). Fixes ship as `0.2.1`.
- **Rollback:** hold Down at power-on for the firmware recovery picker (Phase 1 D4); backup in `~/projects/xteink-research/backups/2026-11-22-before-summarize/`
- **Live proxy:** revision `ai-proxy-00009-szv`

## To do later (owner decision 2026-11-22: not blocking)

- **Device verification (D2, D3, D5-D9, upstream-merge check):** [planning/todo/summarize-device-verification.md](../todo/summarize-device-verification.md)
- **Prompt refinement and latency:** see below.

Full task file: [planning/todo/summarize-prompt-refinement.md](../todo/summarize-prompt-refinement.md).

- **Prompt refinement (TBD).** The model still leaks small same-chapter facts in a few runs
  (judge flagged 1-2 of 29 per run: `tale-mail`, `alice-ch1`, `hp-zoo`, `hobbit-ch1`). Ideas: show
  the model only `text_before` for the recap and use `text_after` only to confirm the position;
  a second pass that deletes sentences past the boundary; a stronger model for this task; make
  judge flags a blocking gate in CI. Also one `502 provider_error` (46 s) on `trap-hp-wand` in run 2:
  consider a retry and a longer provider timeout for summarize.
- Run 3 of the final configuration was stopped early (owner direction, token budget): it had flagged
  `tale-mail` and `alice-ch1` before it was stopped; no report was written for it.

## Gate checklist

| Gate | Check | State | Evidence (log file / commit) |
|---|---|---|---|
| S1 | Schema accepts new fixtures, rejects bad ones | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| S2 | Existing tests and fixtures unchanged | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| S3 | Prompt builder cases | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| S4 | `UNKNOWN_BOOK` mapping | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| S5 | Per-task output budget | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| S6 | `/v1/info` lists `summarize` and limits | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| E1–E8 | Evals (see plan) | pass (accepted) | runs 1-2 of final config: 35/35 and 34/35, GATE PASS; phase-2/20261122-final-config-eval-3runs.txt; evals/reports/ |
| W1–W8 | Window extractor | pass | host tests 19/19 (phase-3/20261122-host-tests-summary-window.txt), full suite 1072/1072 (phase-3/20261122-host-tests-all.txt); W8 spike: phase-3/spike-note.md (device heap/time deferred to Phase 5 D8) |
| C1 | Host: summarize body matches contract fixtures; old requests unchanged | pass | AiClient/AiClientSummarize tests, phase-4/*host-tests-all.txt (1081/1081) |
| C2 | Host: response parsing, unknown-book answer is an answer, old proxy gives `ProxyOutdated` | pass | same log |
| C3 | Menu item hidden unless Ask AI is configured | pass (host + review) | `isConfigured` host tests (`test/ai_settings`); gating is `AI_SETTINGS.isConfigured()` in `EpubReaderMenuActivity::buildMenuItems`; device check in Phase 5 D1/D7 (same approach as Phase 5 A2) |
| C4 | `pio run -e pagemind_x4pro` builds; host tests pass; clang-format clean | pass | phase-4/*-pio-build.txt: RAM 34.1%, flash 84.9%; 1081/1081; `clang-format-fix -g` no changes |
| C5 | Contract stays in sync (server and device read the same fixtures) | pass | proxy pytest 92 passed (phase-4/*proxy-pytest.txt) and device host tests both validate every fixture in `cloud/ai-proxy/contract/` |
| D1 | Popular English book, mid-chapter | pass | Owner: works. Log: `Summarize: page 151/642, before 1080 B, after 1065 B, chapter 'THE BOY WHO LIVED'`; TLS 1.3 ok; `/v1/info` 200 in 11.5 s; `/v1/ask` 200 in 22.7 s; min free heap 83 KB, min block 34.8 KB (phase-5/20261122-0154-serial-d1.txt) |
| D2 | Chapter start and chapter end | pass (owner report) | Owner: works. No log: the logger died when the reader restarted itself after the first flow (USB port dropped); fixed with a reconnecting logger |
| D3 | Spoiler trap | pass (owner report) | Owner: no spoilers seen across multiple chapters; no single trap page checked |
| D4 | Unknown book | pass (owner report) | Owner: works |
| D5 | Hindi | not run | accepted by owner for the 0.2.0 release |
| D6 | Errors | not run | accepted by owner for the 0.2.0 release |
| D7 | Orientation and input | not run | accepted by owner for the 0.2.0 release |
| D8 | Memory and power | not run | D1 request: min free heap 83 KB, min block 34.8 KB; no repeated-run test |
| D9 | Request log on the server | not run | |
| Release | `0.2.0` published | tag pushed; see step log | GitHub release `PageMind 0.2.0` |

## Step log (newest first)

| Date | Step | Result | Log / commit |
|---|---|---|---|
| 2026-11-22 | Merged upstream `develop` (31 commits; PR #3787 merged upstream 2026-11-19). 6 conflicts: took upstream for PR-owned files, kept `FORK_DISPLAY_VERSION` and the `askAiAvailable` test stub; dropped the duplicated Indic test. Host 1092/1092; `pagemind_x4pro_release` builds (RAM 34.3%, flash 84.6%). Version 0.2.0, tag pushed | Done | phase-5/*merge* |
| 2026-11-22 | Phase 5: flashed the summarize build (hash verified, 118 s); D1-D4 pass per owner. Observations: on the device the ask took 22.7 s and `/v1/info` 11.5 s (cold start + two TLS handshakes), slower than the eval's p95 of 16 s through the proxy alone: add to the prompt-refinement to-do (latency) | Done | phase-5/ |
| 2026-11-22 | Phase 5: SD backup before flashing (`/.crosspoint` 423 files identical, `My Clippings.txt` 572 B); no stray `pagemind-ai.json` on the card | Done | backups/2026-11-22-before-summarize |
| 2026-11-22 | Phase 5 started: README (Summarize so far section: popular books only, what is sent incl. unread words after the position, spoiler caveat, update the server) and proxy README (2 new env vars) | In progress | git log |
| 2026-11-22 | Phase 4: tests first (C1/C2, replacing the Phase 3 skip), then `AiProtocol` (Summarize task, `ProxyOutdated`, window fields, `tailUtf8`, `checkTaskSupported`), `Section::getTextForPage`, menu item `SUMMARIZE`, `EpubReaderActivity::startSummarize`, `AiAskActivity` skips the picker, 2 new strings (English; other languages fall back). Host 1081/1081; firmware builds, flash 84.9% (was 83.3% at 0.1.0). First build attempt failed to start (`pio` not on PATH in the background shell): used `.venv/bin/pio` | Done | phase-4/ |
| 2026-11-22 | Phase 3: `src/ai/SummaryWindow.{h,cpp}` + `test/ai_summary_window` (19 tests, tests first). Found and fixed a red test left from Phase 1: `AiClient.RequestsFromFixturesMatchContract` read the new summarize fixtures; skipped them until Phase 4 (marked TODO). Added a 12-page-per-side cap. Spike note written. Full host suite 1072/1072, clang-format clean | Done | phase-3/ |
| 2026-11-22 | Phase 2 closed: final config (prompt v2 + corrected judge) run 1 **35/35 GATE PASS**, run 2 **34/35 GATE PASS** (one 502 from the router), run 3 stopped early. Owner accepted 34/35 and deferred prompt refinement (see To do later) | Done, accepted | phase-2/*final-config*, evals/reports/ |
| 2026-11-22 | Phase 2: **prompt v1 baseline** (run 1, Sonnet judge): 34/35 rules pass; judge flagged 11 of 29 answers as leaking later facts (mostly same-chapter details: 'five daughters', 'wait at Dover for a young lady', Alice growing). E1 FAIL, all other gates pass. Fixed eval flaw: forbidden term `burning` was in the reader's own text (added guard test, 91 tests). Prompt v2: exact-boundary rule, only-met-facts rule, self-check, stop-early rule; deployed as revision 00009-szv | v1 baseline saved | phase-2/20261122-prompt-v1-baseline-run1.txt, evals/reports/baseline/ |
| 2026-11-22 | Phase 2: wrote `evals/summarize_cases.jsonl` (35 cases: 16 known, 6 traps, 6 unknown, 4 edge, 3 Hindi), `evals/run_summarize_evals.py`, 13 scorer/case tests (90 pass). Smoke test of 3 cases passed; judge model had to be changed (sonnet-5-5 and others 404 on the router) | In progress | phase-2/ |
| 2026-11-21 | Phase 1: tests first (red), then schema, prompts, validation, UNKNOWN_BOOK, info limit, 7 fixtures | 77 passed; S2 note: the existing info test now expects `summarize` in `tasks` (additive change) | phase-1/*-pytest-all.txt |
| 2026-11-21 | Plan written and approved; status tracking set up | Done | 4a582587 |

## How to resume

1. `git pull` is not needed (single machine); run `git status --short` and `git log --oneline -10`.
2. Read "Current position" and the last rows of the step log.
3. Re-run the last recorded test command to confirm the state is still green.
4. Continue from "Next action".
