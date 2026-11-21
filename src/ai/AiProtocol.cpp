#include "AiProtocol.h"

#include <ArduinoJson.h>

#include <cstring>

namespace ai {

namespace {

const char* taskName(Task t) {
  switch (t) {
    case Task::Define:
      return "define";
    case Task::Translate:
      return "translate";
    case Task::Ask:
      return "ask";
    case Task::Summarize:
      return "summarize";
    case Task::Explain:
    default:
      return "explain";
  }
}

// Length of the UTF-8 sequence starting with `lead`, or 1 for an invalid byte.
size_t sequenceLength(const unsigned char lead) {
  if (lead < 0x80) return 1;
  if ((lead & 0xE0) == 0xC0) return 2;
  if ((lead & 0xF0) == 0xE0) return 3;
  if ((lead & 0xF8) == 0xF0) return 4;
  return 1;
}

// Errors are only trusted when the body is the proxy's JSON {"error": "<code>"}.
// Google's front end can answer 401/404/5xx with HTML, which is not a bad key.
Error mapError(const int status, const std::string_view body) {
  if (status <= 0) return Error::Connection;
  JsonDocument doc;
  if (deserializeJson(doc, body.data(), body.size()) || !doc["error"].is<const char*>()) {
    return Error::ServerUnavailable;
  }
  const char* code = doc["error"].as<const char*>();
  if (status == 401 && strcmp(code, "unauthorized") == 0) return Error::BadKey;
  if (status == 429 && strcmp(code, "rate_limited") == 0) return Error::RateLimited;
  if (status == 502 && strcmp(code, "provider_error") == 0) return Error::ProviderFailed;
  if (status == 413 && (strcmp(code, "text_too_long") == 0 || strcmp(code, "body_too_large") == 0)) {
    return Error::TextTooLong;
  }
  if ((status == 413 && strcmp(code, "context_too_long") == 0) ||
      (status == 422 && strcmp(code, "invalid_request") == 0)) {
    return Error::ClientBug;
  }
  return Error::ServerUnavailable;
}

}  // namespace

const char* errorName(const Error e) {
  switch (e) {
    case Error::None:
      return "none";
    case Error::NotConfigured:
      return "not_configured";
    case Error::NoWifi:
      return "no_wifi";
    case Error::OutOfMemory:
      return "out_of_memory";
    case Error::Connection:
      return "connection";
    case Error::BadKey:
      return "bad_key";
    case Error::TextTooLong:
      return "text_too_long";
    case Error::RateLimited:
      return "rate_limited";
    case Error::ProviderFailed:
      return "provider_failed";
    case Error::ServerUnavailable:
      return "server_unavailable";
    case Error::BadResponse:
      return "bad_response";
    case Error::ClientBug:
      return "client_bug";
    case Error::ProxyOutdated:
      return "proxy_outdated";
  }
  return "unknown";
}

std::string_view truncateUtf8(const std::string_view s, const size_t maxChars) {
  size_t pos = 0;
  size_t chars = 0;
  while (pos < s.size() && chars < maxChars) {
    const size_t len = sequenceLength(static_cast<unsigned char>(s[pos]));
    if (pos + len > s.size()) break;  // cut-off sequence at the end: drop it
    pos += len;
    chars++;
  }
  return s.substr(0, pos);
}

std::string_view tailUtf8(const std::string_view s, const size_t maxChars) {
  size_t chars = 0;
  size_t pos = s.size();
  while (pos > 0 && chars < maxChars) {
    pos--;
    if ((static_cast<unsigned char>(s[pos]) & 0xC0) != 0x80) chars++;
  }
  // `pos` is on a lead byte (or 0) unless the text starts with a broken sequence: skip those bytes.
  while (pos < s.size() && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) pos++;
  return s.substr(pos);
}

Error checkTaskSupported(const Info& info, const Task task) {
  if (task == Task::Summarize && !info.supportsSummarize) return Error::ProxyOutdated;
  return Error::None;
}

std::string buildAskBody(const AskRequest& req, const Limits& limits) {
  JsonDocument doc;
  doc["task"] = taskName(req.task);
  // ArduinoJson copies std::string; string_view needs an explicit copy.
  if (req.task == Task::Summarize) {
    // No selected text: the reading position sits between the two windows.
    if (!req.chapter.empty()) doc["chapter"] = std::string(truncateUtf8(req.chapter, 200));
    doc["text_before"] = std::string(tailUtf8(req.textBefore, limits.maxWindowChars));
    if (!req.textAfter.empty()) doc["text_after"] = std::string(truncateUtf8(req.textAfter, limits.maxWindowChars));
  } else {
    doc["text"] = std::string(truncateUtf8(req.text, limits.maxInputChars));
    if (!req.context.empty()) doc["context"] = std::string(truncateUtf8(req.context, limits.maxContextChars));
    if (req.task == Task::Ask && !req.question.empty()) {
      doc["question"] = std::string(truncateUtf8(req.question, limits.maxQuestionChars));
    }
  }
  if (!req.language.empty()) doc["language"] = std::string(truncateUtf8(req.language, 40));
  if (!req.bookTitle.empty()) doc["book_title"] = std::string(truncateUtf8(req.bookTitle, 200));
  if (!req.bookAuthor.empty()) doc["book_author"] = std::string(truncateUtf8(req.bookAuthor, 200));
  std::string body;
  serializeJson(doc, body);
  return body;
}

Error parseAskResponse(const int status, const std::string_view body, Answer& out) {
  out = Answer{};
  if (status != 200) return mapError(status, body);
  if (body.size() > MAX_RESPONSE_BYTES) return Error::BadResponse;
  JsonDocument doc;
  if (deserializeJson(doc, body.data(), body.size())) return Error::BadResponse;
  if (!doc["answer"].is<const char*>() || !doc["model"].is<const char*>() || !doc["truncated"].is<bool>()) {
    return Error::BadResponse;
  }
  const char* answer = doc["answer"].as<const char*>();
  if (answer[0] == '\0') return Error::BadResponse;
  out.text = answer;
  out.model = doc["model"].as<const char*>();
  out.truncated = doc["truncated"].as<bool>();
  return Error::None;
}

Error parseInfoResponse(const int status, const std::string_view body, Info& out) {
  out = Info{};
  if (status != 200) return mapError(status, body);
  if (body.size() > MAX_RESPONSE_BYTES) return Error::BadResponse;
  JsonDocument doc;
  if (deserializeJson(doc, body.data(), body.size())) return Error::BadResponse;
  if (doc["api"] != 1 || !doc["max_input_chars"].is<uint32_t>() || !doc["max_context_chars"].is<uint32_t>()) {
    return Error::BadResponse;
  }
  out.api = 1;
  out.limits.maxInputChars = doc["max_input_chars"].as<uint32_t>();
  out.limits.maxContextChars = doc["max_context_chars"].as<uint32_t>();
  out.limits.maxQuestionChars = doc["max_question_chars"] | 500u;
  if (out.limits.maxInputChars == 0) return Error::BadResponse;
  out.limits.maxWindowChars = doc["max_window_chars"] | 3000u;
  if (doc["tasks"].is<JsonArrayConst>()) {
    for (JsonVariantConst t : doc["tasks"].as<JsonArrayConst>()) {
      if (t.is<const char*>() && strcmp(t.as<const char*>(), "summarize") == 0) out.supportsSummarize = true;
    }
  }
  out.provider = doc["provider"] | "";
  out.model = doc["model"] | "";
  return Error::None;
}

}  // namespace ai
