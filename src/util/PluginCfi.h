#pragma once

#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

namespace pluginsync {

inline std::string buildSpineCfi(const int spineIndex) {
  if (spineIndex < 0) return "";
  char buf[48];
  // EPUB CFI spine step is 2 * (spineIndex + 1) inside package spine (/6).
  snprintf(buf, sizeof(buf), "epubcfi(/6/%d!/4/1:0)", (spineIndex + 1) * 2);
  return buf;
}

inline int parseSpineFromCfi(const std::string_view cfi) {
  constexpr std::string_view prefix = "epubcfi(/6/";
  if (cfi.size() <= prefix.size() || cfi.compare(0, prefix.size(), prefix) != 0) return -1;
  size_t pos = prefix.size();
  int step = 0;
  bool anyDigit = false;
  while (pos < cfi.size() && std::isdigit(static_cast<unsigned char>(cfi[pos]))) {
    anyDigit = true;
    step = step * 10 + (cfi[pos] - '0');
    if (step > 200000) return -1;
    ++pos;
  }
  if (!anyDigit || step < 2 || (step % 2) != 0) return -1;
  return (step / 2) - 1;
}

}  // namespace pluginsync
