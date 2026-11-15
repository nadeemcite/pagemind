# Phase 3: Device settings and secure key storage

**Status:** Complete

## Goal

The user enters the **AI server URL** and **API key** once, on the device or in
the device's web UI. The key is stored encrypted, is never shown again, and
survives reboots and firmware updates.

## Entry criteria

- Phase 2 complete (contract v1 frozen, a deployed proxy available for testing).

## Work

1. **Settings**
   - New store `src/ai/AiSettingsStore.{h,cpp}` (new files), a `PersistableStore`
     JSON file under `/.crosspoint/`, like `KOReaderCredentialStore`.
   - Fields: enabled (on/off), server URL, API key, answer language
     (default: the UI language), default task.
   - Register the settings in `src/SettingsList.h` under the Connectivity category,
     using `stringGetter`/`stringSetter` accessors (no new fields inside
     `CrossPointSettings`, so upstream merges stay easy).
   - Mark the key `obfuscated`: the web settings API then returns only `hasPassword`,
     never the value (`src/network/CrossPointWebServer.cpp:1641`).
2. **Encryption at rest**
   - Today credentials use XOR with the MAC address, which the code itself says is
     "not cryptographically secure" (`lib/Serialization/ObfuscationUtils.h`).
   - New helper `src/ai/SecretBox.{h,cpp}`: AES-256-GCM with a key derived (HKDF-SHA256,
     label `"ai-api-key"`) from `deviceSecret()` (`lib/DeviceSecret/DeviceSecret.h`,
     32 random bytes in NVS that never leave the device). Uses wolfSSL, already in the
     build (`FREEINK_CONTENT_WOLFSSL`).
   - Stored form: base64 of `version | nonce | ciphertext | tag`.
   - If decryption fails (SD card moved to another device, or file damaged): treat the
     key as missing and ask the user to enter it again. Never crash, never log the key.
3. **Validation**
   - URL must be `https://` (allow `http://` only for local network testing, with a
     warning), at most 200 characters.
   - "Test connection" action: calls `GET /v1/info` (needs the Phase 4 HTTP client; in
     this phase it can be a minimal call or be left to Phase 4).
4. **Web UI**
   - The settings page shows an "AI Assistant" group: URL field, write-only key field
     ("saved" or "not set"), enable toggle.
   - Edit the web UI sources, not generated files (see `.agents/rules/generated-files-cache.md`).
5. **Translations**
   - New `StrId` strings in `lib/I18n/translations/english.yaml` (Hindi optional).

## Scope change (2026-11-15, approved by owner before the work)

Typing the URL and a 40+ character key on the e-ink keyboard is slow and
error-prone. Setup order becomes:

1. **SD-card setup file (new):** `deploy.sh` and `rotate-key.sh add` write
   `pagemind-ai.json` (server URL + API key). The user copies it to the SD card
   root. At boot PageMind imports it, seals the key, turns Ask AI on and deletes
   the file. Code: `src/ai/AiSetupImport.{h,cpp}`; parsing in `AiSettingsStore`.
2. **Web settings page:** paste URL and key (already built).
3. **Device keyboard:** fallback only.

New checks: **A7** (setup-file parsing host tests) and **D6** (copy the file to
the SD card, reboot: settings appear, the file is gone). A "Set up from phone"
QR button is a possible later improvement, not in this phase.

## Implementation notes (decided during the phase)

- Key derivation follows the existing `lib/Epub/BookKey.cpp` scheme,
  `sha256("ai-api-key:" || deviceSecret())`, instead of HKDF: one random 32-byte
  secret with a distinct label needs no extra extraction step, and it keeps one
  scheme in the codebase. Stored form: `sb1:` + base64(nonce | ciphertext | tag);
  the label is authenticated as additional data.
- Files: `src/ai/SecretBox.{h,cpp}`, `src/ai/AiSettingsStore.{h,cpp}`
  (`/.crosspoint/ai.json`), `src/activities/settings/AiSettingsActivity.{h,cpp}`.
  Upstream hooks: `SettingsActivity.{h,cpp}` (menu row), `main.cpp` (load at boot),
  `SettingsList.cpp` (web settings), `english.yaml` (strings), `test/CMakeLists.txt`.
- "Test connection" moves to Phase 4, which adds the HTTP client.
- Host tests build the real wolfCrypt 5.7.2 (AES-GCM, SHA-256) and ArduinoJson 7.4.2
  with CMake FetchContent, the same releases as the firmware.
- A5 (web API never returns the key) is covered by code review plus device check D1:
  the setting uses `.withObfuscated()`, for which `CrossPointWebServer` returns
  `value: null` and `hasPassword`. A host test of the web handler would need the
  whole web server stubbed.
- Web "Clear" empties the key; an empty entry on the device keyboard keeps it.

## Tests and evals

### Automated (agent)

| ID | Check | How |
|---|---|---|
| A1 | Encryption round trip | New host test `test/ai_secret_box`: encrypt → decrypt returns the same key; empty key; 512-byte key |
| A2 | Tamper and wrong-device | Flip one byte → decryption fails cleanly; different device secret → fails cleanly; truncated input → fails cleanly |
| A3 | Nonce uniqueness | 1,000 encryptions of the same key → 1,000 different ciphertexts |
| A4 | Store load/save | Host test: save → load restores URL/enabled; damaged JSON falls back to defaults |
| A5 | Web API never returns the key | Test the settings JSON builder: key field has `value: null` and `hasPassword: true` |
| A7 | Setup-file import | Host test: valid file → URL, key, language set and Ask AI on; invalid URL, missing key, damaged or oversized file → rejected, nothing changed |
| A6 | Builds and existing tests | `pio run -e x4pro`; all host tests pass; clang-format clean |

### Device (owner)

| ID | Check | Expected |
|---|---|---|
| D1 | Enter URL and key in the web UI, reboot | Values kept; web UI shows key as "saved", never the value |
| D2 | Open `/.crosspoint/` AI settings file on a computer | No readable key in the file |
| D3 | Enter settings on the device itself (on-screen keyboard) | Works in all 4 orientations |
| D4 | OTA update to a new build (Phase 1 pipeline) | Settings survive the update |
| D6 | SD-card setup file | Copy `pagemind-ai.json` from `deploy.sh` to the SD card root, reboot: AI Assistant shows the URL and "Saved", Ask AI is on, the file is deleted |
| D5 | Memory | Internal heap and PSRAM free/largest block unchanged after opening and saving settings 20 times |

## Gate

The phase is complete when A1–A6 and D1–D5 pass and are recorded below.
Promise for later phases: **`AiSettingsStore` returns a valid URL and the decrypted key,
or a clear "not configured" state.**

## Gate record

| Check | Date | Commit | Result | Log / notes |
|---|---|---|---|---|
| A1 | 2026-11-15 | e1218911 | Pass | `AiSecretBoxTest` (real wolfCrypt 5.7.2): round trip incl. lengths 1–512, empty/too-long rejected |
| A2 | 2026-11-15 | e1218911 | Pass | Every flipped byte, other device secret, other label, missing secret, truncated/malformed input → clean failure, output cleared |
| A3 | 2026-11-15 | e1218911 | Pass | 1,000 seals of the same key → 1,000 distinct values |
| A4 | 2026-11-15 | e1218911 | Pass | `AiSettingsTest` 10/10: JSON round trip, plain key never in file, unreadable key flagged without crash, damaged JSON → defaults, URL checks |
| A5 | 2026-11-15 | e1218911 | Pass (review) | `aiApiKey` uses `.withObfuscated()`; `CrossPointWebServer.cpp:1641` returns `value: null` + `hasPassword`. Confirmed on device in D1 |
| A6 | 2026-11-15 | e1218911 | Pass | `pagemind_x4pro_release` builds (RAM +96 B, flash +10.6 KB vs Phase 1); host tests 1034/1034; clang-format clean |
| A7 | 2026-11-15 | e6c11d3b | Pass | `AiSettingsTest` setup-file cases 4/4 (valid file; 9 invalid files change nothing; oversized rejected); host tests 1038/1038 |
| D1 | 2026-11-15 | b2fcb846 (0.0.2-dev) | Pass | Owner: all AI Assistant steps work on device (web settings entry, key shown as "(unchanged)", values kept after reboot) |
| D2 | 2026-11-15 | de5cb988 (0.0.2-dev) | Pass | `/.crosspoint/ai.json` holds `enabled`, `serverUrl`, `answerLanguage`, `apiKeySealed` (`sb1:`, 104 chars). Neither device key, nor `pm_`, `apiKey`, `Bearer` appears in it |
| D3 | 2026-11-15 | b2fcb846 (0.0.2-dev) | Pass | Owner: AI Assistant screen and on-screen keyboard entry work on device |
| D4 | 2026-11-15 | 0.0.3 tag | Pass | Owner: OTA from 0.0.2-dev to release 0.0.3; About shows `PageMind 0.0.3 (CrossPoint 2.0.0)`; AI Assistant keeps Ask AI on, URL, key Saved, language hinglish |
| D5 | 2026-11-15 | de5cb988 (0.0.2-dev) | Pass | Owner: ~20 open/edit/back rounds. Home before: 159,500 B free, largest block 114,676 B; Home after: 159,492 B, 114,676 B; PSRAM unchanged. Serial dropped 04:51:40–04:54:05 (no reboot; uptime continuous) while most rounds ran, so the result rests on before/after Home readings |
| D6 | 2026-11-15 | de5cb988 (0.0.2-dev) | Pass | Owner copied `pagemind-ai.json` (from `rotate-key.sh add`) to the SD root; after restart the file is gone and `ai.json` has Ask AI on with the server URL and a sealed key. Which key was sealed cannot be read off-device by design. Phase 4 D2 confirmed that importing a setup file replaces an existing key (server device id changed to the setup-file key) |
| Owner sign-off | 2026-11-15 | 595eb87a | Complete | Owner approved |
