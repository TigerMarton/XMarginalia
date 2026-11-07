#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/activities/reader/BookSettings.h"

static int g_checks = 0;
#define CHECK(cond)                                                      \
  do {                                                                   \
    g_checks++;                                                          \
    if (!(cond)) {                                                       \
      fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      return 1;                                                          \
    }                                                                    \
  } while (0)

int test_book_settings_defaults() {
  BookSettings bs;
  CHECK(!bs.enabled);
  CHECK(bs.fontPointSize == BookSettings::USE_GLOBAL);
  CHECK(bs.fontFamily == BookSettings::USE_GLOBAL);
  CHECK(bs.sdFontFamilyName.empty());
  CHECK(bs.lineSpacing == BookSettings::USE_GLOBAL);
  CHECK(bs.screenMargin == BookSettings::USE_GLOBAL);
  CHECK(bs.paragraphAlignment == BookSettings::USE_GLOBAL);
  CHECK(bs.paragraphIndentSpaces == BookSettings::USE_GLOBAL);
  CHECK(bs.forceParagraphIndent == BookSettings::USE_GLOBAL);
  CHECK(bs.guideDots == BookSettings::USE_GLOBAL);
  CHECK(bs.hyphenationEnabled == BookSettings::USE_GLOBAL);
  CHECK(bs.focusReadingEnabled == BookSettings::USE_GLOBAL);
  return 0;
}

int test_book_settings_json_roundtrip() {
  BookSettings bs;
  bs.enabled = true;
  bs.fontPointSize = 18;
  bs.fontFamily = 1;
  bs.sdFontFamilyName = "Baskerville";
  bs.lineSpacing = 2;
  bs.screenMargin = 15;
  bs.paragraphAlignment = 0;
  bs.paragraphIndentSpaces = 4;
  bs.forceParagraphIndent = 1;
  bs.guideDots = 1;
  bs.hyphenationEnabled = 0;
  bs.focusReadingEnabled = 1;

  std::string json = bs.toJson();
  CHECK(!json.empty());
  CHECK(json.find("\"enabled\":true") != std::string::npos);
  CHECK(json.find("\"sdFontFamilyName\":\"Baskerville\"") != std::string::npos);
  CHECK(json.find("\"fontPointSize\":18") != std::string::npos);
  CHECK(json.find("\"forceParagraphIndent\":1") != std::string::npos);
  CHECK(json.find("\"guideDots\":1") != std::string::npos);

  BookSettings loaded = BookSettings::fromJson(json);
  CHECK(loaded.enabled == bs.enabled);
  CHECK(loaded.fontPointSize == bs.fontPointSize);
  CHECK(loaded.fontFamily == bs.fontFamily);
  CHECK(loaded.sdFontFamilyName == bs.sdFontFamilyName);
  CHECK(loaded.lineSpacing == bs.lineSpacing);
  CHECK(loaded.screenMargin == bs.screenMargin);
  CHECK(loaded.paragraphAlignment == bs.paragraphAlignment);
  CHECK(loaded.paragraphIndentSpaces == bs.paragraphIndentSpaces);
  CHECK(loaded.forceParagraphIndent == bs.forceParagraphIndent);
  CHECK(loaded.guideDots == bs.guideDots);
  CHECK(loaded.hyphenationEnabled == bs.hyphenationEnabled);
  CHECK(loaded.focusReadingEnabled == bs.focusReadingEnabled);
  return 0;
}

int test_book_settings_partial_json() {
  std::string json = "{\"enabled\":true,\"fontPointSize\":20,\"guideDots\":1}";
  BookSettings bs = BookSettings::fromJson(json);
  CHECK(bs.enabled);
  CHECK(bs.fontPointSize == 20);
  CHECK(bs.guideDots == 1);
  CHECK(bs.fontFamily == BookSettings::USE_GLOBAL);
  CHECK(bs.forceParagraphIndent == BookSettings::USE_GLOBAL);
  CHECK(bs.sdFontFamilyName.empty());
  return 0;
}

int test_book_settings_effective_resolution() {
  BookSettings bs;
  // Disabled: effective values fall back to defaults
  CHECK(bs.effectiveFontPointSize() == 14);
  CHECK(!bs.effectiveGuideDots());
  CHECK(!bs.effectiveForceParagraphIndent());
  CHECK(std::string_view(bs.effectiveSdFontFamilyName()).empty());

  // Enabled with custom values
  bs.enabled = true;
  bs.fontPointSize = 22;
  bs.guideDots = 1;
  bs.forceParagraphIndent = 1;
  bs.sdFontFamilyName = "Georgia";
  CHECK(bs.effectiveFontPointSize() == 22);
  CHECK(bs.effectiveGuideDots());
  CHECK(bs.effectiveForceParagraphIndent());
  CHECK(std::string_view(bs.effectiveSdFontFamilyName()) == "Georgia");

  // Enabled but with USE_GLOBAL: falls back
  bs.fontPointSize = BookSettings::USE_GLOBAL;
  bs.guideDots = BookSettings::USE_GLOBAL;
  bs.forceParagraphIndent = BookSettings::USE_GLOBAL;
  bs.sdFontFamilyName = "";
  CHECK(bs.effectiveFontPointSize() == 14);
  CHECK(!bs.effectiveGuideDots());
  CHECK(!bs.effectiveForceParagraphIndent());
  CHECK(std::string_view(bs.effectiveSdFontFamilyName()).empty());
  return 0;
}

int test_guide_dot_spacing_math() {
  // Guide dots render in gap between words when gap >= 8 px:
  // dotX = wordRightX + gap / 2
  // dotSize = (wordAscender >= 16) ? 2 : 1
  auto computeGuideDot = [](int wordRightX, int nextWordLeftX, int ascender, int& outX, int& outSize) -> bool {
    int gap = nextWordLeftX - wordRightX;
    if (gap < 8) return false;
    outX = wordRightX + gap / 2;
    outSize = (ascender >= 16) ? 2 : 1;
    return true;
  };

  int dotX = 0, dotSize = 0;
  // Gap of 7: too small for guide dot
  CHECK(!computeGuideDot(100, 107, 14, dotX, dotSize));

  // Gap of 8: exactly at threshold, 1px dot for ascender 14
  CHECK(computeGuideDot(100, 108, 14, dotX, dotSize));
  CHECK(dotX == 104);
  CHECK(dotSize == 1);

  // Gap of 10: 2px dot for ascender 18
  CHECK(computeGuideDot(50, 60, 18, dotX, dotSize));
  CHECK(dotX == 55);
  CHECK(dotSize == 2);

  // Gap of 15: midpoint at 50 + 7 = 57
  CHECK(computeGuideDot(50, 65, 20, dotX, dotSize));
  CHECK(dotX == 57);
  CHECK(dotSize == 2);
  return 0;
}

int test_forced_paragraph_indent_logic() {
  // Forced paragraph indent rule:
  // If forced, extraParagraphSpacing = false and paragraphIndentSpaces = std::max(spaces, (uint8_t)2)
  auto resolveIndent = [](bool forceIndent, bool extraSpacing, uint8_t spaces,
                          bool& outExtraSpacing, uint8_t& outSpaces) {
    if (forceIndent) {
      outExtraSpacing = false;
      outSpaces = std::max(spaces, static_cast<uint8_t>(2));
    } else {
      outExtraSpacing = extraSpacing;
      outSpaces = spaces;
    }
  };

  bool extra = true;
  uint8_t spaces = 0;

  // Unforced: retains extra spacing and zero indent
  resolveIndent(false, true, 0, extra, spaces);
  CHECK(extra == true);
  CHECK(spaces == 0);

  // Forced: disables extra spacing, forces at least 2 indent spaces
  resolveIndent(true, true, 0, extra, spaces);
  CHECK(extra == false);
  CHECK(spaces == 2);

  // Forced with larger indent: retains the larger indent (e.g. 4 spaces)
  resolveIndent(true, true, 4, extra, spaces);
  CHECK(extra == false);
  CHECK(spaces == 4);
  return 0;
}

int main() {
  printf("Running typography host tests...\n");
  if (test_book_settings_defaults()) return 1;
  if (test_book_settings_json_roundtrip()) return 1;
  if (test_book_settings_partial_json()) return 1;
  if (test_book_settings_effective_resolution()) return 1;
  if (test_guide_dot_spacing_math()) return 1;
  if (test_forced_paragraph_indent_logic()) return 1;

  printf("All typography host tests passed! (%d checks)\n", g_checks);
  return 0;
}
