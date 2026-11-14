#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace ai {

enum class UrlCheck : uint8_t { Ok, Insecure, Empty, TooLong, Invalid };

constexpr size_t MAX_SERVER_URL = 200;
constexpr size_t MAX_API_KEY = 256;
constexpr size_t MAX_LANGUAGE = 40;

// Ok: https URL. Insecure: http URL (allowed for local testing; the UI warns).
UrlCheck checkServerUrl(std::string_view url);

// Trims spaces and trailing slashes, so "https://x.run.app/" works.
std::string normalizeServerUrl(std::string_view url);

}  // namespace ai

// "Ask AI" settings in /.crosspoint/ai.json. The API key is kept in memory in
// plain text and written only sealed (src/ai/SecretBox.h), so the file on the
// SD card is useless on another device.
class AiSettingsStore : public PersistableStore<AiSettingsStore> {
 public:
  static const char* getFilePath() { return "/.crosspoint/ai.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool isEnabled() const { return enabled; }
  void setEnabled(bool on) { enabled = on; }

  const std::string& getServerUrl() const { return serverUrl; }
  void setServerUrl(std::string_view url) { serverUrl = ai::normalizeServerUrl(url); }

  bool hasApiKey() const { return !apiKey.empty(); }
  const std::string& getApiKey() const { return apiKey; }
  void setApiKey(std::string_view key);

  // A saved key could not be opened (SD card from another device, or damaged).
  bool apiKeyUnreadable() const { return keyUnreadable; }

  // Empty means "the device's UI language".
  const std::string& getAnswerLanguage() const { return answerLanguage; }
  void setAnswerLanguage(std::string_view language);

  // Enabled, with a usable URL and a key.
  bool isConfigured() const;

  // Applies a setup file ({"serverUrl", "apiKey", optional "answerLanguage"}):
  // turns Ask AI on. False, with nothing changed, when the JSON, URL or key is invalid.
  bool applySetupJson(std::string_view json);

 private:
  AiSettingsStore() = default;
  friend class PersistableStore<AiSettingsStore>;

  bool enabled = false;
  std::string serverUrl;
  std::string apiKey;
  std::string answerLanguage;
  bool keyUnreadable = false;
};

#define AI_SETTINGS AiSettingsStore::getInstance()
