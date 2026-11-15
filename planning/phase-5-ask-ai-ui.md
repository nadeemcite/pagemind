# Phase 5: Ask AI user experience and public release

**Status:** In progress

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

## Decisions (2026-11-15, before the work)

- **Entry point:** a fourth button **Ask AI** in the selection action bar (after
  Look up, Clip, Bookmark), shown only in Clip mode when AI is configured. The
  X4 Pro is a touch board, where this bar is how actions are chosen. Button-only
  boards reach the bar's other actions through Confirm; Ask AI for them is left
  for later (X4 Pro is the only release target).
- **Flow:** `src/activities/reader/AiAskActivity`: task picker (OptionPopup:
  Explain, Define, Translate, Ask a question → keyboard), Wi-Fi if needed,
  "Asking AI…", then the answer in the existing `DictionaryDefinitionActivity`
  plain-text viewer (paged, reader font, Indic shaping). Errors show the Phase 4
  messages with Retry / Back.
- **Context:** the current page's logical text (`Section::getTextFromSectionFile`,
  which returns `wordText()`, not PR #3787's shaped display form), windowed around
  the selection by `ai::contextAround` and capped at the server limit. Only the
  page on screen is sent, so there are no spoilers.
- **Answer language:** the AI Assistant setting, or the device UI language name
  when it is empty.
- **Wi-Fi lifetime:** like `KOReaderSyncActivity`: if this flow turned Wi-Fi on,
  leaving it disconnects and does `silentRestartToReader()`, reopening the book at
  the same place and returning network heap. If Wi-Fi was already on, nothing
  restarts.
- **Checks adjusted:** A2 (button visibility) becomes code review plus device
  check, because `ClipSelectionActivity` cannot run on the host. A3 (answer
  pagination) is covered by the existing upstream viewer; device check D1/D2
  verify long English and Hindi answers.

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
| A1 | 2026-11-15 | 9b71480c | Pass | `AiClientTest` context cases 6/6: short page whole, window holds the selection and stays within the limit, page start/end, whitespace differences, not-found fallback, Hindi never split |
| A2 | | | | |
| A3 | | | | |
| A4 | | | | |
| D1 | 2026-11-15 | 78ffc363 (0.0.3-dev) | Pass | Owner: English book, Explain/Define/Translate/Ask a question all answer; Back returns to the page. Server log has all four tasks. Context check: page 6 at 18 pt = 16 lines, 71 words, 407 B, sent whole as context |
| D2 | 2026-11-15 | 78ffc363 (0.0.3-dev) | Pass | Owner: Hindi book, all four tasks answer; Devanagari renders correctly in the answer viewer |
| D3 | 2026-11-15 | 78ffc363 (0.0.3-dev) | Pass | Owner: selection, task picker and answer screen work in all 4 orientations |
| D4 | | | | |
| D5 | | | | |
| D6 | | | | |
| D7 | | | | |
| Final eval | | | | |
| `v0.1.0` published | | | | |
| Owner sign-off | | | | |
