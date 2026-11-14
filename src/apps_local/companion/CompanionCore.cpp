#include "CompanionCore.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace companion {
namespace {

bool isSpaceChar(const char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

void skipSpaces(const std::string_view s, size_t& pos) {
  while (pos < s.size() && isSpaceChar(s[pos])) ++pos;
}

void appendJsonEscaped(std::string& out, const std::string_view value) {
  out.push_back('"');
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"':
        out.append("\\\"");
        break;
      case '\\':
        out.append("\\\\");
        break;
      case '\b':
        out.append("\\b");
        break;
      case '\f':
        out.append("\\f");
        break;
      case '\n':
        out.append("\\n");
        break;
      case '\r':
        out.append("\\r");
        break;
      case '\t':
        out.append("\\t");
        break;
      default:
        if (ch < 0x20) {
          char hex[7];
          std::snprintf(hex, sizeof(hex), "\\u%04x", static_cast<unsigned>(ch));
          out.append(hex);
        } else {
          out.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
  out.push_back('"');
}

void appendKeyValueString(std::string& out, const char* key, const std::string_view value, bool& first) {
  if (!first) out.push_back(',');
  first = false;
  appendJsonEscaped(out, key);
  out.push_back(':');
  appendJsonEscaped(out, value);
}

void appendKeyValueInt(std::string& out, const char* key, const long long value, bool& first) {
  if (!first) out.push_back(',');
  first = false;
  appendJsonEscaped(out, key);
  char buf[32];
  std::snprintf(buf, sizeof(buf), ":%lld", value);
  out.append(buf);
}

void appendKeyValueFloat(std::string& out, const char* key, const float value, bool& first) {
  if (!first) out.push_back(',');
  first = false;
  appendJsonEscaped(out, key);
  char buf[32];
  std::snprintf(buf, sizeof(buf), ":%.4f", static_cast<double>(value));
  out.append(buf);
}

// Parse a JSON string token starting at s[pos] == '"'. Advances pos past closing quote.
bool parseJsonStringToken(const std::string_view s, size_t& pos, std::string& out) {
  out.clear();
  if (pos >= s.size() || s[pos] != '"') return false;
  ++pos;
  while (pos < s.size()) {
    const char c = s[pos++];
    if (c == '"') return true;
    if (c == '\\') {
      if (pos >= s.size()) return false;
      const char esc = s[pos++];
      switch (esc) {
        case '"':
          out.push_back('"');
          break;
        case '\\':
          out.push_back('\\');
          break;
        case '/':
          out.push_back('/');
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u': {
          if (pos + 4 > s.size()) return false;
          unsigned code = 0;
          for (int i = 0; i < 4; ++i) {
            const char hc = s[pos++];
            code <<= 4;
            if (hc >= '0' && hc <= '9') {
              code |= static_cast<unsigned>(hc - '0');
            } else if (hc >= 'a' && hc <= 'f') {
              code |= static_cast<unsigned>(hc - 'a' + 10);
            } else if (hc >= 'A' && hc <= 'F') {
              code |= static_cast<unsigned>(hc - 'A' + 10);
            } else {
              return false;
            }
          }
          if (code <= 0x7F) {
            out.push_back(static_cast<char>(code));
          } else if (code <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | ((code >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          } else {
            out.push_back(static_cast<char>(0xE0 | ((code >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          }
          break;
        }
        default:
          out.push_back(esc);
          break;
      }
    } else {
      out.push_back(c);
    }
  }
  return false;
}

// Find a top-level key in a flat JSON object and return the position of its value.
bool findJsonValuePos(const std::string_view json, const std::string_view targetKey, size_t& valuePos) {
  size_t pos = 0;
  skipSpaces(json, pos);
  if (pos >= json.size() || json[pos] != '{') return false;
  ++pos;

  while (pos < json.size()) {
    skipSpaces(json, pos);
    if (pos >= json.size() || json[pos] == '}') return false;
    if (json[pos] == ',') {
      ++pos;
      continue;
    }
    std::string key;
    if (!parseJsonStringToken(json, pos, key)) return false;
    skipSpaces(json, pos);
    if (pos >= json.size() || json[pos] != ':') return false;
    ++pos;
    skipSpaces(json, pos);
    if (pos >= json.size()) return false;

    if (key == targetKey) {
      valuePos = pos;
      return true;
    }

    // Skip over value
    if (json[pos] == '"') {
      std::string dummy;
      if (!parseJsonStringToken(json, pos, dummy)) return false;
    } else if (json[pos] == '{' || json[pos] == '[') {
      const char openCh = json[pos];
      const char closeCh = (openCh == '{') ? '}' : ']';
      int depth = 0;
      while (pos < json.size()) {
        if (json[pos] == '"') {
          std::string dummy;
          if (!parseJsonStringToken(json, pos, dummy)) return false;
          continue;
        }
        if (json[pos] == openCh) ++depth;
        if (json[pos] == closeCh) {
          --depth;
          if (depth == 0) {
            ++pos;
            break;
          }
        }
        ++pos;
      }
    } else {
      while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && !isSpaceChar(json[pos])) {
        ++pos;
      }
    }
  }
  return false;
}

bool extractJsonStringField(const std::string_view json, const std::string_view key, std::string& out) {
  size_t pos = 0;
  if (!findJsonValuePos(json, key, pos)) return false;
  if (pos >= json.size() || json[pos] != '"') return false;
  return parseJsonStringToken(json, pos, out);
}

bool extractJsonBoolField(const std::string_view json, const std::string_view key, bool& out) {
  size_t pos = 0;
  if (!findJsonValuePos(json, key, pos)) return false;
  if (json.substr(pos).starts_with("true")) {
    out = true;
    return true;
  }
  if (json.substr(pos).starts_with("false")) {
    out = false;
    return true;
  }
  return false;
}

}  // namespace

std::string normalizeBridgeHost(std::string_view rawHost) {
  while (!rawHost.empty() && isSpaceChar(rawHost.front())) rawHost.remove_prefix(1);
  while (!rawHost.empty() && isSpaceChar(rawHost.back())) rawHost.remove_suffix(1);

  if (rawHost.starts_with("https://")) {
    rawHost.remove_prefix(8);
  } else if (rawHost.starts_with("http://")) {
    rawHost.remove_prefix(7);
  }

  while (!rawHost.empty() && rawHost.back() == '/') {
    rawHost.remove_suffix(1);
  }
  const size_t slashPos = rawHost.find('/');
  if (slashPos != std::string_view::npos) {
    rawHost = rawHost.substr(0, slashPos);
  }
  return std::string(rawHost);
}

bool parseConfigJson(const std::string_view json, CompanionConfig& out) {
  out = CompanionConfig{};
  std::string rawHost;
  if (!extractJsonStringField(json, "host", rawHost)) {
    extractJsonStringField(json, "url", rawHost);
  }
  std::string_view trimmedHost = rawHost;
  while (!trimmedHost.empty() && isSpaceChar(trimmedHost.front())) trimmedHost.remove_prefix(1);
  out.useHttp = trimmedHost.starts_with("http://");
  out.host = normalizeBridgeHost(rawHost);
  extractJsonStringField(json, "api_key", out.apiKey);
  if (out.apiKey.empty()) {
    extractJsonStringField(json, "apiKey", out.apiKey);
  }

  bool explicitEnabled = true;
  if (extractJsonBoolField(json, "enabled", explicitEnabled)) {
    out.enabled = explicitEnabled && !out.host.empty();
  } else {
    out.enabled = !out.host.empty();
  }
  return out.isValid();
}

std::string extractPageAnchorText(const std::string_view pageVisibleText, const size_t maxChars) {
  if (pageVisibleText.empty() || maxChars == 0) return {};

  // Collapse runs of whitespace into single spaces so the anchor matches the
  // server's normalized paragraph text.
  std::string normalized;
  normalized.reserve(std::min<size_t>(pageVisibleText.size(), 1024));
  bool prevSpace = true;
  for (const char c : pageVisibleText) {
    if (isSpaceChar(c)) {
      if (!prevSpace) {
        normalized.push_back(' ');
        prevSpace = true;
      }
    } else {
      normalized.push_back(c);
      prevSpace = false;
    }
  }
  if (!normalized.empty() && normalized.back() == ' ') {
    normalized.pop_back();
  }
  if (normalized.size() <= maxChars) return normalized;

  // Take the trailing slice of the page (the exact bottom of the screen that
  // the user has read up to) and align to a UTF-8 / word boundary.
  size_t start = normalized.size() - maxChars;
  while (start < normalized.size() && (static_cast<uint8_t>(normalized[start]) & 0xC0) == 0x80) {
    ++start;
  }
  const size_t nextSpace = normalized.find(' ', start);
  if (nextSpace != std::string::npos && nextSpace + 20 < normalized.size()) {
    start = nextSpace + 1;
  }
  return normalized.substr(start);
}

std::string buildQueryJson(const CompanionQuery& query) {
  std::string out;
  out.reserve(512);
  out.push_back('{');
  bool first = true;

  appendKeyValueString(out, "book_uuid", query.bookUuid, first);
  appendKeyValueString(out, "isbn", query.isbn, first);
  appendKeyValueString(out, "asin", query.asin, first);
  appendKeyValueString(out, "ko_md5", query.koMd5, first);
  appendKeyValueString(out, "title", query.title, first);
  appendKeyValueString(out, "author", query.author, first);
  appendKeyValueString(out, "filename", query.filename, first);
  appendKeyValueInt(out, "spine_index", query.spineIndex, first);
  appendKeyValueString(out, "chapter_title", query.chapterTitle, first);
  if (query.hasVisibleOffset) {
    appendKeyValueInt(out, "visible_offset", static_cast<long long>(query.visibleOffset), first);
  }
  appendKeyValueFloat(out, "chapter_progress", std::clamp(query.chapterProgress, 0.0f, 1.0f), first);
  appendKeyValueFloat(out, "book_progress", std::clamp(query.bookProgress, 0.0f, 1.0f), first);
  appendKeyValueString(out, "anchor_text", query.anchorText, first);
  appendKeyValueString(out, "selected_text", query.selectedText, first);
  appendKeyValueString(out, "action_type", query.actionType, first);

  out.push_back('}');
  return out;
}

bool parseQueryResponse(const std::string_view json, CompanionResponse& out) {
  out = CompanionResponse{};
  if (extractJsonStringField(json, "ans", out.answer) && !out.answer.empty()) {
    extractJsonStringField(json, "pos", out.positionLabel);
    out.ok = true;
    return true;
  }
  if (extractJsonStringField(json, "error", out.errorMessage) && !out.errorMessage.empty()) {
    out.ok = false;
    return false;
  }
  if (extractJsonStringField(json, "detail", out.errorMessage) && !out.errorMessage.empty()) {
    out.ok = false;
    return false;
  }
  out.errorMessage = "Unexpected response from Companion Bridge.";
  out.ok = false;
  return false;
}

}  // namespace companion
