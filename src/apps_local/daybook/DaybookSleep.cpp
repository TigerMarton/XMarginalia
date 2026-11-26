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

struct Row {
  std::string text;
  bool heading = false;
  bool bold = false;
};

}  // namespace

namespace daybook {

bool drawAgenda(GfxRenderer& renderer) {
  const std::time_t now = std::time(nullptr);
  if (!clockfloor::trusted(static_cast<int64_t>(now))) return false;
  std::tm t{};
  localtime_r(&now, &t);
  const cal::Day today = cal::daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

  std::string text;
  std::vector<cal::Event> events;
  std::vector<cal::Task> tasks;
  if (readText(kEventsPath, cal::kMaxFileBytes, text)) cal::parse(text, events);
  if (readText(kTasksPath, cal::kMaxTaskFileBytes, text)) cal::parseTasks(text, tasks);

  const bool twelve = SETTINGS.clockFormat == 1;
  std::vector<cal::Occurrence> occ;
  cal::occurrencesOn(events, today, occ);
  std::vector<size_t> overdue, due;
  cal::overdueTasks(tasks, today, overdue);
  cal::tasksDueOn(tasks, today, due);

  std::vector<Row> rows;
  if (!occ.empty()) {
    rows.push_back({"EVENTS", true, true});
    for (const auto& o : occ) {
      const cal::Event& e = events[o.event];
      std::string line = e.allDay ? std::string("All day") : cal::formatMinutes(e.startMin, twelve);
      line += "  " + cal::foldForDisplay(e.title);
      rows.push_back({line, false, false});
    }
  }
  if (!overdue.empty()) {
    rows.push_back({"OVERDUE", true, true});
    for (const size_t i : overdue) {
      const cal::Date d = cal::civilFromDays(tasks[i].due);
      char buf[24];
      std::snprintf(buf, sizeof buf, "%s %d", kMon[d.m - 1], d.d);
      rows.push_back({"[ ] " + cal::foldForDisplay(tasks[i].title) + "  (" + buf + ")", false, true});
    }
  }
  std::vector<size_t> dueOpen;
  for (const size_t i : due) {
    if (!tasks[i].done) dueOpen.push_back(i);
  }
  if (!dueOpen.empty()) {
    rows.push_back({"TO-DO TODAY", true, true});
    for (const size_t i : dueOpen) rows.push_back({"[ ] " + cal::foldForDisplay(tasks[i].title), false, false});
  }
  if (rows.empty()) rows.push_back({"Nothing planned", false, false});

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
  const int lhRow = renderer.getLineHeight(fBody) + 4;
  const int lhSec = renderer.getLineHeight(fBoldSmall) + 6;
  const int maxH = H - 2 * margin;

  // How many rows fit. A "+N more" row is kept in reserve when any are cut.
  int used = pad * 2 + lhHead;
  size_t shown = 0;
  for (; shown < rows.size(); shown++) {
    const int need = rows[shown].heading ? lhSec : lhRow;
    if (used + need + lhRow > maxH && shown + 1 < rows.size()) break;
    if (used + need > maxH) break;
    used += need;
  }
  const size_t hidden = rows.size() - shown;
  if (hidden > 0) used += lhRow;
  const int boxH = std::min(used + pad, maxH);

  renderer.fillRect(margin, margin, boxW, boxH, false);
  renderer.drawRect(margin, margin, boxW, boxH, 2, true);

  int y = margin + pad;
  char head[40];
  std::snprintf(head, sizeof head, "%s, %d %s", kDow[cal::weekday(today)], t.tm_mday, kMon[t.tm_mon]);
  renderer.drawText(fBold, margin + pad, y, head, true, EpdFontFamily::BOLD);
  y += lhHead;
  renderer.drawLine(margin + pad, y - 3, margin + boxW - pad, y - 3, true);

  for (size_t i = 0; i < shown; i++) {
    const Row& r = rows[i];
    if (r.heading) {
      renderer.drawText(fBoldSmall, margin + pad, y, r.text.c_str(), true, EpdFontFamily::BOLD);
      y += lhSec;
    } else {
      const std::string fit = renderer.truncatedText(fBody, r.text.c_str(), textW);
      renderer.drawText(r.bold ? fBold : fBody, margin + pad, y, fit.c_str(), true,
                        r.bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      y += lhRow;
    }
  }
  if (hidden > 0) {
    char more[24];
    std::snprintf(more, sizeof more, "+%u more", static_cast<unsigned>(hidden));
    renderer.drawText(fSmall, margin + pad, y, more, true);
  }
  LOG_INF(kTag, "sleep agenda: %u row(s), %u hidden", static_cast<unsigned>(rows.size()), static_cast<unsigned>(hidden));
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
