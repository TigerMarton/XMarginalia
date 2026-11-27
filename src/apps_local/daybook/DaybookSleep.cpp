#include "DaybookSleep.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>

#include <FsHelpers.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "../../CrossPointSettings.h"
#include "../ClockFloor.h"
#include "../ui/ToyboxFonts.h"
#include "DaybookCore.h"

namespace cal = daybook;

namespace {

constexpr const char* kTag = "DBK";
constexpr const char* kEventsPath = "/.crosspoint/calendar/events.txt";
constexpr const char* kTasksPath = "/.crosspoint/calendar/tasks.txt";
constexpr const char* kSettingsPath = "/.crosspoint/calendar/settings.txt";

constexpr const char* kDow[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
constexpr const char* kMon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool readText(const char* path, size_t maxBytes, std::string& out) {
  out.clear();
  if (Storage.exists(path)) return Storage.readFileToString(kTag, path, maxBytes, out);
  const std::string tmp = std::string(path) + ".tmp";
  return Storage.exists(tmp.c_str()) && Storage.readFileToString(kTag, tmp, maxBytes, out);
}

}  // namespace

namespace daybook {

bool drawAgenda(GfxRenderer& renderer) {
  const std::time_t now = std::time(nullptr);
  if (!clockfloor::trusted(static_cast<int64_t>(now))) return false;
  std::tm t{};
  localtime_r(&now, &t);
  const cal::Day today = cal::daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  const int nowMin = t.tm_hour * 60 + t.tm_min;

  std::string text;
  std::vector<cal::Event> events;
  std::vector<cal::Task> tasks;
  if (readText(kEventsPath, cal::kMaxFileBytes, text)) cal::parse(text, events);
  if (readText(kTasksPath, cal::kMaxTaskFileBytes, text)) cal::parseTasks(text, tasks);

  const bool twelve = SETTINGS.clockFormat == 1;
  cal::Agenda agenda = cal::buildAgenda(events, tasks, today, nowMin, twelve);
  if (!agendaTomorrow()) agenda.tomorrow.clear();

  toybox::ensureFonts(renderer);
  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  const int fBody = toybox::kReadingFontId;
  const int fSmall = toybox::kReadingSmallFontId;
  const int fBold = toybox::kReadingBoldFontId;
  const int fBoldSmall = toybox::kReadingBoldSmallFontId;

  const int margin = 16;
  const int pad = 12;
  const int boxW = W - 2 * margin;
  const int textW = boxW - 2 * pad;
  const int lhHead = renderer.getLineHeight(fBold) + 6;
  const int maxH = H - 2 * margin;

  cal::AgendaMetrics m;
  m.section = renderer.getLineHeight(fBoldSmall) + 6;
  m.item = renderer.getLineHeight(fBody) + 4;
  m.strong = renderer.getLineHeight(fBold) + 4;
  m.rule = 14;
  m.tomSection = renderer.getLineHeight(fBoldSmall) + 4;
  m.tomItem = renderer.getLineHeight(fSmall) + 3;
  m.more = renderer.getLineHeight(fSmall) + 3;
  m.avail = maxH - 2 * pad - lhHead;

  const std::string tomorrowTitle = [&] {
    const cal::Date d = cal::civilFromDays(today + 1);
    char buf[48];
    std::snprintf(buf, sizeof buf, "TOMORROW  %s %d %s", kDow[cal::weekday(today + 1)], d.d, kMon[d.m - 1]);
    return std::string(buf);
  }();
  const std::vector<cal::AgendaLine> lines = cal::layoutAgenda(agenda, m, tomorrowTitle, nowMin >= cal::kEveningMinute);

  int used = pad * 2 + lhHead;
  for (const cal::AgendaLine& l : lines) {
    switch (l.style) {
      case cal::AgendaStyle::Section: used += m.section; break;
      case cal::AgendaStyle::Item: used += m.item; break;
      case cal::AgendaStyle::Strong: used += m.strong; break;
      case cal::AgendaStyle::Rule: used += m.rule; break;
      case cal::AgendaStyle::TomSection: used += m.tomSection; break;
      case cal::AgendaStyle::TomItem: used += m.tomItem; break;
      case cal::AgendaStyle::More: used += m.more; break;
    }
  }
  const int boxH = std::min(used + pad, maxH);
  const int boxY = agendaBottom() ? H - margin - boxH : margin;

  renderer.fillRect(margin, boxY, boxW, boxH, false);
  renderer.drawRect(margin, boxY, boxW, boxH, 2, true);

  const int x = margin + pad;
  int y = boxY + pad;
  char head[40];
  std::snprintf(head, sizeof head, "%s, %d %s", kDow[cal::weekday(today)], t.tm_mday, kMon[t.tm_mon]);
  renderer.drawText(fBold, x, y, head, true, EpdFontFamily::BOLD);
  // When it was drawn: the screen does not change while the device sleeps.
  const std::string stamp = "as of " + cal::formatMinutes(nowMin, twelve);
  renderer.drawText(fSmall, margin + boxW - pad - renderer.getTextWidth(fSmall, stamp.c_str()),
                    y + renderer.getLineHeight(fBold) - renderer.getLineHeight(fSmall), stamp.c_str(), true);
  y += lhHead;
  renderer.drawLine(x, y - 3, margin + boxW - pad, y - 3, true);

  for (const cal::AgendaLine& l : lines) {
    switch (l.style) {
      case cal::AgendaStyle::Section:
        renderer.drawText(fBoldSmall, x, y, l.text.c_str(), true, EpdFontFamily::BOLD);
        y += m.section;
        break;
      case cal::AgendaStyle::Item: {
        const std::string fit = renderer.truncatedText(fBody, l.text.c_str(), textW);
        renderer.drawText(fBody, x, y, fit.c_str(), true);
        y += m.item;
        break;
      }
      case cal::AgendaStyle::Strong:
        renderer.drawText(fBold, x, y, l.text.c_str(), true, EpdFontFamily::BOLD);
        y += m.strong;
        break;
      case cal::AgendaStyle::Rule:
        renderer.drawLine(x, y + m.rule / 2, margin + boxW - pad, y + m.rule / 2, true);
        y += m.rule;
        break;
      case cal::AgendaStyle::TomSection:
        renderer.drawText(fBoldSmall, x, y, l.text.c_str(), true, EpdFontFamily::BOLD);
        y += m.tomSection;
        break;
      case cal::AgendaStyle::TomItem: {
        const std::string fit = renderer.truncatedText(fSmall, l.text.c_str(), textW);
        renderer.drawText(fSmall, x, y, fit.c_str(), true);
        y += m.tomItem;
        break;
      }
      case cal::AgendaStyle::More:
        renderer.drawText(fSmall, x, y, l.text.c_str(), true);
        y += m.more;
        break;
    }
  }
  LOG_INF(kTag, "sleep agenda: %u line(s), %u for tomorrow", static_cast<unsigned>(lines.size()),
          static_cast<unsigned>(agenda.tomorrow.size()));
  return true;
}

namespace {
// The value after "key=" on its own line of the settings file, or "".
std::string settingValue(const std::string& text, const char* key) {
  const std::string tag = std::string(key) + "=";
  size_t at = text.find(tag);
  while (at != std::string::npos && at > 0 && text[at - 1] != '\n') at = text.find(tag, at + 1);
  if (at == std::string::npos) return {};
  const size_t end = text.find('\n', at);
  return text.substr(at + tag.size(), end == std::string::npos ? std::string::npos : end - (at + tag.size()));
}
}  // namespace

bool agendaEnabled() {
  std::string text;
  return readText(kSettingsPath, 512, text) && settingValue(text, "agenda") == "1";
}

bool agendaTomorrow() {
  std::string text;
  if (!readText(kSettingsPath, 512, text)) return true;  // on until it is switched off
  return settingValue(text, "tomorrow") != "0";
}

bool agendaBottom() {
  std::string text;
  return readText(kSettingsPath, 512, text) && settingValue(text, "bottom") == "1";
}

std::string wallpaperPath() {
  std::string text;
  if (!readText(kSettingsPath, 512, text)) return {};
  return settingValue(text, "wall");
}

std::vector<std::string> wallpaperChoices() {
  std::vector<std::string> out;
  for (const char* dir : {"/", "/sleepscreen", "/sleep", "/.sleep", "/wallpapers"}) {
    for (const auto& name : Storage.listFiles(dir, 120)) {
      const std::string n(name.c_str());
      if (n.empty() || n[0] == '.' || !FsHelpers::hasBmpExtension(n)) continue;
      const std::string full = (dir[1] == '\0' ? std::string("/") : std::string(dir) + "/") + n;
      if (full.size() > 120) continue;
      out.push_back(full);
      if (out.size() >= 60) break;
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace daybook
