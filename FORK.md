# About this fork

This is a personal fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader),
built for the Xteink X4 Pro (`pio run -e x4pro`). It is not meant to be merged upstream.

## What this fork adds

| Addition | Where | Status |
|---|---|---|
| Indic script shaping (upstream PR [#3787](https://github.com/crosspoint-reader/crosspoint-reader/pull/3787)) | merged into `personal` | merged |
| AI proxy server (Cloud Run) | [`cloud/ai-proxy/`](cloud/ai-proxy/README.md) | ready to deploy |
| "Ask AI" on selected text (firmware) | — | planned |
| OTA updates from this fork instead of upstream | — | planned |

## Branches and remotes

- `personal` (default branch): upstream `develop` + PR #3787 + this fork's work.
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
pio run -e x4pro
git push origin personal
```

Keep fork changes in their own files and folders where possible, so these merges
rarely conflict.

## Warning: OTA updates

Until the OTA change lands, **do not install updates from the device's
Settings**. They download official CrossPoint firmware and remove this fork's
features.
