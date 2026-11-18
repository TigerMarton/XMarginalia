#include "CompanionClient.h"

#include <Arduino.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Utf8.h>
#include <WiFi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../DevMode.h"
#include "../../WifiCredentialStore.h"
#include "../../network/WifiPowerSaveGuard.h"
#include "../../util/PluginLocations.h"
#include "../bridge/BridgeHttp.h"

#if defined(FREEINK_NET_WOLFSSL)
#include <SecureHttpClient.h>
#endif

#if defined(ESP_PLATFORM)
#include <esp_sntp.h>
#endif

namespace companion {
namespace {

constexpr const char* kTag = "COMPANION";
constexpr unsigned long kWifiConnectTimeoutMs = 15000;

#if defined(FREEINK_NET_WOLFSSL)
// wolfSSL uses the default allocator (which prefers PSRAM via wolfPsramMalloc on
// ESP32-S3 boards). Check the default heap pool plus a small internal SRAM floor
// for lwIP TCP socket buffers so we never falsely reject queries when PSRAM has
// megabytes free.
constexpr size_t kMinDefaultFreeForTls = 35000;
constexpr size_t kMinDefaultBlockForTls = 20000;
constexpr size_t kMinInternalFreeForSocket = 4096;
constexpr size_t kMinInternalBlockForSocket = 2048;

const char* loadOptionalCaRoots() {
  static std::string sdRoots;
  static bool probed = false;
  if (!probed) {
    probed = true;
    HalFile file;
    if (Storage.exists(CompanionClient::kRootsOverridePath) &&
        Storage.openFileForRead(kTag, CompanionClient::kRootsOverridePath, file)) {
      const size_t size = file.size();
      if (size > 512 && size < 65536) {
        sdRoots.resize(size);
        if (file.read(reinterpret_cast<uint8_t*>(sdRoots.data()), size) == static_cast<int>(size) &&
            sdRoots.find("-----BEGIN CERTIFICATE-----") != std::string::npos) {
          LOG_INF(kTag, "Using custom SD root bundle (%u bytes)", static_cast<unsigned>(size));
        } else {
          sdRoots.clear();
        }
      }
    }
  }
  return sdRoots.empty() ? nullptr : sdRoots.c_str();
}

bool checkCompanionHeap(std::string& errorOut) {
  const auto defHeap = HalMemory::getDefaultHeap();
  const auto intHeap = HalMemory::getInternalHeap();
  LOG_INF(kTag, "Heap check: default(free=%zu, max=%zu) internal(free=%zu, max=%zu)", defHeap.freeBytes,
          defHeap.largestBlockBytes, intHeap.freeBytes, intHeap.largestBlockBytes);
  if (defHeap.freeBytes < kMinDefaultFreeForTls || defHeap.largestBlockBytes < kMinDefaultBlockForTls ||
      intHeap.freeBytes < kMinInternalFreeForSocket || intHeap.largestBlockBytes < kMinInternalBlockForSocket) {
    LOG_ERR(kTag, "Insufficient heap for Companion query: defFree=%zu defBlock=%zu intFree=%zu intBlock=%zu",
            defHeap.freeBytes, defHeap.largestBlockBytes, intHeap.freeBytes, intHeap.largestBlockBytes);
    char buf[160];
    std::snprintf(buf, sizeof(buf), "Low memory for AI query (int=%uKB/%uKB). Try closing and reopening the book.",
                  static_cast<unsigned>(intHeap.freeBytes / 1024),
                  static_cast<unsigned>(intHeap.largestBlockBytes / 1024));
    errorOut = buf;
    return false;
  }
  return true;
}
#endif

bool connectFromSavedWifi(bool& broughtUpByUs, std::string& errorOut) {
  broughtUpByUs = false;
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

#if !defined(FREEINK_NET_WOLFSSL)
  // Simulator uses host curl directly when CROSSPLAY_COMPANION_URL is set.
  if (std::getenv("CROSSPLAY_COMPANION_URL") != nullptr) {
    return true;
  }
#endif

  WIFI_STORE.loadFromFile();
  std::string ssid = WIFI_STORE.getLastConnectedSsid();
  std::optional<WifiCredential> cred;
  if (!ssid.empty()) {
    cred = WIFI_STORE.findCredential(ssid);
  }
  if (!cred && WIFI_STORE.getCredentialCount() > 0) {
    cred = WIFI_STORE.getCredentialAt(0);
  }
  if (!cred || cred->ssid.empty()) {
    errorOut = "No saved Wi-Fi network found. Connect to Wi-Fi once in Settings first.";
    return false;
  }

  LOG_INF(kTag, "Waking Wi-Fi for companion query (SSID: %s)", cred->ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(50);
  WiFi.setSleep(false);
  WiFi.begin(cred->ssid.c_str(), cred->password.empty() ? nullptr : cred->password.c_str());
  broughtUpByUs = true;

  const unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startMs >= kWifiConnectTimeoutMs) {
      LOG_ERR(kTag, "Wi-Fi connect timed out after %lu ms", kWifiConnectTimeoutMs);
#if defined(ESP_PLATFORM)
      esp_sntp_stop();
#endif
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      broughtUpByUs = false;
      errorOut = "Could not connect to saved Wi-Fi network.";
      return false;
    }
    delay(40);
  }
  return true;
}

void teardownWifiIfOwned(const bool broughtUpByUs) {
  if (!broughtUpByUs || devmode::holdsRadio()) return;
  LOG_INF(kTag, "Tearing down Wi-Fi radio after companion query");
#if defined(ESP_PLATFORM)
  esp_sntp_stop();
#endif
  WiFi.disconnect(true, true);
  delay(50);
  WiFi.mode(WIFI_OFF);
  delay(50);
}

}  // namespace

bool CompanionClient::loadConfig(CompanionConfig& out) {
  out = CompanionConfig{};
  HalFile file;
  if (Storage.exists(kConfigPath) && Storage.openFileForRead(kTag, kConfigPath, file)) {
    const size_t size = file.size();
    if (size > 0 && size <= 4096) {
      std::string raw(size, '\0');
      if (file.read(reinterpret_cast<uint8_t*>(raw.data()), size) == static_cast<int>(size)) {
        if (parseConfigJson(raw, out)) {
          return true;
        }
      }
    }
  }

  // Optional fallback: if no /.crosspoint/companion.json exists, allow
  // reading companion_url / companion_host from the Audiobookshelf plugin config.
  const std::string absDir = PluginLocations::findPluginDir("audiobookshelf");
  if (!absDir.empty()) {
    std::string raw;
    if (Storage.readFileToString(kTag, absDir + "/config.json", 4096, raw) && parseConfigJson(raw, out)) {
      return true;
    }
  }

#if !defined(FREEINK_NET_WOLFSSL)
  if (const char* envUrl = std::getenv("CROSSPLAY_COMPANION_URL")) {
    out.enabled = true;
    out.useHttp = std::strncmp(envUrl, "http://", 7) == 0;
    out.host = normalizeBridgeHost(envUrl);
    if (const char* envKey = std::getenv("CROSSPLAY_COMPANION_KEY")) {
      out.apiKey = envKey;
    } else {
      out.apiKey = "sim-companion-key";
    }
    return out.isValid();
  }
#endif
  return false;
}

bool CompanionClient::isConfigured() {
  CompanionConfig cfg;
  return loadConfig(cfg);
}

CompanionResponse CompanionClient::query(const CompanionQuery& req) {
  CompanionResponse result;
  CompanionConfig cfg;
  if (!loadConfig(cfg)) {
    result.errorMessage = "Configure /.crosspoint/companion.json on the SD card to use AI Companion.";
    return result;
  }

  {
    const auto preDef = HalMemory::getDefaultHeap();
    const auto preInt = HalMemory::getInternalHeap();
    LOG_INF(kTag, "Pre-WiFi heap: default(free=%zu, max=%zu) internal(free=%zu, max=%zu)", preDef.freeBytes,
            preDef.largestBlockBytes, preInt.freeBytes, preInt.largestBlockBytes);
  }

  bool broughtWifiUp = false;
  std::string wifiError;
  if (!connectFromSavedWifi(broughtWifiUp, wifiError)) {
    result.errorMessage = std::move(wifiError);
    return result;
  }

  struct RadioTeardownGuard {
    bool owned;
    ~RadioTeardownGuard() { teardownWifiIfOwned(owned); }
  } radioGuard{broughtWifiUp};

  WifiPowerSaveGuard psGuard;

  const std::string body = buildQueryJson(req);
  std::string rawResponse;
  std::string transportMessage;
  int status = 0;

#if defined(FREEINK_NET_WOLFSSL)
  if (!checkCompanionHeap(transportMessage)) {
    result.errorMessage = std::move(transportMessage);
    return result;
  }

  freeink::SecureHttpClient http;
  if (const char* roots = loadOptionalCaRoots()) {
    http.setCACert(roots);
  } else {
    http.setInsecure();
  }
  http.setTimeout(60000);
  http.setFollowRedirects(2);
  http.setReuse(false);
  http.setUserAgent("XMarginalia-Companion");

  const std::string url = cfg.baseUrl() + "/api/ereader/v1/query";
  if (!http.begin(url)) {
    result.errorMessage = "Invalid Companion Bridge URL in /.crosspoint/companion.json.";
    return result;
  }
  http.addHeader("Content-Type", "application/json");
  if (!cfg.apiKey.empty()) {
    http.addHeader("Authorization", std::string("Bearer ") + cfg.apiKey);
    http.addHeader("X-API-Key", cfg.apiKey);
  }

  LOG_INF(kTag, "POST %s (%u bytes)", url.c_str(), static_cast<unsigned>(body.size()));
  status = http.sendRequest("POST", reinterpret_cast<const uint8_t*>(body.data()), body.size());
  if (status <= 0) {
    LOG_ERR(kTag, "POST %s failed: %d", url.c_str(), status);
    http.end();
    result.errorMessage = "Could not reach Companion Bridge (" + cfg.host + "). Check Wi-Fi and server URL.";
    return result;
  }
  rawResponse = http.getString();
  http.end();
#else
  const bridge::Endpoint endpoint = {
      cfg.host.c_str(),
      kTag,
      "CROSSPLAY_COMPANION_URL",
      kRootsOverridePath,
  };

  bridge::Headers headers;
  headers.add("Content-Type", "application/json");
  if (!cfg.apiKey.empty()) {
    headers.add("X-API-Key", cfg.apiKey);
  }

  status = bridge::request(endpoint, "POST", "/api/ereader/v1/query", cfg.apiKey,
                           reinterpret_cast<const uint8_t*>(body.data()), body.size(), rawResponse, transportMessage,
                           &headers);
  if (status == 0) {
    result.errorMessage = transportMessage.empty() ? "Could not reach Companion Bridge." : transportMessage;
    return result;
  }
#endif

  if (status == 401 || status == 403) {
    result.errorMessage = "Companion API key rejected (HTTP " + std::to_string(status) + ").";
    return result;
  }
  if (status == 429) {
    result.errorMessage = "Companion rate limit reached. Try again in a moment.";
    return result;
  }

  if (parseQueryResponse(rawResponse, result)) {
    result.answer = utf8FoldTypography(result.answer);
    result.positionLabel = utf8FoldTypography(result.positionLabel);
    return result;
  }

  // Only accept parsed JSON error messages when rawResponse actually looks like
  // a JSON object; otherwise include the HTTP status and snippet so proxy / SSO
  // redirects or HTML 404/502 pages are immediately obvious.
  const bool looksLikeJson = !rawResponse.empty() && rawResponse.front() == '{';
  if (looksLikeJson && !result.errorMessage.empty() &&
      result.errorMessage != "Unexpected response from Companion Bridge.") {
    result.errorMessage = utf8FoldTypography(result.errorMessage);
    return result;
  }

  std::string snippet;
  snippet.reserve(80);
  for (const char c : rawResponse) {
    if (snippet.size() >= 80) break;
    if (c == '\r' || c == '\n' || c == '\t') {
      if (!snippet.empty() && snippet.back() != ' ') snippet.push_back(' ');
    } else if (static_cast<unsigned char>(c) >= 0x20) {
      snippet.push_back(c);
    }
  }
  LOG_ERR(kTag, "Unexpected HTTP %d from %s: %s", status, cfg.host.c_str(), snippet.c_str());
  result.errorMessage =
      "HTTP " + std::to_string(status) + " from " + cfg.host + (snippet.empty() ? "." : (": " + snippet));
  return result;
}

}  // namespace companion
