#pragma once

#include <cstdint>
#include <string>

#include "AiProtocol.h"

// HTTPS client for the user's AI proxy (contract v1). Verifies the server
// against src/ai/AiTrustedRoots.h, never follows redirects, and never logs the
// API key, book text or answers. Call only while Wi-Fi is connected; each call
// blocks until the request finishes or times out.
namespace ai {

struct CallStats {
  int status = 0;              // HTTP status, 0 when no response
  uint32_t ms = 0;             // wall time of the request
  uint32_t minFreeHeap = 0;    // lowest internal free heap seen around the call
  uint32_t minLargestBlock = 0;
};

class Client {
 public:
  // Uses AI_SETTINGS (server URL and API key).
  Error info(Info& out, CallStats* stats = nullptr);
  Error ask(const AskRequest& req, const Limits& limits, Answer& out, CallStats* stats = nullptr);

 private:
  Error request(const char* method, const char* path, const std::string& body, std::string& response,
                CallStats& stats);
};

}  // namespace ai
