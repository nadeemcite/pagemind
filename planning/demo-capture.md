# Demo capture tooling

**Status:** In progress

## Goal

Make the screenshots and a short video for the README and for sharing,
**without manual screenshots**: one command drives a real X4 Pro through a
scripted Ask AI demo and saves presentable images and a captioned video.

## Why not an emulator

CrossPoint has no simulator (no host build target; CrossInk's simulator is tied to
CrossInk's code). Driving the real device gives real screens with less work.

## Design

### 1. Demo build with serial commands (firmware)

A separate build target, **`pagemind_x4pro_demo`** (in `platformio.fork.ini`), sets
`-DPAGEMIND_DEMO=1`. Only that build compiles the demo commands, so release and
normal development firmware never contain them. Code in new files
(`src/demo/DemoCommands.{h,cpp}`); `src/main.cpp` gets one hook next to the existing
`CMD:SCREENSHOT` handler.

| Command | Does | Reply |
|---|---|---|
| `CMD:OPEN <sd path>` | Opens a book (EPUB) at its saved position | `OK OPEN` / `ERR …` |
| `CMD:PAGE <+n/-n>` | Turns pages forward or back | `OK PAGE <spine> <page>` |
| `CMD:SELECT <first> <last>` | Opens the selection screen on the current page with words `first..last` selected (uses `ClipSelectionActivity::setInitialSelection`), so the **Ask AI** button shows | `OK SELECT` |
| `CMD:ASK <task> [text]` | Starts Ask AI for the current selection, or for `text`, with the task chosen (skips the picker) | `OK ASK` when the answer or an error is on screen |
| `CMD:PICKER` | Shows the task picker for the current selection | `OK PICKER` |
| `CMD:BACK` | Same as pressing Back | `OK BACK` |
| `CMD:WAIT` | Replies when rendering and any request are finished | `OK WAIT` |
| `CMD:SCREENSHOT` | Existing command: sends the framebuffer | binary frame |

Replies let the script wait for each step instead of guessing with timers.
Book text in commands is public-domain demo text only.

### 2. Storyboard file

`demo/storyboard.yaml` lists the scenes, for example:

```yaml
books:
  english: /Books/A Tale of Two Cities.epub
  hindi:   /Books/Panch Parmeshwar.epub
scenes:
  - name: page
    steps: [{open: english}, {page: +5}, {shot: 01-page, caption: "Reading as usual"}]
  - name: select
    steps: [{select: [12, 30]}, {shot: 02-select, caption: "Select a passage, tap Ask AI"}]
  - name: picker
    steps: [{picker: true}, {shot: 03-picker, caption: "Choose what you want"}]
  - name: explain
    steps: [{ask: explain}, {shot: 04-answer, caption: "The answer, on e-ink"}, {back: true}]
  - name: hindi
    steps: [{open: hindi}, {select: [4, 18]}, {ask: translate}, {shot: 05-hindi}]
```

Books are public-domain EPUBs (for example from Project Gutenberg) that the owner
copies to the SD card once.

### 3. Capture script

`scripts/demo_capture.py storyboard.yaml --port auto --out demo/out`:

1. Connects over USB serial, runs each step, waits for each `OK`.
2. Saves each screenshot as PNG, rotated to the reader's orientation (the
   framebuffer is in panel orientation; same rule as `src/util/ScreenshotUtil.cpp`).
3. **Presentable output** (Pillow): each screen inside a simple device frame
   (rounded bezel, soft shadow) on a light background; composites for the README
   slots: hero (page + answer side by side), 4-step strip, Hindi answer.
4. **Video** (ffmpeg, installed): the framed scenes in order with captions and
   short cross-fades → `demo/out/pagemind-demo.mp4` (about 45–60 s) and a small
   `pagemind-demo.gif` for the README.
5. Writes `demo/out/manifest.json` (scene, file, caption) for re-runs.

`deploy.sh`'s terminal scene is rendered from a recorded text transcript
(no secrets) as a terminal-styled image.

### 4. README

The four screenshot slots in `README.md` are filled from `demo/out/`; the images
live in `docs/images/pagemind/` (sized for the web, a few hundred KB in total).

## Implementation notes

- Demo books: *A Tale of Two Cities* (Project Gutenberg) and Premchand's Hindi
  stories (public domain, from the CrossInk-Indic test set), kept in `demo/books/`
  (git-ignored) and copied to `/PageMind Demo/` on the SD card. The owner's own
  books are copyrighted and are not used.
- `STATE` (current screen name) replaces `WAIT`: the script waits for the expected
  screen, then for the e-ink refresh; `SHOT` holds the render lock while sending.

## Changes after owner review (2026-11-15)

- **Tap markers:** `CMD:WHERE askai` / `WHERE option <n>` return a control's screen
  rectangle (selection action bar, popup interaction table); the storyboard's
  `mark` step draws an orange tap marker there, and the video shows each tapped
  screen first without, then with the marker.
- **Languages, not only Hindi:** scenes "Read in any language" (Hindi page),
  "Translated into yours" (Hindi → English) and "Answers in the language you
  choose" (Dickens explained in Spanish); README composite `languages`.
- `--rerender` redraws framed images, composites and video from the last run.

## Work order

1. Demo build target and `DemoCommands` (OPEN, PAGE, BACK, WAIT, SCREENSHOT reuse).
2. SELECT, PICKER and ASK (needs small hooks in `EpubReaderActivity` and
   `AiAskActivity` to preset the selection and task).
3. `demo_capture.py`: serial driver, PNG conversion, device frame, composites.
4. ffmpeg video and GIF.
5. Storyboard with real scenes; fill the README slots.

## Tests and evals

### Automated (agent)

| ID | Check | How |
|---|---|---|
| A1 | Framebuffer → PNG | Host test of the converter with synthetic frames in all 4 orientations (known pixel lands in the right corner) |
| A2 | Storyboard validation | Unknown step, missing book, bad word range → clear error before touching the device |
| A3 | Demo commands never ship | `pagemind_x4pro_release` and `pagemind_x4pro` firmware contain no `CMD:ASK` / `CMD:SELECT` strings; demo build does |
| A4 | Builds and existing tests | Demo, dev and release builds; all host tests; clang-format; existing `CMD:SCREENSHOT` unchanged |

### Device (owner)

| ID | Check | Expected |
|---|---|---|
| D1 | Full storyboard run | Every scene completes unattended; images in the right orientation |
| D2 | Re-run | Same scenes again give equivalent images (answers may word differently) |
| D3 | Output quality | Owner approves the framed screenshots, composites, video and GIF |
| D4 | Back to release | After the session, update or flash back to the release build; About shows the release version |

## Gate

Complete when A1–A4 and D1–D4 pass and the README slots are filled.

## Gate record

| Check | Date | Commit | Result | Log / notes |
|---|---|---|---|---|
| A1 | 2026-11-15 | see git log | Pass | `scripts/demo/test_demo.py`: random framebuffers match the `ScreenshotUtil.cpp` pixel loop in all 4 orientations; short buffer rejected |
| A2 | 2026-11-15 | see git log | Pass | 8 bad storyboards (no scenes, unknown book/step, bad range/task, two actions, duplicate name, unknown composite shot) rejected before connecting |
| A3 | 2026-11-15 | see git log | Pass | Demo strings: demo build 3, `pagemind_x4pro` 0, `pagemind_x4pro_release` 0 |
| A4 | 2026-11-15 | see git log | Pass | Demo, dev and release builds succeed; host tests 1053/1053; clang-format clean; `CMD:SCREENSHOT` path untouched (demo handler runs first only in demo builds) |
| D1 | 2026-11-15 | dcb092a2 | Pass | Run 3: all 7 scenes unattended (Dickens chapter I: reading, selection with Ask AI button, picker, Explain, Define "incredulity"; Premchand page, Hindi → English translation); correct orientation; composites, 25 s MP4 and GIF made. Runs 1–2 found wrong pages and Hindi answers; fixed with GOTO and LANG |
| D2 | 2026-11-15 | see git log | Pass | Run 4 (tap markers, language scenes) repeated every scene unattended with the same screens; answers worded differently as expected; 30 s video |
| D3 | | | | |
| D4 | | | | |
| Owner sign-off | | | | |
