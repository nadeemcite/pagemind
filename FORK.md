# PageMind

PageMind is a custom firmware for the Xteink X4 Pro, based on
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).
It is **not** an official CrossPoint release: please report PageMind problems
in this repository, not to the CrossPoint project.

## What PageMind adds

| Addition | Where | Status |
|---|---|---|
| Indic script shaping (upstream PR [#3787](https://github.com/crosspoint-reader/crosspoint-reader/pull/3787)) | merged into `personal` | merged |
| Own name, releases and OTA updates | `src/fork/`, `platformio.fork.ini`, `.github/workflows/fork-release.yml` | phase 1 |
| AI proxy server (Cloud Run) | [`cloud/ai-proxy/`](cloud/ai-proxy/README.md) | ready to deploy |
| "Ask AI" on selected text (firmware) | [`planning/`](planning/README.md) | planned (phases 3–5) |

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
