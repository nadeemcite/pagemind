#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// AI proxy API contract v1 (cloud/ai-proxy/contract/README.md): request
// building and response parsing, with no network code so it runs in host tests.
namespace ai {

enum class Task : uint8_t { Explain, Define, Translate, Ask };

enum class Error : uint8_t {
  None,
  NotConfigured,      // AI off, or no URL / key
  NoWifi,             // could not join a network
  OutOfMemory,        // not enough heap to start TLS
  Connection,         // DNS, TCP, TLS (incl. an untrusted certificate) or timeout: no HTTP response
  BadKey,             // 401 {"error":"unauthorized"}
  TextTooLong,        // 413 text_too_long / body_too_large
  RateLimited,        // 429
  ProviderFailed,     // 502 provider_error
  ServerUnavailable,  // any other status, or a body without a contract error code
  BadResponse,        // 200 that does not match the contract
  ClientBug,          // 413 context_too_long, 422 invalid_request
};

const char* errorName(Error e);

struct AskRequest {
  Task task = Task::Explain;
  std::string text;
  std::string context;
  std::string question;
  std::string language;
  std::string bookTitle;
  std::string bookAuthor;
};

struct Limits {
  uint32_t maxInputChars = 4000;
  uint32_t maxContextChars = 2000;
  uint32_t maxQuestionChars = 500;
};

struct Info {
  int api = 0;
  Limits limits;
  std::string provider;
  std::string model;
};

struct Answer {
  std::string text;
  std::string model;
  bool truncated = false;
};

// Largest response body the device reads; a bigger one is BadResponse.
constexpr size_t MAX_RESPONSE_BYTES = 16 * 1024;

// Cut `s` to at most `maxChars` Unicode code points (the unit the server counts),
// never splitting a UTF-8 sequence. Invalid bytes count as one character each.
std::string_view truncateUtf8(std::string_view s, size_t maxChars);

// JSON body for POST /v1/ask. Text fields are trimmed to `limits`; empty
// optional fields are left out.
std::string buildAskBody(const AskRequest& req, const Limits& limits);

// Map an HTTP status and body to an answer or an error. status <= 0 means no
// HTTP response was received.
Error parseAskResponse(int status, std::string_view body, Answer& out);
Error parseInfoResponse(int status, std::string_view body, Info& out);

}  // namespace ai
