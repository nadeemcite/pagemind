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

- **Phase:** 1 done locally; owner has not been asked to review it
- **Step:** Phase 1 committed
- **Next action:** Phase 2: deploy the proxy with `summarize`, write `evals/summarize_cases.jsonl` and scoring, run against 9router
- **Blocked on:** nothing

## Gate checklist

| Gate | Check | State | Evidence (log file / commit) |
|---|---|---|---|
| S1 | Schema accepts new fixtures, rejects bad ones | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| S2 | Existing tests and fixtures unchanged | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| S3 | Prompt builder cases | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| S4 | `UNKNOWN_BOOK` mapping | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| S5 | Per-task output budget | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| S6 | `/v1/info` lists `summarize` and limits | pass | pytest log phase-1/20261121-pytest-all.txt (77 passed) |
| E1–E8 | Evals (see plan) | todo | |
| W1–W8 | Window extractor | todo | |
| C1–C5 | Device client | todo | |
| D1–D9 | Device verification | todo | |
| Release | `0.2.0` published | todo | |

## Step log (newest first)

| Date | Step | Result | Log / commit |
|---|---|---|---|
| 2026-11-21 | Phase 1: tests first (red), then schema, prompts, validation, UNKNOWN_BOOK, info limit, 7 fixtures | 77 passed; S2 note: the existing info test now expects `summarize` in `tasks` (additive change) | phase-1/*-pytest-all.txt |
| 2026-11-21 | Plan written and approved; status tracking set up | Done | 4a582587 |

## How to resume

1. `git pull` is not needed (single machine); run `git status --short` and `git log --oneline -10`.
2. Read "Current position" and the last rows of the step log.
3. Re-run the last recorded test command to confirm the state is still green.
4. Continue from "Next action".
