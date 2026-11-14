#include "AiSettingsStore.h"

#include <Logging.h>

#include "SecretBox.h"

namespace {
constexpr char KEY_LABEL[] = "ai-api-key";

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' || s.front() == '\r'))
    s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}
}  // namespace

namespace ai {

std::string normalizeServerUrl(std::string_view url) {
  url = trim(url);
  while (!url.empty() && url.back() == '/') url.remove_suffix(1);
  return std::string(url);
}

UrlCheck checkServerUrl(std::string_view url) {
  if (url.empty()) return UrlCheck::Empty;
  if (url.size() > MAX_SERVER_URL) return UrlCheck::TooLong;
  bool secure;
  if (url.substr(0, 8) == "https://") {
    secure = true;
    url.remove_prefix(8);
  } else if (url.substr(0, 7) == "http://") {
    secure = false;
    url.remove_prefix(7);
  } else {
    return UrlCheck::Invalid;
  }
  const std::string_view host = url.substr(0, url.find('/'));
  if (host.empty() || host.front() == '.' || host.front() == ':') return UrlCheck::Invalid;
  for (const char c : url) {
    if (c <= ' ' || c == '"' || c == '<' || c == '>' || c == '\\' || c == '?' || c == '#' || c == 0x7F)
      return UrlCheck::Invalid;
  }
  return secure ? UrlCheck::Ok : UrlCheck::Insecure;
}

}  // namespace ai

void AiSettingsStore::setApiKey(std::string_view key) {
  key = trim(key);
  apiKey.assign(key.substr(0, ai::MAX_API_KEY));
  keyUnreadable = false;
}

void AiSettingsStore::setAnswerLanguage(std::string_view language) {
  language = trim(language);
  answerLanguage.assign(language.substr(0, ai::MAX_LANGUAGE));
}

bool AiSettingsStore::isConfigured() const {
  const ai::UrlCheck url = ai::checkServerUrl(serverUrl);
  return enabled && (url == ai::UrlCheck::Ok || url == ai::UrlCheck::Insecure) && !apiKey.empty();
}

bool AiSettingsStore::applySetupJson(std::string_view json) {
  if (json.empty() || json.size() > 2048) return false;
  JsonDocument doc;
  if (deserializeJson(doc, json.data(), json.size()) || !doc.is<JsonObjectConst>()) return false;
  const std::string url = ai::normalizeServerUrl(doc["serverUrl"] | "");
  const ai::UrlCheck check = ai::checkServerUrl(url);
  if (check != ai::UrlCheck::Ok && check != ai::UrlCheck::Insecure) return false;
  const std::string_view key = trim(doc["apiKey"] | "");
  if (key.empty() || key.size() > ai::MAX_API_KEY) return false;

  enabled = true;
  serverUrl = url;
  setApiKey(key);
  if (doc["answerLanguage"].is<const char*>()) setAnswerLanguage(doc["answerLanguage"].as<const char*>());
  return true;
}

void AiSettingsStore::toJson(JsonDocument& doc) const {
  doc["enabled"] = enabled;
  doc["serverUrl"] = serverUrl.c_str();
  doc["answerLanguage"] = answerLanguage.c_str();
  if (!apiKey.empty()) {
    const std::string sealed = secretbox::seal(KEY_LABEL, apiKey);
    if (sealed.empty()) {
      LOG_ERR("AISET", "Could not seal the API key; it is not saved");
    } else {
      doc["apiKeySealed"] = sealed.c_str();
    }
  }
}

bool AiSettingsStore::fromJson(JsonVariantConst doc) {
  enabled = doc["enabled"] | false;
  serverUrl = ai::normalizeServerUrl(doc["serverUrl"] | "");
  setAnswerLanguage(doc["answerLanguage"] | "");
  apiKey.clear();
  keyUnreadable = false;
  const char* sealed = doc["apiKeySealed"] | "";
  if (sealed[0] != '\0') {
    std::string key;
    if (secretbox::open(KEY_LABEL, sealed, key)) {
      apiKey = std::move(key);
    } else {
      keyUnreadable = true;
      LOG_ERR("AISET", "Saved API key cannot be read on this device; enter it again");
    }
  }
  return true;
}
