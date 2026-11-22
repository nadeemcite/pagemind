# TODO: finish the device verification of Summarize so far

**Status:** To do (deferred by the owner on 2026-11-22, token budget). Not blocking; `0.2.0` is already released.
**A new session can start from this file.** Plan and gates: `planning/summarize-so-far.md` (Phase 5).

## Why

`0.2.0` was released before D5-D9 were run, and the build that shipped (merged with upstream `develop`)
had never run on a reader. Only D1-D4 were checked, by the owner, on the build before the merge.

## First step

Install `0.2.0` on the reader (Settings -> Check for updates), confirm it boots, About shows
`PageMind 0.2.0`, and Ask AI and Summarize so far both still answer. Anything wrong ships as `0.2.1`.
Before any USB flash: back up `/.crosspoint` and `/My Clippings.txt` (memory note `sd-backup-scope`).

## Checks still to run

| ID | Check | Expected | How to record |
|---|---|---|---|
| Merge | Upstream merge did not break reading, Indic text, dictionary, clippings, Ask AI | Normal use | Owner notes; serial log |
| D2 | Chapter start, then chapter end | Both answer; log shows a short before / empty after | Serial log (the old log was lost) |
| D3 | One chosen spoiler trap: stop on the page just before a well-known twist | Twist not in the recap | Owner note: book, page, answer |
| D5 | Hindi book; English book answered in Hindi | Correct Devanagari shaping, readable paging | Owner screenshots or notes |
| D6 | Wi-Fi off; wrong key; proxy down; proxy older than 0.2.0 (needs a test proxy or a temporary old revision); rate limit | Clear message with Retry, reader never freezes. Old proxy shows "Your AI server is too old..." | Serial log + notes |
| D7 | All 4 orientations; buttons, remapped buttons, touch | Menu item, Wi-Fi flow and answer usable; Back returns to the same page | Owner notes |
| D8 | 20 summaries in a row (about 25 s each) | No heap growth (internal and PSRAM), Wi-Fi off afterwards, normal auto-sleep, reading position intact | `[MEM]` lines in the serial log |
| D9 | Server request log after the runs | Sizes sent match the README privacy note (about 200 words each side, same chapter) | `gcloud run services logs read ai-proxy --region us-central1 --project nadeem-personal` |
| Page walk | Extra heap and time of the 7 page loads | Expect under 10 KB transient, under 100 ms; unverified | Serial log timing around `Summarize:` |

## Tools

- Serial logger that reconnects after the reader restarts itself: `planning/summarize-so-far-logs/phase-5/serial_log.py`
  (`.venv/bin/python <script> /dev/cu.usbmodem101 <out.txt> 1500`; pyserial is in `.venv`).
- The reader leaves USB while Wi-Fi flows restart it; replug if the port disappears.
- Save logs in `planning/summarize-so-far-logs/phase-5/` and update `STATUS.md` after each check.

## Also from this feature (parked elsewhere)

- Prompt refinement and latency (22.7 s per ask on the device): `planning/todo/summarize-prompt-refinement.md`.
- Verify the README claim "roughly one answer in thirty" against device use, or soften it.

## Done when

D2, D3, D5-D9 and the merge check are recorded in `STATUS.md` with logs, or the owner accepts a stated gap.
