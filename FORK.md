# PageMind

PageMind is a custom firmware for the Xteink X4 Pro, based on
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).
It adds **Ask AI**: select a passage while reading and get it explained, a word
defined in context, a translation, or an answer to your own question, on the
e-reader, from an AI server that you run yourself.
It is **not** an official CrossPoint release: please report PageMind problems
in this repository, not to the CrossPoint project.

## What PageMind adds

| Addition | Where | Status |
|---|---|---|
| Indic script shaping (upstream PR [#3787](https://github.com/crosspoint-reader/crosspoint-reader/pull/3787)) | merged into `personal` | merged |
| Own name, releases and OTA updates | `src/fork/`, `platformio.fork.ini`, `.github/workflows/fork-release.yml` | phase 1 |
| Ask AI on selected text | `src/ai/`, `src/activities/reader/AiAskActivity.*` | done |
| AI Assistant settings, encrypted API key, SD-card setup file | `src/ai/`, Settings → Network → AI Assistant | done |
| Your own AI server on Google Cloud Run (OpenAI or Ollama) | [`cloud/ai-proxy/`](cloud/ai-proxy/README.md) | done |

The implementation plan, with its five phases and test gates, is in [`planning/`](planning/README.md).

## Install

Supported device: **Xteink X4 Pro** only, for now.

1. Back up the SD card's `/.crosspoint/` folder (reading progress, settings, bookmarks).
2. Download `pagemind-<version>-x4pro.bin` from
   [Releases](https://github.com/nadeemcite/pagemind/releases).
3. Connect the X4 Pro by USB-C and wake it.
4. Flash it with one of:
   - the web flasher at https://crosspointreader.com/#flash-tools → select X4 Pro → "Custom .bin"
   - `esptool.py --chip esp32s3 --port <port> --baud 921600 write_flash 0x10000 pagemind-<version>-x4pro.bin`

Books, fonts, settings and reading progress on the SD card are kept.

> **USB-locked devices:** some units bought from third-party stores have USB flashing
> locked. CrossPoint's unlock tool officially supports only CrossPoint and CrossInk;
> flashing other firmware on a locked device may leave it stuck or bricked. PageMind
> keeps OTA updates working, but if your device is locked, read
> [CrossPoint's warning](https://github.com/crosspoint-reader/crosspoint-reader#usb-locked-devices-xteink-unlocker)
> first and proceed at your own risk.

## Ask AI

**Using it:** in a book, hold a word and drag to select a passage, then tap
**Ask AI** in the action bar and pick a task:

| Task | What you get |
|---|---|
| Explain | The passage in simple words |
| Define | The meaning of a word or phrase as used here |
| Translate | The passage in your answer language |
| Ask a question | Type a question about the passage |

The reader joins Wi-Fi if needed, shows the answer in a paged view in your reading
font (Indic scripts included), and **Back** returns you to the same page. If Ask AI
had to turn Wi-Fi on, the book reopens after a short restart on the way back.

**Setting it up** (once):

1. **Deploy your own AI server** to Google Cloud Run with
   [`cloud/ai-proxy/deploy.sh`](cloud/ai-proxy/README.md#deploy-to-google-cloud-run).
   You need a Google Cloud project with billing turned on and an API key from
   OpenAI or Ollama. The script prints the server URL and writes `pagemind-ai.json`.
2. **Give the reader the URL and key**, easiest first:
   - copy `pagemind-ai.json` to the root of the SD card and restart the reader
     (it imports the file, encrypts the key, turns Ask AI on and deletes the file);
   - or start File Transfer, open the reader's address in a browser, and paste the
     URL and key under **Settings → AI Assistant**;
   - or type them on the reader: **Settings → Network → AI Assistant**.
3. **Check it:** Settings → Network → AI Assistant → **Test connection**.

**Answer language:** set it under AI Assistant; empty means the reader's UI language.

**Privacy:** when you use Ask AI, the reader sends the selected text, the text of
the page on screen (never pages ahead, so no spoilers), the book title and author,
and your answer language to **your own** server, which forwards it to the AI
provider you chose (OpenAI or Ollama, under their terms). Nothing goes to the
PageMind project. Your server's logs keep only sizes and timing, never book text or
answers. The API key is stored encrypted with a secret that never leaves the reader.
The reader checks your server's HTTPS certificate before sending anything.

**Cost:** you pay your own Google Cloud and AI provider bills. Cloud Run sleeps
when unused, so occasional use usually fits in or near its free tier; the stored
container images may cost a few cents a month. AI costs depend on your provider and
model. Set a spending limit at your provider and a
budget alert in Google Cloud. Your server allows 10 questions per minute per key.

**Limits:** Xteink X4 Pro only for now; Ask AI needs Wi-Fi; the first question after
a long pause can take a few seconds longer while the server wakes up.

## Updates

**Settings → Check for updates** installs the latest PageMind release from this
repository. It never installs official CrossPoint. To go back to official CrossPoint,
flash it with the web flasher.

## Build from source

```bash
pio run -e pagemind_x4pro                 # development build (version "<v>-dev-<sha>")
pio run -e pagemind_x4pro -t upload       # build and flash over USB
pio run -e pagemind_x4pro_release         # release build, as published
```

The fork version is `[pagemind] version` in `platformio.fork.ini`.

## Making a release

1. Set `version` in `platformio.fork.ini` (numbers only, e.g. `0.1.0`), commit, push.
2. `git tag 0.1.0 && git push origin 0.1.0`
3. `.github/workflows/fork-release.yml` builds `pagemind-0.1.0-x4pro.bin` and
   publishes the release. Devices see it only if it is not marked pre-release.

Upstream release workflows are disabled in this repository's Actions settings.

## Branches and remotes

- `personal` (default branch): upstream `develop` + PR #3787 + PageMind's work.
- Remotes in a local clone:

  ```bash
  git remote add upstream https://github.com/crosspoint-reader/crosspoint-reader.git
  git remote add pr3787   https://github.com/ssafayet/crosspoint-reader.git
  ```

## Syncing with upstream

```bash
git fetch upstream && git fetch pr3787
git checkout personal
git merge upstream/develop
git merge pr3787/indic-language-support-without-harfbuzz   # until #3787 is merged upstream
pio run -e pagemind_x4pro
git push origin personal
```

Keep fork changes in their own files and folders where possible, so these merges
rarely conflict.

## License

MIT, like CrossPoint Reader. CrossPoint's copyright notice is kept in `LICENSE`.
