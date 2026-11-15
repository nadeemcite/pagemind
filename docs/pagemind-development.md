# PageMind development

How PageMind is built, released and kept in sync with
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).
The feature plan and its test gates are in [`planning/`](../planning/README.md).

## Repository layout

| Path | What it is |
|---|---|
| `src/fork/` | PageMind identity: name, version string, update source |
| `src/ai/` | Ask AI: settings, encrypted key, protocol, HTTPS client, trusted roots |
| `src/activities/reader/AiAskActivity.*` | The Ask AI flow inside a book |
| `src/activities/settings/Ai*Activity.*` | AI Assistant settings and Test connection |
| `cloud/ai-proxy/` | The AI server (FastAPI on Cloud Run), its API contract, deploy scripts and evals |
| `platformio.fork.ini` | PageMind build targets and version |
| `.github/workflows/fork-release.yml`, `ai-proxy.yml` | Release build and server CI |
| `planning/` | Phase plans, decisions and test records |

Fork code lives in its own files where possible; upstream files get small hooks
only, so merges from CrossPoint rarely conflict.

## Build from source

Toolchain setup is CrossPoint's: [getting started](contributing/getting-started.md)
(pioarduino core 6.1.19, Python 3.13).

```bash
pio run -e pagemind_x4pro                 # development build (version "<v>-dev-<sha>")
pio run -e pagemind_x4pro -t upload       # build and flash over USB
pio run -e pagemind_x4pro_release         # release build, as published
```

Host tests: `cmake -S test -B build/test && cmake --build build/test && ctest --test-dir build/test`.
Server tests: `cd cloud/ai-proxy && uv run pytest`.

## Making a release

1. Set `version` in `platformio.fork.ini` (numbers only, e.g. `0.1.0`), commit, push.
2. `git tag 0.1.0 && git push origin 0.1.0`
3. `.github/workflows/fork-release.yml` builds `pagemind-0.1.0-x4pro.bin` and
   publishes the release. Devices see it only if it is not marked pre-release.

Upstream release workflows are disabled in this repository's Actions settings.

## Branches and remotes

`personal` is the default branch: upstream `develop` + PR #3787 + PageMind.

```bash
git remote add upstream https://github.com/crosspoint-reader/crosspoint-reader.git
git remote add pr3787   https://github.com/ssafayet/crosspoint-reader.git
# Upstream's release tags (0.1.0 … 2.0.0) clash with PageMind versions:
git config remote.upstream.tagOpt --no-tags
git config remote.pr3787.tagOpt --no-tags
# Keep PageMind's README.md when merging (see .gitattributes):
git config merge.ours.driver true
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

CrossPoint's own README is not merged into PageMind's: read it
[on GitHub](https://github.com/crosspoint-reader/crosspoint-reader#readme).
