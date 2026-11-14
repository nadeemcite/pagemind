#pragma once

#include <cstddef>
#include <cstdio>

// PageMind fork identity. Fork-specific values live here so upstream merges
// touch as few upstream lines as possible.

#define FORK_NAME "PageMind"

// OTA reads this repository's latest (non-prerelease) GitHub release.
#define FORK_OTA_RELEASES_URL "https://api.github.com/repos/nadeemcite/pagemind/releases/latest"
#define FORK_OTA_ASSET_PREFIX "pagemind"

// CROSSPOINT_UPSTREAM_VERSION is set by scripts/fork_version.py in the pagemind_* envs.
#ifdef CROSSPOINT_UPSTREAM_VERSION
#define FORK_DISPLAY_VERSION FORK_NAME " " CROSSPOINT_VERSION " (CrossPoint " CROSSPOINT_UPSTREAM_VERSION ")"
#else
#define FORK_DISPLAY_VERSION FORK_NAME " " CROSSPOINT_VERSION
#endif

namespace fork_identity {

// Release asset name: "<prefix>-<tag><suffix>.bin", e.g. "pagemind-0.1.0-x4pro.bin".
// False when it does not fit in `out`.
inline bool otaAssetName(char* out, size_t outSize, const char* tag, const char* boardSuffix) {
  const int n = snprintf(out, outSize, "%s-%s%s.bin", FORK_OTA_ASSET_PREFIX, tag, boardSuffix);
  return n > 0 && static_cast<size_t>(n) < outSize;
}

}  // namespace fork_identity
