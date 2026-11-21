# TODO: refine the Summarize-so-far prompt

**Status:** To do (deferred by the owner on 2026-11-22 to save tokens). Not blocking Phases 3-5.
**Owner picks this up when token quota is back.** A new session can start from this file.

## Why

The Phase 2 gate passed (runs: 35/35 and 34/35), but the model still leaks small facts from later
in the same chapter in about 1-2 of 29 judged answers per run. Phase 2 history is in
`planning/summarize-so-far-logs/STATUS.md` and the reports in `cloud/ai-proxy/evals/reports/`
(the `baseline/` folder holds the older runs).

## Current state

- Prompt v2: `SUMMARIZE_SYSTEM_PROMPT` in `cloud/ai-proxy/app/prompts.py` (exact boundary, only-met-facts rule,
  self-check, stop-early rule).
- Live: Cloud Run revision `ai-proxy-00009-szv`, model `ag/gemini-3.8-flash` on 9router.
- Judge: `ag/claude-sonnet-4-6` through the same router (advisory, run with `--judge-model`).
- Cases: `cloud/ai-proxy/evals/summarize_cases.jsonl` (35 cases). Runner: `evals/run_summarize_evals.py`.

## Known leaking cases to start from

| Case | What leaked |
|---|---|
| `tale-mail` | Detail from later in the same chapter (the instruction to wait at Dover) |
| `alice-ch1` | Scene that comes after the position (landing in the hallway) |
| `hp-zoo` | Flagged in run 2 (reason not recorded; read the report) |
| `hobbit-ch1` | Judge flagged an inaccurate detail (Thorin arrives last); possibly not a spoiler |
| `trap-hp-wand` | One 502 (router timeout, 46 s) in run 2 |

## Ideas to try (one at a time, record each run)

1. Send the model only `text_before` for the recap and use `text_after` only to confirm the position (or drop
   `text_after`; compare leaks and "wrong place" errors).
2. Two-step answer: draft, then a second pass that deletes sentences past the boundary.
3. A stronger model for this task (check `GET /v1/models` on the router first: several listed models return 404).
4. Shorter recap target (fewer words means fewer chances to go past the boundary).
5. Make judge flags a blocking gate (fail the run on any flag, not on a 95% rate), and add more trap cases per book.
6. Provider retry and a longer timeout for the `summarize` task (the 502 in run 2).
7. Move prompt text into a versioned file and keep each eval report next to the prompt version it tested.

## Latency seen on the device (2026-11-22)

D1 on the reader: `/v1/info` 11.5 s (Cloud Run cold start plus TLS) and `/v1/ask` 22.7 s (2 s TLS plus the
model's reasoning). It works but feels slow. Ideas: cache the `/v1/info` result for the session, skip it for
summarize (the limits are the defaults), a faster model, or lower reasoning effort (`ag/gemini-3.8-flash-low`).

## How to run the evals

```bash
cd cloud/ai-proxy
export AI_PROXY_KEY=...        # a device key (from Secret Manager `ai-proxy-device-keys`); never print or commit it
export JUDGE_API_KEY=...       # 9router key (Secret Manager `9router_api_key` in project 597120914452)
export JUDGE_BASE_URL=https://9router.devnadeem.com/v1/
.venv/bin/python -m evals.run_summarize_evals --url <proxy url> --judge-model ag/claude-sonnet-4-6 --runs 3
# deploy a changed prompt first:
./deploy.sh --project nadeem-personal --provider openai --base-url https://9router.devnadeem.com/v1/ \
  --model ag/gemini-3.8-flash --key-secret 9router_api_key --yes
```

## Done when

Three runs in a row with zero judge flags and no forbidden terms, or the owner accepts a stated rate.
Record the result in `STATUS.md` (step log) and save the logs in `planning/summarize-so-far-logs/`.
