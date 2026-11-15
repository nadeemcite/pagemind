#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ai/AiProtocol.h"

namespace fs = std::filesystem;
using ai::Error;

namespace {

std::string readFile(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::vector<fs::path> fixtures(const char* kind) {
  std::vector<fs::path> out;
  for (const auto& e : fs::directory_iterator(fs::path(CONTRACT_DIR) / "fixtures" / kind)) {
    if (e.path().extension() == ".json") out.push_back(e.path());
  }
  EXPECT_FALSE(out.empty()) << kind;
  return out;
}

size_t utf8Chars(const std::string& s) {
  size_t n = 0;
  for (const unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}

bool validUtf8(std::string_view s) {
  size_t i = 0;
  while (i < s.size()) {
    const unsigned char c = s[i];
    const size_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (len == 0 || i + len > s.size()) return false;
    for (size_t k = 1; k < len; k++)
      if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
    i += len;
  }
  return true;
}

// Checks a request body against contract/ask-request.schema.json: required
// "text", no unknown fields, enum task, string types, maxLength in code points.
void expectMatchesRequestSchema(const std::string& body) {
  JsonDocument schema;
  ASSERT_FALSE(deserializeJson(schema, readFile(fs::path(CONTRACT_DIR) / "ask-request.schema.json")));
  JsonDocument doc;
  ASSERT_FALSE(deserializeJson(doc, body)) << body;
  ASSERT_TRUE(doc["text"].is<const char*>()) << body;
  for (JsonPairConst kv : doc.as<JsonObjectConst>()) {
    JsonVariantConst prop = schema["properties"][kv.key().c_str()];
    ASSERT_FALSE(prop.isNull()) << "unknown field " << kv.key().c_str();
    ASSERT_TRUE(kv.value().is<const char*>()) << kv.key().c_str();
    const std::string v = kv.value().as<const char*>();
    if (prop["enum"].is<JsonArrayConst>()) {
      bool found = false;
      for (JsonVariantConst e : prop["enum"].as<JsonArrayConst>()) found |= v == e.as<const char*>();
      EXPECT_TRUE(found) << v;
    }
    if (prop["maxLength"].is<int>()) EXPECT_LE(utf8Chars(v), prop["maxLength"].as<size_t>()) << kv.key().c_str();
    if (prop["minLength"].is<int>()) EXPECT_GE(utf8Chars(v), prop["minLength"].as<size_t>()) << kv.key().c_str();
  }
}

ai::Task taskFrom(const char* s) {
  const std::string t = s ? s : "explain";
  if (t == "define") return ai::Task::Define;
  if (t == "translate") return ai::Task::Translate;
  if (t == "ask") return ai::Task::Ask;
  return ai::Task::Explain;
}

}  // namespace

// A1: requests built from every contract request fixture match the schema and keep the text.
TEST(AiClient, RequestsFromFixturesMatchContract) {
  for (const auto& path : fixtures("requests")) {
    JsonDocument f;
    ASSERT_FALSE(deserializeJson(f, readFile(path))) << path;
    ai::AskRequest req;
    req.task = taskFrom(f["task"] | "explain");
    req.text = f["text"] | "";
    req.context = f["context"] | "";
    req.question = f["question"] | "";
    req.language = f["language"] | "";
    req.bookTitle = f["book_title"] | "";
    req.bookAuthor = f["book_author"] | "";
    const std::string body = ai::buildAskBody(req, ai::Limits{});
    expectMatchesRequestSchema(body);
    JsonDocument out;
    deserializeJson(out, body);
    EXPECT_EQ(std::string(out["text"].as<const char*>()), req.text) << path.filename();
  }
}

TEST(AiClient, EscapesQuotesNewlinesAndKeepsHindi) {
  ai::AskRequest req;
  req.task = ai::Task::Translate;
  req.text = "He said \"wait\"\nand left. मित्रता \\ end";
  req.language = "Hindi";
  const std::string body = ai::buildAskBody(req, ai::Limits{});
  JsonDocument out;
  ASSERT_FALSE(deserializeJson(out, body)) << body;
  EXPECT_EQ(std::string(out["text"].as<const char*>()), req.text);
  EXPECT_EQ(std::string(out["task"].as<const char*>()), "translate");
}

TEST(AiClient, QuestionOnlySentForAskTask) {
  ai::AskRequest req;
  req.text = "x";
  req.question = "why?";
  EXPECT_EQ(ai::buildAskBody(req, {}).find("question"), std::string::npos);
  req.task = ai::Task::Ask;
  EXPECT_NE(ai::buildAskBody(req, {}).find("\"question\":\"why?\""), std::string::npos);
}

TEST(AiClient, TrimsToServerLimits) {
  ai::AskRequest req;
  req.text = std::string(5000, 'a');
  req.context = std::string(3000, 'c');
  ai::Limits limits;
  limits.maxInputChars = 100;
  limits.maxContextChars = 10;
  JsonDocument out;
  deserializeJson(out, ai::buildAskBody(req, limits));
  EXPECT_EQ(std::string(out["text"].as<const char*>()).size(), 100u);
  EXPECT_EQ(std::string(out["context"].as<const char*>()).size(), 10u);
}

// A2: every contract response and error fixture maps to the expected result.
TEST(AiClient, ResponseFixturesParse) {
  for (const auto& path : fixtures("responses")) {
    const std::string body = readFile(path);
    if (path.filename().string().rfind("info", 0) == 0) {
      ai::Info info;
      ASSERT_EQ(ai::parseInfoResponse(200, body, info), Error::None) << path.filename();
      EXPECT_EQ(info.api, 1);
      EXPECT_EQ(info.limits.maxInputChars, 4000u);
      EXPECT_EQ(info.model, "glm-5.3-flash");
    } else {
      ai::Answer a;
      ASSERT_EQ(ai::parseAskResponse(200, body, a), Error::None) << path.filename();
      EXPECT_FALSE(a.text.empty());
      EXPECT_EQ(a.truncated, path.filename() == "ask-truncated.json");
    }
  }
}

TEST(AiClient, ErrorFixturesMapToErrors) {
  const std::pair<const char*, Error> expected[] = {
      {"401-unauthorized.json", Error::BadKey},          {"413-body_too_large.json", Error::TextTooLong},
      {"413-text_too_long.json", Error::TextTooLong},    {"413-context_too_long.json", Error::ClientBug},
      {"422-invalid_request.json", Error::ClientBug},    {"429-rate_limited.json", Error::RateLimited},
      {"502-provider_error.json", Error::ProviderFailed},
  };
  ASSERT_EQ(fixtures("errors").size(), std::size(expected)) << "a fixture without an expected mapping";
  for (const auto& [name, err] : expected) {
    const fs::path p = fs::path(CONTRACT_DIR) / "fixtures" / "errors" / name;
    const int status = std::stoi(std::string(name).substr(0, 3));
    ai::Answer a;
    EXPECT_EQ(ai::parseAskResponse(status, readFile(p), a), err) << name;
    ai::Info info;
    EXPECT_EQ(ai::parseInfoResponse(status, readFile(p), info), err) << name;
  }
}

// A3: hostile or unexpected bodies give a clean error.
TEST(AiClient, HostileBodies) {
  ai::Answer a;
  EXPECT_EQ(ai::parseAskResponse(0, "", a), Error::Connection);
  EXPECT_EQ(ai::parseAskResponse(-1, "", a), Error::Connection);
  // Google front-end HTML pages are never "bad key".
  EXPECT_EQ(ai::parseAskResponse(401, "<html><title>401 Unauthorized</title></html>", a), Error::ServerUnavailable);
  EXPECT_EQ(ai::parseAskResponse(404, "<!DOCTYPE html><title>Error 404</title>", a), Error::ServerUnavailable);
  EXPECT_EQ(ai::parseAskResponse(503, "", a), Error::ServerUnavailable);
  EXPECT_EQ(ai::parseAskResponse(401, R"({"error":"something_else"})", a), Error::ServerUnavailable);
  EXPECT_EQ(ai::parseAskResponse(500, R"({"error":"unauthorized"})", a), Error::ServerUnavailable);
  const char* bad200[] = {"",
                          "{",
                          R"({"answer":"x"})",
                          R"({"answer":"","model":"m","truncated":false})",
                          R"({"answer":5,"model":"m","truncated":false})",
                          R"({"answer":"x","model":"m","truncated":"no"})",
                          "[1,2,3]",
                          "null",
                          "\xff\xfe\x00garbage"};
  for (const char* b : bad200) {
    EXPECT_EQ(ai::parseAskResponse(200, b, a), Error::BadResponse) << b;
    EXPECT_TRUE(a.text.empty());
  }
  const std::string huge = R"({"answer":")" + std::string(ai::MAX_RESPONSE_BYTES, 'x') + R"(","model":"m","truncated":false})";
  EXPECT_EQ(ai::parseAskResponse(200, huge, a), Error::BadResponse);
  ai::Info info;
  EXPECT_EQ(ai::parseInfoResponse(200, R"({"api":2,"max_input_chars":10,"max_context_chars":1})", info),
            Error::BadResponse);
  EXPECT_EQ(ai::parseInfoResponse(200, R"({"api":1,"max_input_chars":0,"max_context_chars":1})", info),
            Error::BadResponse);
  EXPECT_EQ(ai::parseInfoResponse(200, R"({"api":1})", info), Error::BadResponse);
}

// A4: trimming never splits a UTF-8 character.
TEST(AiClient, TruncateUtf8NeverSplits) {
  const std::string mixed = "aé मित्रता 😀 x";  // 1-, 2-, 3- and 4-byte characters
  for (size_t n = 0; n <= 20; n++) {
    const auto cut = ai::truncateUtf8(mixed, n);
    EXPECT_TRUE(validUtf8(cut)) << n;
    EXPECT_LE(utf8Chars(std::string(cut)), n);
  }
  EXPECT_EQ(ai::truncateUtf8(mixed, 100), mixed);
  EXPECT_EQ(ai::truncateUtf8("मित्रता", 1), "म");
  EXPECT_EQ(ai::truncateUtf8("😀😀", 1), "😀");
  EXPECT_EQ(ai::truncateUtf8("ab\xE0\xA4", 10), "ab");  // cut-off sequence at the end is dropped
  EXPECT_EQ(ai::truncateUtf8("", 5), "");
}

TEST(AiClient, CountsCodePointsLikeTheServer) {
  ai::AskRequest req;
  req.text = std::string();
  for (int i = 0; i < 5000; i++) req.text += "अ";  // 3 bytes each
  JsonDocument out;
  deserializeJson(out, ai::buildAskBody(req, ai::Limits{}));
  EXPECT_EQ(utf8Chars(out["text"].as<const char*>()), 4000u);
}
