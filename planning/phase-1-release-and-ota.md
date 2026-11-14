# Phase 1: Fork identity, releases and OTA

**Status:** Complete

## Goal

The fork has its own name, builds release files with GitHub Actions, and the
device's "Check for updates" installs **this fork's** releases, never official
CrossPoint.

## Entry criteria

- Firmware name decided (see Decisions in [README](README.md)).
- `personal` builds for `x4pro` (done: commit `4bfaa025`).

## Work

1. **Identity**
   - Version string shows the fork name, for example `<name>-0.1.0 (CrossPoint 2.0.0)`.
     Version flags are in `platformio.ini` (`[crosspoint] version`, `CROSSPOINT_VERSION`).
   - About screen shows the fork name and a "based on CrossPoint" line
     (`src/activities/settings/AboutActivity.cpp`).
   - Keep upstream's MIT copyright notice; add the fork's own notice.
2. **OTA source**
   - `src/network/OtaUpdater.cpp:22` hardcodes the official releases API URL. Move the
     owner/repo into one build flag (for example `-DFORK_RELEASES_REPO=\"<owner>/<repo>\"`),
     so upstream merges touch one line at most.
   - Asset name pattern (`OtaUpdater.cpp:57`, `crosspoint-<tag><suffix>.bin`) becomes
     `<name>-<tag>-x4pro.bin`. The board-suffix logic stays.
   - Releases must be **pre-release-aware**: decide whether pre-releases are offered
     (default: no).
3. **Release pipeline**
   - New workflow `.github/workflows/fork-release.yml` (new file, so upstream
     workflow changes never conflict): on a numeric tag like `0.1.0`, build `pagemind_x4pro_release`, name the asset to
     match the OTA pattern, publish a GitHub release with notes.
   - Disable upstream release workflows in the fork (they publish CrossPoint names).
4. **Docs**
   - `FORK.md`: install guide (web flasher with custom `.bin`, or `pio`), the
     USB-locked device warning from upstream README, how to update.

## Implementation notes (decided during the phase)

- Version scheme: release tags are plain numbers like `0.1.0` (no `v`), because OTA
  compares `major.minor.patch` numerically and GitHub's `releases/latest` skips
  pre-releases. Test releases are therefore real releases `0.0.1` and `0.0.2`.
- Fork values live in `src/fork/ForkIdentity.h`; build targets in `platformio.fork.ini`
  (`pagemind_x4pro`, `pagemind_x4pro_release`); `scripts/fork_version.py` sets the version.
- Upstream file hooks: `OtaUpdater.cpp` (URL + asset name), `AboutActivity.cpp`,
  `BootActivity.cpp`, `SleepActivity.cpp` (name), `platformio.ini` (`extra_configs`),
  `test/CMakeLists.txt` (one line).
- Name changed from NCrossPoint to **PageMind** (repo `nadeemcite/pagemind`) before any
  device installed a release, so no OTA transition was needed. Checks A1–A5 were rerun.
- A2 uses a new test folder `test/fork_ota` instead of editing the upstream test.

## Tests and evals

### Automated (agent)

| ID | Check | How |
|---|---|---|
| A1 | Firmware builds | `pio run -e pagemind_x4pro_release` succeeds; firmware contains the fork OTA URL and no upstream releases URL |
| A2 | Release JSON parsing finds the fork asset name | Host test `test/fork_ota`: fork release JSON (asset `pagemind-0.0.2-x4pro.bin`) is found; official CrossPoint assets and other boards are ignored |
| A3 | All host tests pass | CMake/CTest command in `test/README` |
| A4 | Release workflow works | Push tag `0.0.1`; workflow publishes release `0.0.1` with `pagemind-0.0.1-x4pro.bin` and its `.sha256`; downloaded file matches the checksum |
| A5 | Formatting | `./bin/clang-format-fix -g` reports no changes |

### Device (owner)

| ID | Check | Expected |
|---|---|---|
| D1 | Flash release `0.0.1` over USB | Boots; About shows fork name and version; books and progress intact |
| D2 | Agent publishes `0.0.2`; then Settings → Check for updates | Offers `0.0.2`, installs, reboots into it |
| D3 | Check for updates again | "Up to date"; never offers an official CrossPoint version |
| D4 | Recovery | Hold Down while powering on (X4 Pro); the SD-card firmware picker opens (recovery mode, `src/main.cpp`), so a `.bin` on the SD card can always be flashed |

## Gate

The phase is complete when A1–A5 and D1–D4 pass and are recorded below.
Promise for later phases: **OTA updates only ever install this fork's releases.**

## Gate record

| Check | Date | Commit | Result | Log / notes |
|---|---|---|---|---|
| A1 | 2026-11-15 | fdf2e8c0 | Pass | `pagemind_x4pro_release`: RAM 34.1%, flash 83.3%; only the PageMind OTA URL present |
| A2 | 2026-11-15 | fdf2e8c0 | Pass | `test/fork_ota`: 7/7 |
| A3 | 2026-11-15 | fdf2e8c0 | Pass | 1014/1014 host tests |
| A4 | 2026-11-15 | fdf2e8c0 | Pass | Run 43634876458 (12 min); release `PageMind 0.0.1` not prerelease; SHA-256 OK; esptool: ESP32-S3 image, checksum and hash valid |
| A5 | 2026-11-15 | fdf2e8c0 | Pass | `./bin/clang-format-fix -g` no changes |
| D1 | 2026-11-15 | fdf2e8c0 | Pass | Owner: flashed `pagemind_x4pro_release` over USB after backing up `/.crosspoint`; boot shows PageMind 0.0.1; About shows `PageMind 0.0.1 (CrossPoint 2.0.0)`; books and progress intact; Hindi renders |
| D2 | 2026-11-15 | 0.0.2 tag | Pass | Owner: Settings → Check for updates offered 0.0.2, installed over Wi-Fi, rebooted; About shows `PageMind 0.0.2 (CrossPoint 2.0.0)` |
| D3 | 2026-11-15 | 0.0.2 tag | Pass | Owner: second check reports up to date; no official CrossPoint version offered |
| D4 | 2026-11-15 | 0.0.2 tag | Pass | Owner: Down at power-on opens "Select firmware file (.bin)" recovery picker |
| Owner sign-off | 2026-11-15 | fc0999f3 | Complete | Owner approved; test releases 0.0.1 and 0.0.2 kept |
