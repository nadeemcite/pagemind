#include <gtest/gtest.h>

#include <string>

#include "FakeDeviceSecret.h"
#include "ai/AiSettingsStore.h"

namespace {

using ai::UrlCheck;

// The store is a singleton; reset it through its own JSON loader.
void reset() {
  JsonDocument empty;
  empty.to<JsonObject>();
  AI_SETTINGS.fromJson(empty.as<JsonVariantConst>());
  fake_device_secret::available = true;
  for (int i = 0; i < 32; i++) fake_device_secret::value[i] = static_cast<uint8_t>(i + 1);
}

std::string saveToString() {
  JsonDocument doc;
  AI_SETTINGS.toJson(doc);
  std::string out;
  serializeJson(doc, out);
  return out;
}

void loadFromString(const std::string& json) {
  JsonDocument doc;
  const auto err = deserializeJson(doc, json);
  if (err) doc.to<JsonObject>();  // what PersistableStore does with a damaged file: defaults
  AI_SETTINGS.fromJson(doc.as<JsonVariantConst>());
}

TEST(AiSettings, DefaultsAreOffAndUnconfigured) {
  reset();
  EXPECT_FALSE(AI_SETTINGS.isEnabled());
  EXPECT_FALSE(AI_SETTINGS.hasApiKey());
  EXPECT_FALSE(AI_SETTINGS.isConfigured());
  EXPECT_EQ(AI_SETTINGS.getAnswerLanguage(), "");
}

TEST(AiSettings, SaveLoadRoundTrip) {
  reset();
  AI_SETTINGS.setEnabled(true);
  AI_SETTINGS.setServerUrl(" https://ai-proxy-abc-uc.a.run.app/ ");
  AI_SETTINGS.setApiKey("pm_secretSecretSecret123");
  AI_SETTINGS.setAnswerLanguage("Hindi");
  const std::string json = saveToString();

  reset();
  loadFromString(json);
  EXPECT_TRUE(AI_SETTINGS.isEnabled());
  EXPECT_EQ(AI_SETTINGS.getServerUrl(), "https://ai-proxy-abc-uc.a.run.app");
  EXPECT_EQ(AI_SETTINGS.getApiKey(), "pm_secretSecretSecret123");
  EXPECT_EQ(AI_SETTINGS.getAnswerLanguage(), "Hindi");
  EXPECT_TRUE(AI_SETTINGS.isConfigured());
  EXPECT_FALSE(AI_SETTINGS.apiKeyUnreadable());
}

TEST(AiSettings, FileNeverContainsThePlainKey) {
  reset();
  AI_SETTINGS.setApiKey("pm_secretSecretSecret123");
  const std::string json = saveToString();
  EXPECT_EQ(json.find("pm_secretSecretSecret123"), std::string::npos);
  EXPECT_NE(json.find("\"apiKeySealed\":\"sb1:"), std::string::npos);
}

TEST(AiSettings, NoKeyMeansNoSealedField) {
  reset();
  EXPECT_EQ(saveToString().find("apiKeySealed"), std::string::npos);
}

TEST(AiSettings, KeyFromAnotherDeviceIsUnreadableNotCrash) {
  reset();
  AI_SETTINGS.setEnabled(true);
  AI_SETTINGS.setServerUrl("https://x.run.app");
  AI_SETTINGS.setApiKey("pm_secretSecretSecret123");
  const std::string json = saveToString();
  fake_device_secret::value[5] ^= 0x55;  // SD card moved to another reader
  loadFromString(json);
  EXPECT_FALSE(AI_SETTINGS.hasApiKey());
  EXPECT_TRUE(AI_SETTINGS.apiKeyUnreadable());
  EXPECT_FALSE(AI_SETTINGS.isConfigured());
  EXPECT_EQ(AI_SETTINGS.getServerUrl(), "https://x.run.app");  // other settings survive
  AI_SETTINGS.setApiKey("pm_newKey");
  EXPECT_FALSE(AI_SETTINGS.apiKeyUnreadable());
}

TEST(AiSettings, DamagedFileFallsBackToDefaults) {
  reset();
  for (const char* bad :
       {"{not json", "", "[]", "{\"enabled\":\"yes\",\"serverUrl\":5}", "{\"apiKeySealed\":\"sb1:garbage\"}"}) {
    reset();
    loadFromString(bad);
    EXPECT_FALSE(AI_SETTINGS.isConfigured()) << bad;
    EXPECT_FALSE(AI_SETTINGS.hasApiKey()) << bad;
  }
}

TEST(AiSettings, ApiKeyAndLanguageAreTrimmedAndBounded) {
  reset();
  AI_SETTINGS.setApiKey("  pm_key \n");
  EXPECT_EQ(AI_SETTINGS.getApiKey(), "pm_key");
  AI_SETTINGS.setApiKey(std::string(1000, 'k'));
  EXPECT_EQ(AI_SETTINGS.getApiKey().size(), ai::MAX_API_KEY);
  AI_SETTINGS.setAnswerLanguage(std::string(100, 'l'));
  EXPECT_EQ(AI_SETTINGS.getAnswerLanguage().size(), ai::MAX_LANGUAGE);
}

TEST(AiSettings, ConfiguredNeedsEnabledUrlAndKey) {
  reset();
  AI_SETTINGS.setServerUrl("https://x.run.app");
  AI_SETTINGS.setApiKey("k");
  EXPECT_FALSE(AI_SETTINGS.isConfigured());
  AI_SETTINGS.setEnabled(true);
  EXPECT_TRUE(AI_SETTINGS.isConfigured());
  AI_SETTINGS.setServerUrl("not a url");
  EXPECT_FALSE(AI_SETTINGS.isConfigured());
  AI_SETTINGS.setServerUrl("http://192.168.1.20:8080");
  EXPECT_TRUE(AI_SETTINGS.isConfigured());  // allowed for local testing
}

TEST(AiUrl, Checks) {
  EXPECT_EQ(ai::checkServerUrl("https://ai-proxy-cyshxss5ta-uc.a.run.app"), UrlCheck::Ok);
  EXPECT_EQ(ai::checkServerUrl("https://example.com/proxy"), UrlCheck::Ok);
  EXPECT_EQ(ai::checkServerUrl("http://192.168.1.20:8080"), UrlCheck::Insecure);
  EXPECT_EQ(ai::checkServerUrl(""), UrlCheck::Empty);
  EXPECT_EQ(ai::checkServerUrl("https://" + std::string(200, 'a')), UrlCheck::TooLong);
  for (const char* bad : {"ftp://x", "x.run.app", "https://", "https:///path", "https://a b.com", "https://x.com?k=1",
                          "https://x.com#f", "https://.x.com", "HTTPS//x.com"}) {
    EXPECT_EQ(ai::checkServerUrl(bad), UrlCheck::Invalid) << bad;
  }
}

TEST(AiUrl, Normalize) {
  EXPECT_EQ(ai::normalizeServerUrl("  https://x.run.app//  "), "https://x.run.app");
  EXPECT_EQ(ai::normalizeServerUrl("https://x.run.app/v"), "https://x.run.app/v");
}

TEST(AiSetupFile, ValidFileTurnsAskAiOn) {
  reset();
  ASSERT_TRUE(AI_SETTINGS.applySetupJson(
      R"({"serverUrl":"https://ai-proxy-abc-uc.a.run.app/","apiKey":" pm_fromDeploy123 ","answerLanguage":"Hindi"})"));
  EXPECT_TRUE(AI_SETTINGS.isEnabled());
  EXPECT_EQ(AI_SETTINGS.getServerUrl(), "https://ai-proxy-abc-uc.a.run.app");
  EXPECT_EQ(AI_SETTINGS.getApiKey(), "pm_fromDeploy123");
  EXPECT_EQ(AI_SETTINGS.getAnswerLanguage(), "Hindi");
  EXPECT_TRUE(AI_SETTINGS.isConfigured());
}

TEST(AiSetupFile, LanguageIsOptionalAndKeepsCurrent) {
  reset();
  AI_SETTINGS.setAnswerLanguage("Hindi");
  ASSERT_TRUE(AI_SETTINGS.applySetupJson(R"({"serverUrl":"https://x.run.app","apiKey":"pm_k"})"));
  EXPECT_EQ(AI_SETTINGS.getAnswerLanguage(), "Hindi");
}

TEST(AiSetupFile, InvalidFilesChangeNothing) {
  const char* bad[] = {
      "",
      "{not json",
      "[]",
      R"("just a string")",
      R"({"apiKey":"pm_k"})",
      R"({"serverUrl":"https://x.run.app"})",
      R"({"serverUrl":"ftp://x","apiKey":"pm_k"})",
      R"({"serverUrl":"https://x.run.app","apiKey":"   "})",
      R"({"serverUrl":"https://x.run.app","apiKey":5})",
  };
  for (const char* json : bad) {
    reset();
    AI_SETTINGS.setServerUrl("https://old.run.app");
    AI_SETTINGS.setApiKey("pm_old");
    EXPECT_FALSE(AI_SETTINGS.applySetupJson(json)) << json;
    EXPECT_FALSE(AI_SETTINGS.isEnabled()) << json;
    EXPECT_EQ(AI_SETTINGS.getServerUrl(), "https://old.run.app") << json;
    EXPECT_EQ(AI_SETTINGS.getApiKey(), "pm_old") << json;
  }
}

TEST(AiSetupFile, OversizedFileAndKeyRejected) {
  reset();
  const std::string bigKey(ai::MAX_API_KEY + 1, 'k');
  EXPECT_FALSE(AI_SETTINGS.applySetupJson(R"({"serverUrl":"https://x.run.app","apiKey":")" + bigKey + "\"}"));
  EXPECT_FALSE(AI_SETTINGS.applySetupJson(std::string(3000, ' ')));
  EXPECT_FALSE(AI_SETTINGS.isEnabled());
}

}  // namespace
