#include "AiClient.h"

// clang-format off
// HttpDownloader.h pulls Arduino/SdFat first (lwip macro order); see OtaUpdater.cpp.
#include "network/HttpDownloader.h"
#include <HalMemory.h>
#include <Logging.h>
#include <SecureHttpClient.h>
// clang-format on

#include <algorithm>

#include "AiSettingsStore.h"
#include "AiTrustedRoots.h"
#include "fork/ForkIdentity.h"
#include "network/WifiPowerSaveGuard.h"

namespace ai {

namespace {

constexpr uint32_t REQUEST_TIMEOUT_MS = 30000;

void noteHeap(CallStats& stats) {
  const auto heap = HalMemory::getInternalHeap();
  stats.minFreeHeap = std::min<uint32_t>(stats.minFreeHeap, heap.freeBytes);
  stats.minLargestBlock = std::min<uint32_t>(stats.minLargestBlock, heap.largestBlockBytes);
}

}  // namespace

Error Client::request(const char* method, const char* path, const std::string& body, std::string& response,
                      CallStats& stats) {
  response.clear();
  stats = CallStats{0, 0, UINT32_MAX, UINT32_MAX};
  if (!AI_SETTINGS.isConfigured()) return Error::NotConfigured;

  noteHeap(stats);
  if (stats.minFreeHeap < HttpDownloader::MIN_TLS_FREE_HEAP ||
      stats.minLargestBlock < HttpDownloader::MIN_TLS_MAX_ALLOC) {
    LOG_ERR("AI", "Not enough heap for TLS: free %u, largest %u", stats.minFreeHeap, stats.minLargestBlock);
    return Error::OutOfMemory;
  }

  WifiPowerSaveGuard psGuard;
  const uint32_t started = millis();
  int status;
  bool overflow = false;
  {
    freeink::SecureHttpClient http;
    if (!http.begin(AI_SETTINGS.getServerUrl() + path)) {
      LOG_ERR("AI", "Bad server URL");
      return Error::NotConfigured;
    }
    http.setTimeout(REQUEST_TIMEOUT_MS);
    // Verify the server; the rest of the firmware calls setInsecure().
    http.setCACert(TRUSTED_ROOTS_PEM);
    http.setFollowRedirects(0);
    http.setUserAgent(FORK_NAME "/" CROSSPOINT_VERSION);
    http.addHeader("Authorization", "Bearer " + AI_SETTINGS.getApiKey());
    http.addHeader("Accept", "application/json");
    if (!body.empty()) http.addHeader("Content-Type", "application/json");

    status = http.sendRequest(method, reinterpret_cast<const uint8_t*>(body.data()), body.size(),
                              [&](const uint8_t* data, size_t len) {
                                noteHeap(stats);
                                if (response.size() + len > MAX_RESPONSE_BYTES) {
                                  overflow = true;
                                  return false;  // stop reading an oversized body
                                }
                                response.append(reinterpret_cast<const char*>(data), len);
                                return true;
                              });
    noteHeap(stats);
  }
  stats.ms = millis() - started;
  stats.status = status > 0 ? status : 0;
  noteHeap(stats);
  LOG_INF("AI", "%s %s -> %d in %u ms, body %u B, min free %u, min block %u", method, path, stats.status, stats.ms,
          static_cast<unsigned>(response.size()), stats.minFreeHeap, stats.minLargestBlock);
  if (overflow) {
    response.clear();
    return Error::BadResponse;
  }
  return Error::None;
}

Error Client::info(Info& out, CallStats* stats) {
  CallStats local;
  CallStats& s = stats ? *stats : local;
  std::string response;
  const Error e = request("GET", "/v1/info", std::string(), response, s);
  if (e != Error::None) return e;
  return parseInfoResponse(s.status, response, out);
}

Error Client::ask(const AskRequest& req, const Limits& limits, Answer& out, CallStats* stats) {
  CallStats local;
  CallStats& s = stats ? *stats : local;
  std::string response;
  const Error e = request("POST", "/v1/ask", buildAskBody(req, limits), response, s);
  if (e != Error::None) return e;
  const Error parsed = parseAskResponse(s.status, response, out);
  if (parsed != Error::None) LOG_ERR("AI", "Ask failed: %s", errorName(parsed));
  return parsed;
}

}  // namespace ai
