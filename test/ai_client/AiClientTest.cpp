#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ai/AiContext.h"
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
  // Contract: "text" is required, except for task "summarize", which requires "text_before".
  const bool summarize = std::string(doc["task"] | "explain") == "summarize";
  ASSERT_TRUE(doc[summarize ? "text_before" : "text"].is<const char*>()) << body;
  if (summarize) EXPECT_TRUE(doc["text"].isNull()) << "summarize sends no selected text";
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
  if (t == "summarize") return ai::Task::Summarize;
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
    req.chapter = f["chapter"] | "";
    req.textBefore = f["text_before"] | "";
    req.textAfter = f["text_after"] | "";
    const std::string body = ai::buildAskBody(req, ai::Limits{});
    expectMatchesRequestSchema(body);
    JsonDocument out;
    deserializeJson(out, body);
    if (req.task == ai::Task::Summarize) {
      // C1: every summarize fixture comes back field for field (this is the contract in both directions).
      for (const char* key :
           {"task", "chapter", "text_before", "text_after", "language", "book_title", "book_author"}) {
        EXPECT_EQ(std::string(out[key] | ""), std::string(f[key] | "")) << key << " in " << path.filename();
      }
    } else {
      EXPECT_EQ(std::string(out["text"].as<const char*>()), req.text) << path.filename();
    }
  }
}

// C1: summarize request details.
TEST(AiClientSummarize, BodyHasWindowsAndNoTextOrQuestion) {
  ai::AskRequest req;
  req.task = ai::Task::Summarize;
  req.text = "must not be sent";
  req.question = "must not be sent";
  req.chapter = "Chapter 3";
  req.textBefore = "before words";
  req.textAfter = "after words";
  req.bookTitle = "Moby-Dick";
  const std::string body = ai::buildAskBody(req, ai::Limits{});
  JsonDocument out;
  ASSERT_FALSE(deserializeJson(out, body));
  EXPECT_EQ(std::string(out["task"].as<const char*>()), "summarize");
  EXPECT_TRUE(out["text"].isNull());
  EXPECT_TRUE(out["question"].isNull());
  EXPECT_TRUE(out["context"].isNull());
  EXPECT_EQ(std::string(out["text_before"].as<const char*>()), "before words");
  EXPECT_EQ(std::string(out["text_after"].as<const char*>()), "after words");
  EXPECT_EQ(std::string(out["chapter"].as<const char*>()), "Chapter 3");
}

TEST(AiClientSummarize, EmptyAfterAndChapterAreLeftOut) {
  ai::AskRequest req;
  req.task = ai::Task::Summarize;
  req.textBefore = "only before";
  JsonDocument out;
  ASSERT_FALSE(deserializeJson(out, ai::buildAskBody(req, ai::Limits{})));
  EXPECT_TRUE(out["text_after"].isNull());
  EXPECT_TRUE(out["chapter"].isNull());
}

TEST(AiClientSummarize, WindowsTrimmedToServerLimitKeepingTheTextNearestThePosition) {
  ai::AskRequest req;
  req.task = ai::Task::Summarize;
  req.textBefore = "AAAA BBBB CCCC";  // position is at the end: keep the end
  req.textAfter = "DDDD EEEE FFFF";   // keep the start
  ai::Limits limits;
  limits.maxWindowChars = 9;
  JsonDocument out;
  ASSERT_FALSE(deserializeJson(out, ai::buildAskBody(req, limits)));
  EXPECT_EQ(std::string(out["text_before"].as<const char*>()), "BBBB CCCC");
  EXPECT_EQ(std::string(out["text_after"].as<const char*>()), "DDDD EEEE");
}

TEST(AiClientSummarize, TrimNeverSplitsHindiCharacters) {
  ai::AskRequest req;
  req.task = ai::Task::Summarize;
  for (int i = 0; i < 50; i++) req.textBefore += "मित्रता ";
  ai::Limits limits;
  limits.maxWindowChars = 25;
  JsonDocument out;
  ASSERT_FALSE(deserializeJson(out, ai::buildAskBody(req, limits)));
  const std::string before = out["text_before"].as<const char*>();
  EXPECT_TRUE(validUtf8(before));
  EXPECT_LE(utf8Chars(before), 25u);
}

TEST(AiClientSummarize, TailUtf8) {
  EXPECT_EQ(ai::tailUtf8("abcdef", 3), "def");
  EXPECT_EQ(ai::tailUtf8("abc", 10), "abc");
  EXPECT_EQ(ai::tailUtf8("", 3), "");
  EXPECT_EQ(ai::tailUtf8("aमि", 2), "मि");  // two whole code points (a letter and a vowel sign)
  EXPECT_TRUE(validUtf8(ai::tailUtf8("मित्रता", 3)));
  EXPECT_EQ(utf8Chars(std::string(ai::tailUtf8("मित्रता", 3))), 3u);
}

// C2: /v1/info tells whether the proxy supports summarize, and its window limit.
TEST(AiClientSummarize, InfoReportsSupportAndWindowLimit) {
  ai::Info info;
  ASSERT_EQ(
      ai::parseInfoResponse(200, readFile(fs::path(CONTRACT_DIR) / "fixtures" / "responses" / "info-ok.json"), info),
      Error::None);
  EXPECT_TRUE(info.supportsSummarize);
  EXPECT_EQ(info.limits.maxWindowChars, 3000u);
  EXPECT_EQ(ai::checkTaskSupported(info, ai::Task::Summarize), Error::None);
}

TEST(AiClientSummarize, OldProxyWithoutSummarizeIsReportedNotRetried) {
  const char* old =
      R"({"api":1,"tasks":["explain","define","translate","ask"],"max_input_chars":4000,"max_context_chars":2000,)"
      R"("max_question_chars":500,"provider":"ollama","model":"m"})";
  ai::Info info;
  ASSERT_EQ(ai::parseInfoResponse(200, old, info), Error::None);
  EXPECT_FALSE(info.supportsSummarize);
  EXPECT_EQ(info.limits.maxWindowChars, 3000u);  // default when the proxy does not report one
  EXPECT_EQ(ai::checkTaskSupported(info, ai::Task::Summarize), Error::ProxyOutdated);
  // The other tasks keep working against an old proxy.
  EXPECT_EQ(ai::checkTaskSupported(info, ai::Task::Explain), Error::None);
  EXPECT_EQ(ai::checkTaskSupported(info, ai::Task::Ask), Error::None);
  EXPECT_STREQ(ai::errorName(Error::ProxyOutdated), "proxy_outdated");
}

TEST(AiClientSummarize, WindowLimitFromServerIsUsed) {
  const char* body =
      R"({"api":1,"tasks":["summarize"],"max_input_chars":4000,"max_context_chars":2000,"max_question_chars":500,)"
      R"("max_window_chars":1200,"provider":"p","model":"m"})";
  ai::Info info;
  ASSERT_EQ(ai::parseInfoResponse(200, body, info), Error::None);
  EXPECT_EQ(info.limits.maxWindowChars, 1200u);
}

// C2: the "book not well known" answer is an ordinary answer, shown like any other.
TEST(AiClientSummarize, UnknownBookAnswerParsesAsAnAnswer) {
  ai::Answer a;
  ASSERT_EQ(ai::parseAskResponse(
                200, readFile(fs::path(CONTRACT_DIR) / "fixtures" / "responses" / "summarize-unknown.json"), a),
            Error::None);
  EXPECT_NE(a.text.find("not well known"), std::string::npos);
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
      {"401-unauthorized.json", Error::BadKey},           {"413-body_too_large.json", Error::TextTooLong},
      {"413-text_too_long.json", Error::TextTooLong},     {"413-context_too_long.json", Error::ClientBug},
      {"422-invalid_request.json", Error::ClientBug},     {"429-rate_limited.json", Error::RateLimited},
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
  const std::string huge =
      R"({"answer":")" + std::string(ai::MAX_RESPONSE_BYTES, 'x') + R"(","model":"m","truncated":false})";
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

// Phase 5 A1: context comes only from the page, stays within limits, keeps the selection.
TEST(AiContext, ShortPageReturnedWhole) {
  EXPECT_EQ(ai::contextAround("A short page.", "short", 2000), "A short page.");
  EXPECT_EQ(ai::contextAround("", "x", 2000), "");
}

TEST(AiContext, WindowContainsSelectionAndStaysWithinLimit) {
  std::string page;
  for (int i = 0; i < 400; i++) page += "word" + std::to_string(i) + " ";
  const std::string sel = "word350 word351";
  const std::string ctx = ai::contextAround(page, sel, 300);
  EXPECT_LE(utf8Chars(ctx), 300u);
  EXPECT_NE(ctx.find(sel), std::string::npos);
  EXPECT_NE(ctx.find("word340"), std::string::npos) << "most context comes from before the selection";
  EXPECT_EQ(ctx.find("word399"), std::string::npos) << "only a little after the selection";
}

TEST(AiContext, SelectionAtPageStartAndEnd) {
  std::string page;
  for (int i = 0; i < 400; i++) page += "w" + std::to_string(i) + " ";
  EXPECT_NE(ai::contextAround(page, "w0 w1", 200).find("w0 w1"), std::string::npos);
  EXPECT_NE(ai::contextAround(page, "w398 w399", 200).find("w398 w399"), std::string::npos);
}

TEST(AiContext, WhitespaceDifferencesStillFindSelection) {
  std::string page;
  for (int i = 0; i < 300; i++) page += "pad ";
  page += "the quick brown fox";
  for (int i = 0; i < 300; i++) page += " tail";
  const std::string ctx = ai::contextAround(page, "the quick\nbrown   fox", 200);
  EXPECT_NE(ctx.find("the quick brown fox"), std::string::npos);
}

TEST(AiContext, NotFoundFallsBackToPageStart) {
  const std::string page(5000, 'a');
  const std::string ctx = ai::contextAround(page, "zzz", 100);
  EXPECT_EQ(ctx, std::string(100, 'a'));
}

TEST(AiContext, HindiNeverSplitAndWithinLimit) {
  std::string page;
  for (int i = 0; i < 500; i++) page += "जुम्मन शेख और अलगू चौधरी में गाढ़ी मित्रता थी। ";
  for (size_t limit : {10u, 57u, 200u, 2000u}) {
    const std::string ctx = ai::contextAround(page, "मित्रता", limit);
    EXPECT_TRUE(validUtf8(ctx)) << limit;
    EXPECT_LE(utf8Chars(ctx), limit) << limit;
  }
  EXPECT_NE(ai::contextAround(page, "मित्रता", 200).find("मित्रता"), std::string::npos);
}
