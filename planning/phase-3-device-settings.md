# Phase 3: Device settings and secure key storage

**Status:** In progress

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

## Tests and evals

### Automated (agent)

| ID | Check | How |
|---|---|---|
| A1 | Encryption round trip | New host test `test/ai_secret_box`: encrypt → decrypt returns the same key; empty key; 512-byte key |
| A2 | Tamper and wrong-device | Flip one byte → decryption fails cleanly; different device secret → fails cleanly; truncated input → fails cleanly |
| A3 | Nonce uniqueness | 1,000 encryptions of the same key → 1,000 different ciphertexts |
| A4 | Store load/save | Host test: save → load restores URL/enabled; damaged JSON falls back to defaults |
| A5 | Web API never returns the key | Test the settings JSON builder: key field has `value: null` and `hasPassword: true` |
| A6 | Builds and existing tests | `pio run -e x4pro`; all host tests pass; clang-format clean |

### Device (owner)

| ID | Check | Expected |
|---|---|---|
| D1 | Enter URL and key in the web UI, reboot | Values kept; web UI shows key as "saved", never the value |
| D2 | Open `/.crosspoint/` AI settings file on a computer | No readable key in the file |
| D3 | Enter settings on the device itself (on-screen keyboard) | Works in all 4 orientations |
| D4 | OTA update to a new build (Phase 1 pipeline) | Settings survive the update |
| D5 | Memory | Internal heap and PSRAM free/largest block unchanged after opening and saving settings 20 times |

## Gate

The phase is complete when A1–A6 and D1–D5 pass and are recorded below.
Promise for later phases: **`AiSettingsStore` returns a valid URL and the decrypted key,
or a clear "not configured" state.**

## Gate record

| Check | Date | Commit | Result | Log / notes |
|---|---|---|---|---|
| A1 | | | | |
| A2 | | | | |
| A3 | | | | |
| A4 | | | | |
| A5 | | | | |
| A6 | | | | |
| D1 | | | | |
| D2 | | | | |
| D3 | | | | |
| D4 | | | | |
| D5 | | | | |
| Owner sign-off | | | | |
