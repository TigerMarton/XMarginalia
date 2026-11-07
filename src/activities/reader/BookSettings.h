#pragma once

#include <cstdint>
#include <string>

class BookSettings {
 public:
  static constexpr uint8_t USE_GLOBAL = 0xFF;

  bool enabled = false;

  uint8_t fontPointSize = USE_GLOBAL;
  uint8_t fontFamily = USE_GLOBAL;
  std::string sdFontFamilyName;
  uint8_t lineSpacing = USE_GLOBAL;
  uint8_t screenMargin = USE_GLOBAL;
  uint8_t paragraphAlignment = USE_GLOBAL;
  uint8_t paragraphIndentSpaces = USE_GLOBAL;
  uint8_t forceParagraphIndent = USE_GLOBAL;  // 0 = off, 1 = on, USE_GLOBAL = use global setting
  uint8_t guideDots = USE_GLOBAL;             // 0 = off, 1 = on, USE_GLOBAL = use global setting
  uint8_t hyphenationEnabled = USE_GLOBAL;    // 0 = off, 1 = on, USE_GLOBAL = use global setting
  uint8_t focusReadingEnabled = USE_GLOBAL;   // 0 = off, 1 = on, USE_GLOBAL = use global setting

  static std::string settingsFilePath(const std::string& cachePath);
  static BookSettings load(const std::string& cachePath);
  bool save(const std::string& cachePath) const;
  static bool remove(const std::string& cachePath);

  // Effective resolution against global settings
  uint8_t effectiveFontPointSize() const;
  uint8_t effectiveFontFamily() const;
  const char* effectiveSdFontFamilyName() const;
  uint8_t effectiveLineSpacing() const;
  uint8_t effectiveScreenMargin() const;
  uint8_t effectiveParagraphAlignment() const;
  uint8_t effectiveParagraphIndentSpaces() const;
  bool effectiveForceParagraphIndent() const;
  bool effectiveGuideDots() const;
  bool effectiveHyphenation() const;
  bool effectiveFocusReading() const;

  // JSON serialization
  std::string toJson() const;
  static BookSettings fromJson(const std::string& json);
};
