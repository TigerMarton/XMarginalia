#pragma once

// Freestanding logic for the Anti-Spoiler E-Reader AI Companion.
// No Arduino, no GfxRenderer, no SD card dependencies so host-tests/companion/
// can compile and verify all config parsing, JSON serialization, anchor
// extraction, and response decoding on the host.

#include <cstdint>
#include <string>
#include <string_view>

namespace companion {

struct CompanionConfig {
  bool enabled = false;
  bool useHttp = false;
  std::string host;
  std::string apiKey;

  bool isValid() const { return enabled && !host.empty(); }
  std::string baseUrl() const { return (useHttp ? "http://" : "https://") + host; }
};

struct CompanionQuery {
  std::string bookUuid;
  std::string isbn;
  std::string asin;
  std::string koMd5;
  std::string title;
  std::string author;
  std::string filename;

  int spineIndex = 0;
  std::string chapterTitle;
  uint32_t visibleOffset = 0;
  bool hasVisibleOffset = false;
  float chapterProgress = 0.0f;
  float bookProgress = 0.0f;

  std::string anchorText;
  std::string selectedText;
  std::string actionType;  // "who_is", "what_is", "explain", "recap_chapter", "recap_story"
};

struct CompanionResponse {
  bool ok = false;
  std::string answer;
  std::string positionLabel;
  std::string errorMessage;
};

// Parse /.crosspoint/companion.json content.
// Supports keys: "enabled" (bool, default true when host is present),
// "host" or "url" (string, strips optional https:// or http:// scheme for BridgeHttp
// unless a full URL with port is kept in simulator mode), and "api_key" (string).
bool parseConfigJson(std::string_view json, CompanionConfig& out);

// Normalize a configured host string for BridgeHttp::Endpoint::productionHost
// (strips leading http:// or https:// and trailing slashes).
std::string normalizeBridgeHost(std::string_view rawHost);

// Extract a clean, whitespace-normalized ~maxChars anchor snippet from the
// end of the current page's visible text so the server can slice the straddling
// chunk immediately after the last visible sentence on the screen.
std::string extractPageAnchorText(std::string_view pageVisibleText, size_t maxChars = 140);

// Build the POST /v1/companion/query JSON body.
std::string buildQueryJson(const CompanionQuery& query);

// Parse the JSON response from POST /v1/companion/query (`{"ans":"...","pos":"..."}`
// or `{"error":"..."}` / `{"detail":"..."}`).
bool parseQueryResponse(std::string_view json, CompanionResponse& out);

}  // namespace companion
