# Feature: Summarize so far (spoiler-free recap for popular books)

**Status:** Released as 0.2.0 on 2026-11-22 (D5-D9 outstanding). Five phases, each with tests and a gate.

**Progress and logs:** [summarize-so-far-logs/STATUS.md](summarize-so-far-logs/STATUS.md) holds the
current position, gate checklist and step log; raw run output goes in `summarize-so-far-logs/phase-N/`.
It is updated and committed after every step so work can be reviewed or resumed in a new session.

## Goal

While reading a well-known book, the reader opens the reader menu, chooses
**Summarize so far**, and gets a short recap: what has happened up to the
current page, plus a brief that helps them understand the book (who is who, the
setting, the situation). **It must not reveal anything that happens after the
current page.** The feature only works for popular books, because the model
answers from its own knowledge of the book. For a book it does not recognise,
it says so instead of guessing.

## What is sent (and what is not)

The device sends:

| Field | Content |
|---|---|
| `book_title`, `book_author` | From the EPUB metadata |
| `chapter` | TOC title of the current chapter; if none, `Chapter N` (spine position); if neither, empty |
| `text_before` | Up to 200 words ending at the end of the current page |
| `text_after` | Up to 200 words that follow the current page |
| `language` | Answer language from settings (as today) |

**Trimming rule.** Both windows stay inside the current chapter. At the start of a chapter
`text_before` is short or empty; near its end `text_after` is short or empty. The window is
never padded from another chapter. (Chapter files are the unit the reader already loads, so
this keeps the cost bounded.)

**The reading position is the end of the current page** (the reader has read everything on it).
`text_after` is unread text. It is sent only so the model can locate the scene. The prompt
tells the model to summarize only up to the boundary and never to use `text_after` as story.
This is the main risk of the feature, so the evals test it hard (Phase 2). Because unread
text leaves the device, the privacy note in the README must say so.

## Decisions (2026-11-21, before the work)

- **New task, same contract version.** Add task `summarize` to API v1; this is additive, so
  existing devices and fixtures are unaffected. New optional request fields: `chapter`,
  `text_before`, `text_after`. For `summarize`, `text_before` is required and `text` is not.
  `GET /v1/info` already lists `tasks`; the device checks for `summarize` and shows "Update
  your AI proxy" if it is missing.
- **Not a selection action.** It needs no selected text, so it lives in the reader menu
  (`EpubReaderMenuActivity`, new `MenuAction::SUMMARIZE`), shown only when Ask AI is
  configured. The network, answer and error flow is reused from `AiAskActivity`.
- **Unknown books.** The model is told to reply with the single token `UNKNOWN_BOOK` when it
  does not know the book well enough to describe its plot. The server turns that into a fixed
  plain message ("This book is not well known enough for a summary."), HTTP 200, so the device
  shows it like any answer. No guessing, no invented plot.
- **Limits.** Words are counted as whitespace-separated tokens. Server caps: `text_before` and
  `text_after` 3000 chars each, `chapter` 200. Answer: about 180 words, plain text, in short
  paragraphs with no Markdown. Output budget for this task is 900 tokens, because the router
  model spends part of it on hidden reasoning (a 400-token cap risks an empty or cut answer).
- **Languages.** Books and answers in English and Hindi are in scope for tests. Scripts with
  no spaces between words (Chinese, Japanese) are out of scope; the extractor falls back to a
  character count and is not tested.
- **Version.** Ships as `0.2.0` (new feature).

## Phases

| # | Phase | Output |
|---|---|---|
| 1 | Contract and server | `summarize` task: schema, validation, prompt, `UNKNOWN_BOOK` handling, fixtures, unit tests |
| 2 | Evals and prompt tuning | Golden-book eval set with spoiler, unknown-book, edge and Hindi cases; thresholds met on the live router |
| 3 | Reading-window extractor (device) | Pure C++ `ai::summaryWindow`, host-tested; spike on reading chapter text from the section cache |
| 4 | Device client and screens | Protocol fields, menu item, flow and answer screen; builds and host tests pass |
| 5 | Real-device verification and release | Owner device matrix, docs, `0.2.0` published |

### Phase 1: Contract and server

Work: `contract/ask-request.schema.json` (new task and fields), `app/main.py` (`AskRequest`
validation, per-task output budget), `app/prompts.py` (system prompt for summarize: spoiler
rule, boundary rule, unknown-book rule, no Markdown), `UNKNOWN_BOOK` mapping, contract README,
fixtures for requests and responses (English, Hindi, start of chapter, end of chapter, unknown
book). The existing system prompt says "only use the text you are given"; summarize needs its
own system prompt because it deliberately uses the model's knowledge of the book.

Gate (all automated, `pytest`):

| ID | Check |
|---|---|
| S1 | Schema accepts every new request fixture and rejects: summarize without `text_before`, oversize fields, unknown fields |
| S2 | Existing 46 tests and old fixtures still pass unchanged |
| S3 | Prompt builder: includes title, author, chapter, both windows, the boundary marker; omits empty parts; start-of-chapter (empty before) and end-of-chapter (empty after) both build |
| S4 | Fake provider returns `UNKNOWN_BOOK` → fixed message, HTTP 200, not logged as a provider error |
| S5 | Summarize uses its own output budget; other tasks keep 400 |
| S6 | `/v1/info` lists `summarize`; limits for the new fields are reported |

### Phase 2: Evals and prompt tuning

Work: extend `evals/` with `summarize_cases.jsonl` and scoring in `run_evals.py`. Text windows
for the cases are short excerpts written or paraphrased by us from public-domain books, or
small quoted passages (a few hundred words each); no copyrighted book text is stored.

Case groups (about 40 cases):

- **Known, early / middle / late position** in about 8 public-domain and popular books
  (Pride and Prejudice, A Tale of Two Cities, Moby-Dick, Frankenstein, Great Expectations, Alice,
  Sherlock Holmes stories, one Hindi classic such as Godaan or Nirmala; copyrighted
  popular books are tested with a short paraphrased window, not their text).
- **Spoiler traps.** Each case has a `forbidden` list of later plot facts and names that must not
  appear (for example, a death or a reveal that happens only after the position). A second
  variant puts a misleading hint in `text_after` and checks it is not repeated as past events.
- **Unknown books.** Invented titles and authors, and an obscure real self-published title: must
  return the unknown-book message, with no plot.
- **Edges.** Chapter start (no `text_before`), chapter end (no `text_after`), chapter name missing.
- **Language.** Hindi answer for an English book; Hindi book answered in Hindi.
- **Format.** Plain text, length at most 200 words, no Markdown or emoji (existing checks).

Scoring: rule checks (forbidden terms, must-contain character or plot-so-far terms, refusal
text for unknown, format, length) plus an LLM judge (the same router, a different model if
available) that answers "does the summary state any event that happens after the position?"
for each case, with the judge's verdict stored in the report. The judge is advisory; the
rule checks decide pass or fail, and judge disagreements are reviewed by hand.

Gate (run against the live router, report committed to `evals/reports/`):

| ID | Check | Threshold |
|---|---|---|
| E1 | Spoiler traps | 0 forbidden terms in all runs; at least 95% of judged cases clean |
| E2 | Unknown books | 100% refuse; no plot invented |
| E3 | Known books useful | at least 90% mention the correct characters or events so far |
| E4 | Format and length | 100% plain text, at most 200 words |
| E5 | Edge cases | all pass |
| E6 | Latency | p95 at most 20 s (the router model reasons; Phase 2 measured about 8 s) |
| E7 | Truncated or empty | 0 |
| E8 | Stability | the full set is run 3 times; same thresholds each time |

If E1 or E2 cannot be met, the prompt is revised; if that fails, the decision to send
`text_after` is revisited with the owner (a fallback is to send only `text_before`).

### Phase 3: Reading-window extractor (device)

Work: `src/ai/SummaryWindow.{h,cpp}` (pure functions, no hardware): given a chapter's text as
a word list or string and the word index where the current page ends, return `before` (last
200 words up to the boundary) and `after` (next 200 words), by words, UTF-8 safe, clipped to
the server limits. Spike first: how to read the chapter text around the current page without
parsing the whole chapter into RAM. Candidates: load neighbouring pages from the section
cache (`Section::loadPage`, as `getTextFromSectionFile` does for one page), walk pages
backwards and forwards until 200 words are gathered or the chapter ends. Record the result and
peak heap in this file. Chapter title from `Epub::getTocIndexForSpineIndex` / `getTocItem`.

Gate (host tests, `test/ai_summary_window`):

| ID | Check |
|---|---|
| W1 | Mid-chapter: exactly 200 words each side, boundary exact |
| W2 | Chapter start: `before` shorter or empty, `after` full; chapter end: the reverse |
| W3 | Chapter of fewer than 400 words: all words used, no duplicates, no overlap |
| W4 | Hindi and mixed-script text: split on spaces, no broken UTF-8 or combining marks |
| W5 | Words never cross a chapter boundary |
| W6 | Output within the server limits (3000 chars each), cut on a word boundary |
| W7 | Chapter name: TOC title, else `Chapter N`, else empty |
| W8 | Spike note recorded: method, extra heap, time on device |

### Phase 4: Device client and screens

Work: `ai::Task::Summarize` and the new fields in `AiProtocol` (`buildAskBody`, limits from
`/v1/info`); a "Summarize so far" menu item (`EpubReaderMenuActivity`, `StrId` in
`english.yaml`; other languages fall back to English until translated); an activity that reuses
the `AiAskActivity` Wi-Fi, ask, retry and answer flow, skipping the task picker; a
"proxy too old" error. Keep the reader hooks small and put the logic in new files. Update the
PageMind dev notes for the new files.

Gate:

| ID | Check |
|---|---|
| C1 | Host tests: request body for summarize matches the contract fixtures byte for byte; old requests unchanged |
| C2 | Host tests: response parsing, unknown-book answer shown as an answer, missing task in `/v1/info` gives the "update proxy" error |
| C3 | Host tests: menu item hidden when AI is off or not configured, shown when ready |
| C4 | `pio run -e pagemind_x4pro` builds; all host tests pass; clang-format clean |
| C5 | Contract check in CI: the device fixtures and `contract/` schema stay in sync (extends the existing contract test) |

### Phase 5: Real-device verification and release

The agent flashes the connected device and drives it through the demo command channel and
serial log; the owner checks screens.

| ID | Check | Expected |
|---|---|---|
| D1 | Popular English book, middle of a chapter | Recap of the story so far, no later events, sensible brief |
| D2 | Start of a chapter, then the end of a chapter | Both work; short windows are logged and sent |
| D3 | Same spot, spoiler trap (a page just before a known plot twist) | Twist not mentioned |
| D4 | Unknown or personal book | Fixed "not well known" message |
| D5 | Hindi book and English book with Hindi answer | Correct Devanagari shaping, readable paging |
| D6 | Errors: Wi-Fi off, wrong key, server down, old proxy | Clear message with Retry; reader never freezes |
| D7 | Orientation and input | All four orientations; buttons, remapped buttons, touch; Back returns to the same page |
| D8 | Memory and power | 20 summaries: no heap growth, Wi-Fi off afterwards, normal auto-sleep; reading position intact |
| D9 | Request log | Server log shows the sizes sent; payload matches the privacy note |

Release: README and `FORK.md` (what it does, popular books only, that unread text is sent, cost),
proxy README, contract docs, `0.2.0` tag through the release pipeline, owner sign-off.

## Gate record

| Phase | Date | Commit | Result | Notes |
|---|---|---|---|---|
| 1 | | | | |
| 2 | 2026-11-22 | see git log | Pass (accepted by owner) | Prompt v2 on `ag/gemini-3.8-flash`; judge `ag/claude-sonnet-4-6`. Final config run 1: 35/35, run 2: 34/35 (router 502), both GATE PASS (E1-E7); run 3 stopped early. Baseline v1 had 11/29 judge flags, v2 about 1-2/29. Prompt refinement deferred: see STATUS.md "To do later". Logs: `summarize-so-far-logs/phase-2/`; reports: `cloud/ai-proxy/evals/reports/` |
| 3 | 2026-11-22 | see git log | Pass | W1-W8: 19 window tests; full host suite 1072/1072; clang-format clean. Walk capped at 12 pages per side. Device heap/time to be measured in Phase 5 (D8). A Phase 1 side effect (summarize fixtures break `AiClientTest`) was found and parked as a marked skip for Phase 4 |
| 4 | 2026-11-22 | see git log | Pass | C1-C5: host 1081/1081, proxy 92/92, firmware builds (RAM 34.1%, flash 84.9%), clang-format clean. Not yet run on the device (Phase 5) |
| 5 | 2026-11-22 | 0.2.0 tag | Released, device checks partial | D1-D4 pass (owner); D5-D9 not run, accepted by owner to release now. Merged upstream `develop` first |
| Owner sign-off | 2026-11-22 | 0.2.0 tag | Released | Owner asked to complete and release; install-over-Wi-Fi is the first run of the merged build |
