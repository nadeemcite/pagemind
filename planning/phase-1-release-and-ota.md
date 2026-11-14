# Phase 1: Fork identity, releases and OTA

**Status:** Not started

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
     workflow changes never conflict): on tag `v*`, build `x4pro`, name the asset to
     match the OTA pattern, publish a GitHub release with notes.
   - Disable upstream release workflows in the fork (they publish CrossPoint names).
4. **Docs**
   - `FORK.md`: install guide (web flasher with custom `.bin`, or `pio`), the
     USB-locked device warning from upstream README, how to update.

## Tests and evals

### Automated (agent)

| ID | Check | How |
|---|---|---|
| A1 | Firmware builds | `pio run -e x4pro` succeeds |
| A2 | Release JSON parsing finds the fork asset name | Extend host test `test/release_json_parser` with a fixture of a fork release JSON (asset `<name>-v0.1.0-x4pro.bin`), plus a negative fixture with only official CrossPoint assets → no update offered |
| A3 | Upstream host tests still pass | `pio run -t unit-tests` (or the CMake/CTest command in `test/README`) |
| A4 | Release workflow works | Push tag `v0.1.0-test1` to the fork; workflow publishes a release whose asset name matches the pattern; download it and compare SHA-256 with the CI artifact |
| A5 | Formatting | `./bin/clang-format-fix -g` reports no changes |

### Device (owner)

| ID | Check | Expected |
|---|---|---|
| D1 | Flash `v0.1.0-test1` over USB | Boots; About shows fork name and version; books and progress intact |
| D2 | Publish `v0.1.0-test2`, then Settings → Check for updates | Offers `v0.1.0-test2`, installs, reboots into it |
| D3 | Check for updates again | "Up to date"; never offers an official CrossPoint version |
| D4 | Recovery | Hold the recovery button at boot (X4 Pro: Down); the device still boots the other firmware slot |

## Gate

The phase is complete when A1–A5 and D1–D4 pass and are recorded below.
Promise for later phases: **OTA updates only ever install this fork's releases.**

## Gate record

| Check | Date | Commit | Result | Log / notes |
|---|---|---|---|---|
| A1 | | | | |
| A2 | | | | |
| A3 | | | | |
| A4 | | | | |
| A5 | | | | |
| D1 | | | | |
| D2 | | | | |
| D3 | | | | |
| D4 | | | | |
| Owner sign-off | | | | |
