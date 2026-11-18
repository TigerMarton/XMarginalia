#include "PluginSync.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "PluginHttp.h"
#include "PluginLocations.h"
#include "network/ProtectedPaths.h"

namespace pluginsync {
namespace {

constexpr const char* kTag = "PSYNC";
constexpr size_t MAX_MANIFEST_SIZE = 8 * 1024;
constexpr size_t MAX_SYNC_RESPONSE = 24 * 1024;

struct SyncManifest {
  std::string title;
  std::string pluginDir;
  std::string tokenFile;
  std::string tokenPath;
  std::string configFile;
  std::string requireMeta;
  std::string authType;
  pluginhttp::RequestSpec authReq;
  std::string authTokenPath;

  pluginhttp::RequestSpec pullReq;
  std::string pullProgressPath;
  std::string pullLocationPath;
  bool pullScale100 = false;

  pluginhttp::RequestSpec pushReq;
};

bool hasMetaKey(const pluginhttp::Headers& meta, const std::string& keyName) {
  if (keyName.empty()) return true;
  const std::string needle = "{meta." + keyName + "}";
  for (const auto& kv : meta) {
    if (kv.first == needle && !kv.second.empty()) return true;
  }
  return false;
}

bool loadSyncManifest(const std::string& manifestPath, SyncManifest& out) {
  std::string raw;
  if (!Storage.readFileToString(kTag, manifestPath, MAX_MANIFEST_SIZE, raw)) return false;

  JsonDocument filter;
  filter["title"] = true;
  filter["token"] = true;
  filter["config"] = true;
  filter["auth"]["type"] = true;
  filter["auth"]["request"] = true;
  filter["auth"]["token_path"] = true;
  filter["sync"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, raw, DeserializationOption::Filter(filter)) != DeserializationError::Ok) return false;
  JsonVariantConst sync = doc["sync"];
  if (sync.isNull()) return false;

  const size_t slash = manifestPath.rfind('/');
  out.pluginDir = (slash == std::string::npos) ? "" : manifestPath.substr(0, slash);
  const std::string folderName = (slash == std::string::npos) ? "" : out.pluginDir.substr(out.pluginDir.rfind('/') + 1);
  out.title = doc["title"] | folderName.c_str();
  out.tokenFile = pluginhttp::inPluginDir(out.pluginDir, doc["token"]["file"] | "");
  out.tokenPath = doc["token"]["path"] | "token";
  out.configFile = pluginhttp::inPluginDir(out.pluginDir, doc["config"]["file"] | "");

  JsonVariantConst auth = doc["auth"];
  out.authType = auth["type"] | "";
  pluginhttp::readRequest(auth["request"], "POST", out.authReq);
  out.authTokenPath = auth["token_path"] | "access_token";

  out.requireMeta = sync["require_meta"] | "";
  pluginhttp::readRequest(sync["pull"], "GET", out.pullReq);
  out.pullProgressPath = sync["pull"]["progress_path"] | "progress";
  out.pullLocationPath = sync["pull"]["location_path"] | "";
  out.pullScale100 = (sync["pull"]["progress_scale"] | 1) == 100;

  pluginhttp::readRequest(sync["push"], "PATCH", out.pushReq);

  return !out.pullReq.url.empty() || !out.pushReq.url.empty();
}

void loadBookMeta(const std::string& bookPath, pluginhttp::Headers& out) {
  out.clear();
  if (bookPath.empty()) return;
  pluginhttp::loadConfigFile(bookPath + ".meta.json", out);
  for (auto& kv : out) {
    kv.first = "{meta." + kv.first + "}";
  }
}

bool findMatchingPlugin(const std::string& bookPath, SyncManifest& outManifest, pluginhttp::Headers& outMeta) {
  loadBookMeta(bookPath, outMeta);
  const auto plugins = PluginLocations::scanPlugins();
  for (const auto& entry : plugins) {
    if (!entry.hasDevice) continue;
    SyncManifest cand;
    if (!loadSyncManifest(entry.dir + "/device.json", cand)) continue;
    if (!hasMetaKey(outMeta, cand.requireMeta)) continue;
    outManifest = std::move(cand);
    return true;
  }
  return false;
}

std::string substituteCommon(std::string tpl, const std::string& token, const pluginhttp::Headers& config,
                             const pluginhttp::Headers& meta) {
  pluginhttp::substituteAll(tpl, "{token}", token);
  for (const auto& kv : config) {
    pluginhttp::substituteAll(tpl, ("{cfg." + kv.first + "}").c_str(), kv.second);
  }
  for (const auto& kv : meta) {
    pluginhttp::substituteAll(tpl, kv.first.c_str(), kv.second);
  }
  return tpl;
}

pluginhttp::Headers substituteHeaders(const pluginhttp::Headers& in, const std::string& token,
                                      const pluginhttp::Headers& config, const pluginhttp::Headers& meta) {
  pluginhttp::Headers out;
  out.reserve(in.size());
  for (const auto& kv : in) {
    out.emplace_back(kv.first, substituteCommon(kv.second, token, config, meta));
  }
  return out;
}

bool tryRefreshToken(const SyncManifest& mf, const pluginhttp::Headers& config, std::string& token) {
  if (mf.authType != "password" || mf.authReq.url.empty() || mf.tokenFile.empty()) return false;
  if (!protectedpaths::isPluginPath(mf.tokenFile)) return false;

  pluginhttp::Headers emptyMeta;
  const std::string url = substituteCommon(mf.authReq.url, token, config, emptyMeta);
  const std::string body = substituteCommon(mf.authReq.body, token, config, emptyMeta);
  const auto headers = substituteHeaders(mf.authReq.headers, token, config, emptyMeta);

  std::string minted;
  if (!pluginhttp::mintPasswordToken(nullptr, url, mf.authReq.method, body, headers, mf.authTokenPath, minted)) {
    return false;
  }
  if (!pluginhttp::saveTokenToFile(mf.tokenFile, mf.tokenPath, minted)) {
    return false;
  }
  token = std::move(minted);
  return true;
}

bool readFloatVariant(JsonVariantConst v, float& out) {
  if (v.is<float>() || v.is<double>() || v.is<int>() || v.is<long>()) {
    out = v.as<float>();
    return true;
  }
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (!s || s[0] == '\0') return false;
    char* end = nullptr;
    const float parsed = std::strtof(s, &end);
    if (end != s) {
      out = parsed;
      return true;
    }
  }
  return false;
}

}  // namespace

bool anySyncPlugin() {
  const auto plugins = PluginLocations::scanPlugins();
  for (const auto& entry : plugins) {
    if (!entry.hasDevice) continue;
    SyncManifest cand;
    if (loadSyncManifest(entry.dir + "/device.json", cand)) return true;
  }
  return false;
}

bool hasSyncForBook(const std::string& bookPath) {
  SyncManifest mf;
  pluginhttp::Headers meta;
  return findMatchingPlugin(bookPath, mf, meta);
}

std::string syncTitleForBook(const std::string& bookPath) {
  SyncManifest mf;
  pluginhttp::Headers meta;
  return findMatchingPlugin(bookPath, mf, meta) ? mf.title : std::string{};
}

bool pullProgress(const std::string& bookPath, RemoteProgress& out, std::string& errorOut) {
  out = RemoteProgress{};
  errorOut.clear();

  SyncManifest mf;
  pluginhttp::Headers meta;
  if (!findMatchingPlugin(bookPath, mf, meta) || mf.pullReq.url.empty()) {
    errorOut = "No matching sync plugin for this book";
    return false;
  }

  pluginhttp::Headers config;
  pluginhttp::loadConfigFile(mf.configFile, config);
  std::string token;
  if (!pluginhttp::loadTokenFromFile(mf.tokenFile, mf.tokenPath, token)) {
    errorOut = "Missing plugin authentication token";
    return false;
  }

  String response;
  const auto run = [&](const std::string& tok) {
    const std::string url = substituteCommon(mf.pullReq.url, tok, config, meta);
    const std::string body = substituteCommon(mf.pullReq.body, tok, config, meta);
    const auto headers = substituteHeaders(mf.pullReq.headers, tok, config, meta);
    return pluginhttp::request(nullptr, url, mf.pullReq.method, body, headers, response, MAX_SYNC_RESPONSE);
  };

  int status = run(token);
  if ((status == 401 || status == 403) && tryRefreshToken(mf, config, token)) {
    response = String();
    status = run(token);
  }

  if (status == 404) {
    out.notFound = true;
    return true;
  }
  if (status < 200 || status >= 300) {
    errorOut = (status < 0) ? "Network error reaching server" : ("HTTP " + std::to_string(status));
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response) != DeserializationError::Ok) {
    errorOut = "Invalid progress response";
    return false;
  }

  float rawProg = 0.0f;
  const JsonVariantConst progVar = pluginhttp::resolvePath(doc.as<JsonVariantConst>(), mf.pullProgressPath);
  if (!readFloatVariant(progVar, rawProg)) {
    // Item exists on server (e.g. audiobook-only progress) with no ebook progress yet.
    out.notFound = true;
    return true;
  }

  if (mf.pullScale100) rawProg /= 100.0f;
  out.percentage = std::clamp(rawProg, 0.0f, 1.0f);

  if (!mf.pullLocationPath.empty()) {
    out.location =
        pluginhttp::variantToString(pluginhttp::resolvePath(doc.as<JsonVariantConst>(), mf.pullLocationPath));
    out.spineHint = parseSpineFromCfi(out.location);
  }
  return true;
}

bool pushProgress(const std::string& bookPath, float progressFrac, const std::string& location, const bool finished,
                  std::string& errorOut) {
  errorOut.clear();

  SyncManifest mf;
  pluginhttp::Headers meta;
  if (!findMatchingPlugin(bookPath, mf, meta) || mf.pushReq.url.empty()) {
    errorOut = "No matching sync plugin for this book";
    return false;
  }

  pluginhttp::Headers config;
  pluginhttp::loadConfigFile(mf.configFile, config);
  std::string token;
  if (!pluginhttp::loadTokenFromFile(mf.tokenFile, mf.tokenPath, token)) {
    errorOut = "Missing plugin authentication token";
    return false;
  }

  progressFrac = std::clamp(progressFrac, 0.0f, 1.0f);
  char progBuf[16];
  snprintf(progBuf, sizeof(progBuf), "%.4f", static_cast<double>(progressFrac));

  const auto run = [&](const std::string& tok) {
    const std::string url = substituteCommon(mf.pushReq.url, tok, config, meta);
    std::string body = substituteCommon(mf.pushReq.body, tok, config, meta);
    pluginhttp::substituteAll(body, "{sync.progress}", progBuf);
    pluginhttp::substituteAll(body, "{sync.location}", location);
    pluginhttp::substituteAll(body, "{sync.finished}", finished ? "true" : "false");
    const auto headers = substituteHeaders(mf.pushReq.headers, tok, config, meta);
    String response;
    return pluginhttp::request(nullptr, url, mf.pushReq.method, body, headers, response, MAX_SYNC_RESPONSE);
  };

  int status = run(token);
  if ((status == 401 || status == 403) && tryRefreshToken(mf, config, token)) {
    status = run(token);
  }

  if (status < 200 || status >= 300) {
    errorOut = (status < 0) ? "Network error reaching server" : ("HTTP " + std::to_string(status));
    return false;
  }
  return true;
}

}  // namespace pluginsync
