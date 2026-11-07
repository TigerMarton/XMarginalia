#include "BookSettings.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
#include <HalStorage.h>
#include <Logging.h>

#include "CrossPointSettings.h"
#include "util/FileContentEquals.h"
#else
#include <fstream>
#include <sstream>
#endif

namespace {
constexpr const char* SETTINGS_FILE_NAME = "book_settings.json";

// Helper to extract string value from simple JSON
bool extractJsonString(const std::string& json, const char* key, std::string& out) {
  std::string search = std::string("\"") + key + "\":\"";
  size_t start = json.find(search);
  if (start == std::string::npos) return false;
  start += search.length();
  size_t end = json.find('"', start);
  if (end == std::string::npos) return false;
  out = json.substr(start, end - start);
  return true;
}

// Helper to extract boolean value from simple JSON
bool extractJsonBool(const std::string& json, const char* key, bool& out) {
  std::string search = std::string("\"") + key + "\":";
  size_t start = json.find(search);
  if (start == std::string::npos) return false;
  start += search.length();
  while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
  if (json.compare(start, 4, "true") == 0) {
    out = true;
    return true;
  }
  if (json.compare(start, 5, "false") == 0) {
    out = false;
    return true;
  }
  return false;
}

// Helper to extract uint8_t value from simple JSON
bool extractJsonUint8(const std::string& json, const char* key, uint8_t& out) {
  std::string search = std::string("\"") + key + "\":";
  size_t start = json.find(search);
  if (start == std::string::npos) return false;
  start += search.length();
  while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
  if (start >= json.length()) return false;
  char* endPtr = nullptr;
  long val = strtol(json.c_str() + start, &endPtr, 10);
  if (endPtr == json.c_str() + start) return false;
  out = static_cast<uint8_t>(val);
  return true;
}
}  // namespace

std::string BookSettings::settingsFilePath(const std::string& cachePath) {
  if (cachePath.empty()) return "";
  return cachePath + "/" + SETTINGS_FILE_NAME;
}

std::string BookSettings::toJson() const {
  char buf[256];
  const int written =
      snprintf(buf, sizeof(buf),
               "{\"enabled\":%s,\"fontPointSize\":%u,\"fontFamily\":%u,\"sdFontFamilyName\":\"%s\",\"lineSpacing\":%u,"
               "\"screenMargin\":%u,\"paragraphAlignment\":%u,\"paragraphIndentSpaces\":%u,\"forceParagraphIndent\":%u,"
               "\"guideDots\":%u,\"hyphenationEnabled\":%u,\"focusReadingEnabled\":%u}",
               enabled ? "true" : "false", static_cast<unsigned>(fontPointSize), static_cast<unsigned>(fontFamily),
               sdFontFamilyName.c_str(), static_cast<unsigned>(lineSpacing), static_cast<unsigned>(screenMargin),
               static_cast<unsigned>(paragraphAlignment), static_cast<unsigned>(paragraphIndentSpaces),
               static_cast<unsigned>(forceParagraphIndent), static_cast<unsigned>(guideDots),
               static_cast<unsigned>(hyphenationEnabled), static_cast<unsigned>(focusReadingEnabled));
  if (written > 0 && static_cast<size_t>(written) < sizeof(buf)) {
    return std::string(buf, written);
  }
  std::string s;
  s.resize(written > 0 ? written + 1 : 384);
  snprintf(&s[0], s.size(),
           "{\"enabled\":%s,\"fontPointSize\":%u,\"fontFamily\":%u,\"sdFontFamilyName\":\"%s\",\"lineSpacing\":%u,"
           "\"screenMargin\":%u,\"paragraphAlignment\":%u,\"paragraphIndentSpaces\":%u,\"forceParagraphIndent\":%u,"
           "\"guideDots\":%u,\"hyphenationEnabled\":%u,\"focusReadingEnabled\":%u}",
           enabled ? "true" : "false", static_cast<unsigned>(fontPointSize), static_cast<unsigned>(fontFamily),
           sdFontFamilyName.c_str(), static_cast<unsigned>(lineSpacing), static_cast<unsigned>(screenMargin),
           static_cast<unsigned>(paragraphAlignment), static_cast<unsigned>(paragraphIndentSpaces),
           static_cast<unsigned>(forceParagraphIndent), static_cast<unsigned>(guideDots),
           static_cast<unsigned>(hyphenationEnabled), static_cast<unsigned>(focusReadingEnabled));
  if (written > 0) s.resize(written);
  return s;
}

BookSettings BookSettings::fromJson(const std::string& json) {
  BookSettings settings;
  if (json.empty()) return settings;

  extractJsonBool(json, "enabled", settings.enabled);
  extractJsonUint8(json, "fontPointSize", settings.fontPointSize);
  extractJsonUint8(json, "fontFamily", settings.fontFamily);
  extractJsonString(json, "sdFontFamilyName", settings.sdFontFamilyName);
  extractJsonUint8(json, "lineSpacing", settings.lineSpacing);
  extractJsonUint8(json, "screenMargin", settings.screenMargin);
  extractJsonUint8(json, "paragraphAlignment", settings.paragraphAlignment);
  extractJsonUint8(json, "paragraphIndentSpaces", settings.paragraphIndentSpaces);
  extractJsonUint8(json, "forceParagraphIndent", settings.forceParagraphIndent);
  extractJsonUint8(json, "guideDots", settings.guideDots);
  extractJsonUint8(json, "hyphenationEnabled", settings.hyphenationEnabled);
  extractJsonUint8(json, "focusReadingEnabled", settings.focusReadingEnabled);

  return settings;
}

BookSettings BookSettings::load(const std::string& cachePath) {
  const std::string path = settingsFilePath(cachePath);
  if (path.empty()) return BookSettings{};

#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  HalFile file;
  if (!Storage.openFileForRead("BOOK_SET", path.c_str(), file)) {
    return BookSettings{};
  }
  const size_t size = file.fileSize();
  if (size == 0 || size > 2048) {
    file.close();
    return BookSettings{};
  }
  std::string json;
  json.resize(size);
  const int n = file.read(reinterpret_cast<uint8_t*>(&json[0]), size);
  file.close();
  if (n <= 0 || static_cast<size_t>(n) != size) {
    return BookSettings{};
  }
  return fromJson(json);
#else
  std::ifstream f(path);
  if (!f.is_open()) return BookSettings{};
  std::stringstream ss;
  ss << f.rdbuf();
  return fromJson(ss.str());
#endif
}

bool BookSettings::save(const std::string& cachePath) const {
  const std::string path = settingsFilePath(cachePath);
  if (path.empty()) return false;

  const std::string json = toJson();

#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  // Wear leveling: check if file content matches existing on disk
  if (fileContentEquals("BOOK_SET", path.c_str(), reinterpret_cast<const uint8_t*>(json.data()), json.size())) {
    return true;
  }

  const std::string tmpPath = path + ".tmp";
  HalFile file;
  if (!Storage.openFileForWrite("BOOK_SET", tmpPath.c_str(), file)) {
    LOG_ERR("BOOK_SET", "Could not open %s for write", tmpPath.c_str());
    return false;
  }
  const size_t written = file.write(reinterpret_cast<const uint8_t*>(json.data()), json.size());
  file.close();

  if (written != json.size()) {
    LOG_ERR("BOOK_SET", "Short write to %s", tmpPath.c_str());
    Storage.remove(tmpPath.c_str());
    return false;
  }

  if (Storage.exists(path.c_str()) && !Storage.remove(path.c_str())) {
    LOG_ERR("BOOK_SET", "Could not replace %s", path.c_str());
    Storage.remove(tmpPath.c_str());
    return false;
  }

  if (!Storage.rename(tmpPath.c_str(), path.c_str())) {
    LOG_ERR("BOOK_SET", "Could not rename %s to %s", tmpPath.c_str(), path.c_str());
    Storage.remove(tmpPath.c_str());
    return false;
  }
  return true;
#else
  std::ofstream f(path);
  if (!f.is_open()) return false;
  f << json;
  return true;
#endif
}

bool BookSettings::remove(const std::string& cachePath) {
  const std::string path = settingsFilePath(cachePath);
  if (path.empty()) return false;

#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (Storage.exists(path.c_str())) {
    return Storage.remove(path.c_str());
  }
  return true;
#else
  return ::remove(path.c_str()) == 0;
#endif
}

uint8_t BookSettings::effectiveFontPointSize() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && fontPointSize != USE_GLOBAL && fontPointSize >= 8 && fontPointSize <= 36) {
    return fontPointSize;
  }
  return SETTINGS.fontPointSize;
#else
  return (enabled && fontPointSize != USE_GLOBAL) ? fontPointSize : 14;
#endif
}

uint8_t BookSettings::effectiveFontFamily() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && fontFamily != USE_GLOBAL) {
    return fontFamily;
  }
  return SETTINGS.fontFamily;
#else
  return (enabled && fontFamily != USE_GLOBAL) ? fontFamily : 0;
#endif
}

const char* BookSettings::effectiveSdFontFamilyName() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && (!sdFontFamilyName.empty() || fontFamily != USE_GLOBAL)) {
    return sdFontFamilyName.c_str();
  }
  return SETTINGS.sdFontFamilyName;
#else
  return (enabled && !sdFontFamilyName.empty()) ? sdFontFamilyName.c_str() : "";
#endif
}

uint8_t BookSettings::effectiveLineSpacing() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && lineSpacing != USE_GLOBAL) {
    return lineSpacing;
  }
  return SETTINGS.lineSpacing;
#else
  return (enabled && lineSpacing != USE_GLOBAL) ? lineSpacing : 2;
#endif
}

uint8_t BookSettings::effectiveScreenMargin() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && screenMargin != USE_GLOBAL) {
    return screenMargin;
  }
  return SETTINGS.screenMargin;
#else
  return (enabled && screenMargin != USE_GLOBAL) ? screenMargin : 5;
#endif
}

uint8_t BookSettings::effectiveParagraphAlignment() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && paragraphAlignment != USE_GLOBAL) {
    return paragraphAlignment;
  }
  return SETTINGS.paragraphAlignment;
#else
  return (enabled && paragraphAlignment != USE_GLOBAL) ? paragraphAlignment : 0;
#endif
}

uint8_t BookSettings::effectiveParagraphIndentSpaces() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && paragraphIndentSpaces != USE_GLOBAL) {
    return paragraphIndentSpaces;
  }
  return SETTINGS.paragraphIndentSpaces;
#else
  return (enabled && paragraphIndentSpaces != USE_GLOBAL) ? paragraphIndentSpaces : 2;
#endif
}

bool BookSettings::effectiveForceParagraphIndent() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && forceParagraphIndent != USE_GLOBAL) {
    return forceParagraphIndent != 0;
  }
  return SETTINGS.forceParagraphIndent != 0;
#else
  return (enabled && forceParagraphIndent != USE_GLOBAL) ? (forceParagraphIndent != 0) : false;
#endif
}

bool BookSettings::effectiveGuideDots() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && guideDots != USE_GLOBAL) {
    return guideDots != 0;
  }
  return SETTINGS.guideDotsEnabled != 0;
#else
  return (enabled && guideDots != USE_GLOBAL) ? (guideDots != 0) : false;
#endif
}

bool BookSettings::effectiveHyphenation() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && hyphenationEnabled != USE_GLOBAL) {
    return hyphenationEnabled != 0;
  }
  return SETTINGS.hyphenationEnabled != 0;
#else
  return (enabled && hyphenationEnabled != USE_GLOBAL) ? (hyphenationEnabled != 0) : false;
#endif
}

bool BookSettings::effectiveFocusReading() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO)
  if (enabled && focusReadingEnabled != USE_GLOBAL) {
    return focusReadingEnabled != 0;
  }
  return SETTINGS.focusReadingEnabled != 0;
#else
  return (enabled && focusReadingEnabled != USE_GLOBAL) ? (focusReadingEnabled != 0) : false;
#endif
}
