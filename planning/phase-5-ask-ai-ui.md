# Phase 5: Ask AI user experience and public release

**Status:** Not started

## Goal

In any EPUB, the reader selects text, chooses **Ask AI**, picks a task, waits
briefly, and reads a well-rendered answer, including Hindi and other Indic
scripts. Then the fork gets its first public release with documentation.

## Entry criteria

- Phase 4 complete (`AiClient::ask()` works on the device).

## Work

1. **Entry point in the selection screen**
   - `src/activities/reader/ClipSelectionActivity` already offers actions (Clip, Lookup,
     Bookmark) and returns a `ClippingResult` with the selected `text`
     (`src/activities/ActivityResult.h`). Add an `AskAi` action, shown only when AI is
     enabled and configured.
   - `src/activities/reader/EpubReaderActivity.cpp` already handles
     `ClippingResult::Action::Lookup` (around line 551); add an `AskAi` branch next to it
     that starts the Ask flow. Keep the hook small; put the logic in new files.
   - Note: PR #3787 also changes `ClipSelectionActivity.cpp`; check this when merging
     upstream.
2. **Task picker**: Explain / Define / Translate / Ask a question (the last opens the
   on-screen keyboard, `KeyboardEntryActivity`).
3. **Context**: send the selected text, about 1–2 nearby paragraphs as `context`,
   book title and author, and the answer language from settings. Never send text
   beyond the current page (no spoilers).
4. **Answer screen** (`src/ai/AiAnswerActivity.{h,cpp}`, new)
   - Modeled on `DictionaryDefinitionActivity` (paged plain text, back to the book).
   - States: connecting Wi-Fi → asking → answer / error with Retry.
   - Uses the reader font so Indic answers are shaped (PR #3787).
   - Option to save the answer as a clipping/note (stretch goal, decided during the phase).
5. **Input and layout**
   - Buttons, touch, and remapped buttons; all four orientations
     (`.agents/rules/ui-activities.md`, input verification matrix).
6. **Public release**
   - `FORK.md` / README: what Ask AI does, BYOC setup guide (Phase 2 `deploy.sh`),
     privacy note (selected text goes to your own server and your provider), cost note.
   - Release `v0.1.0` through the Phase 1 pipeline with release notes.

## Tests and evals

### Automated (agent)

| ID | Check | How |
|---|---|---|
| A1 | Context extraction | Host test: context comes only from the current and previous text, stays within limits, never cuts a UTF-8 character |
| A2 | Action visibility | Host test: Ask AI hidden when AI is disabled or not configured |
| A3 | Answer pagination | Host test: long English and Hindi answers split into pages without losing or repeating text |
| A4 | Builds and existing tests | `pio run -e x4pro`; all host tests pass; clang-format clean |

### Device (owner): end-to-end matrix

| ID | Check | Expected |
|---|---|---|
| D1 | English book: Explain, Define, Translate (to Hindi), Ask | Correct answers; Back returns to the same page |
| D2 | Hindi book: same four tasks | Answers render with correct Devanagari shaping |
| D3 | Orientation | All 4 orientations: selection, picker, answer screen usable |
| D4 | Input | Buttons, remapped buttons, touch: no double actions, no stuck screens |
| D5 | Errors | Wi-Fi off, wrong key, server down, rate limit: clear message and Retry; reader never freezes |
| D6 | Memory and power | 20 Ask AI rounds: no heap growth (internal and PSRAM), Wi-Fi off afterwards, normal auto-sleep |
| D7 | Fresh-user eval | A second person follows only the docs: deploys the proxy with `deploy.sh`, flashes `v0.1.0`, configures the device, and asks a question. Target: done in under 30 minutes with no help |

### Final eval: real-reading session

Read for 30 minutes with 10 Ask AI uses across English and Hindi books. Record for
each: task, wait time, answer useful (yes/no). Target: at least 8 of 10 useful, no
crash, no lost reading position.

## Gate

The phase is complete when A1–A4, D1–D7 and the final eval pass and are recorded
below, and `v0.1.0` is published.

## Gate record

| Check | Date | Commit | Result | Log / notes |
|---|---|---|---|---|
| A1 | | | | |
| A2 | | | | |
| A3 | | | | |
| A4 | | | | |
| D1 | | | | |
| D2 | | | | |
| D3 | | | | |
| D4 | | | | |
| D5 | | | | |
| D6 | | | | |
| D7 | | | | |
| Final eval | | | | |
| `v0.1.0` published | | | | |
| Owner sign-off | | | | |
