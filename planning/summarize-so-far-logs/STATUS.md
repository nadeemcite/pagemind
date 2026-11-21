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

- **Phase:** 1 (Contract and server)
- **Step:** not started
- **Next action:** write the contract schema changes (S1), then tests first
- **Blocked on:** nothing

## Gate checklist

| Gate | Check | State | Evidence (log file / commit) |
|---|---|---|---|
| S1 | Schema accepts new fixtures, rejects bad ones | todo | |
| S2 | Existing tests and fixtures unchanged | todo | |
| S3 | Prompt builder cases | todo | |
| S4 | `UNKNOWN_BOOK` mapping | todo | |
| S5 | Per-task output budget | todo | |
| S6 | `/v1/info` lists `summarize` and limits | todo | |
| E1–E8 | Evals (see plan) | todo | |
| W1–W8 | Window extractor | todo | |
| C1–C5 | Device client | todo | |
| D1–D9 | Device verification | todo | |
| Release | `0.2.0` published | todo | |

## Step log (newest first)

| Date | Step | Result | Log / commit |
|---|---|---|---|
| 2026-11-21 | Plan written and approved; status tracking set up | Done | this commit |

## How to resume

1. `git pull` is not needed (single machine); run `git status --short` and `git log --oneline -10`.
2. Read "Current position" and the last rows of the step log.
3. Re-run the last recorded test command to confirm the state is still green.
4. Continue from "Next action".
