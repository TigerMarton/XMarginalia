#include "DaybookActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#ifndef SIMULATOR
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <variant>

#include "../../CrossPointSettings.h"
#include "../../activities/ActivityResult.h"
#include "../../activities/util/KeyboardEntryActivity.h"
#include "DaybookPlugin.h"
#include "DaybookSleep.h"
#include "../../components/HeaderBackTapTarget.h"
#include "../../components/UITheme.h"
#include "../ClockFloor.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"

namespace cal = daybook;

namespace {

constexpr const char* kTag = "CAL";
constexpr const char* kDir = "/.crosspoint/calendar";
constexpr const char* kEventsPath = "/.crosspoint/calendar/events.txt";
constexpr const char* kSettingsPath = "/.crosspoint/calendar/settings.txt";
constexpr const char* kTasksPath = "/.crosspoint/calendar/tasks.txt";
constexpr const char* kListsPath = "/.crosspoint/calendar/lists.txt";
constexpr size_t kSettingsBytes = 512;
constexpr int kM = 12;  // side margin

constexpr const char* kDow[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
constexpr const char* kDowLong[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
constexpr const char* kMonthName[] = {"January", "February", "March",     "April",   "May",      "June",
                                      "July",    "August",   "September", "October", "November", "December"};
constexpr const char* kMonthShort[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
constexpr const char* kRepeatName[] = {"Never", "Daily", "Weekly", "Monthly", "Yearly"};

constexpr int kBody = toybox::kReadingFontId;
constexpr int kSmall = toybox::kReadingSmallFontId;
constexpr int kBold = toybox::kReadingBoldFontId;
constexpr int kBoldSmall = toybox::kReadingBoldSmallFontId;

// Same two-step as Weather: a write lands beside itself and is renamed, so a
// power cut leaves the old file, never a half one.
bool readText(const char* path, size_t maxBytes, std::string& out) {
  out.clear();
  if (Storage.exists(path)) return Storage.readFileToString(kTag, path, maxBytes, out);
  const std::string tmp = std::string(path) + ".tmp";
  return Storage.exists(tmp.c_str()) && Storage.readFileToString(kTag, tmp, maxBytes, out);
}

bool writeText(const char* path, const std::string& text) {
  Storage.ensureDirectoryExists(kDir);
  const std::string tmp = std::string(path) + ".tmp";
  {
    HalFile file;
    if (!Storage.openFileForWrite(kTag, tmp.c_str(), file)) return false;
    if (file.write(text.data(), text.size()) != text.size()) {
      file.close();
      Storage.remove(tmp.c_str());
      return false;
    }
    file.flush();
  }
  if (!Storage.replaceFile(tmp.c_str(), path)) {
    if (Storage.exists(path)) Storage.remove(tmp.c_str());
    return false;
  }
  return true;
}

using daybook::addMonths;

}  // namespace

std::string DaybookActivity::weekdayName(const cal::Day day) { return kDowLong[cal::weekday(day)]; }

// "Thu 26 Nov 2026", or "Thu 26 Nov" without the year.
namespace {
std::string dayTitle(const cal::Day day, const bool withYear = true) {
  const cal::Date d = cal::civilFromDays(day);
  char buf[40];
  if (withYear) {
    std::snprintf(buf, sizeof buf, "%s %d %s %d", kDow[cal::weekday(day)], d.d, kMonthShort[d.m - 1], d.y);
  } else {
    std::snprintf(buf, sizeof buf, "%s %d %s", kDow[cal::weekday(day)], d.d, kMonthShort[d.m - 1]);
  }
  return buf;
}

// The time column of a row: "All day", "09:00-10:00", or the span of a multi-day event.
std::string shortWhen(const cal::Event& ev, const cal::Occurrence& o, const bool twelveHour) {
  if (o.end > o.start) return cal::formatSpan(o.start, o.end);
  if (ev.allDay) return "All day";
  std::string s = cal::formatMinutes(ev.startMin, twelveHour);
  if (ev.endMin > ev.startMin) s += "-" + cal::formatMinutes(ev.endMin, twelveHour);
  return s;
}


const char* unitName(const cal::Repeat r, const bool plural) {
  switch (r) {
    case cal::Repeat::Daily:
      return plural ? "days" : "day";
    case cal::Repeat::Weekly:
      return plural ? "weeks" : "week";
    case cal::Repeat::Monthly:
      return plural ? "months" : "month";
    case cal::Repeat::Yearly:
      return plural ? "years" : "year";
    default:
      return "";
  }
}

}  // namespace

std::unique_ptr<Activity> DaybookActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<DaybookActivity>(renderer, mappedInput);
}

// --- Lifecycle and storage ------------------------------------------------------------------

void DaybookActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);  // a wake lands here directly, not through the Shelf screen that registers them
  if (started_) {  // back from the keyboard screen: keep everything as it was
    changed();
    return;
  }
  started_ = true;
  loadAll();
  refreshToday();
  selected_ = today_;
  openMonthOf(today_);
  view_ = home();
  hitsReady_ = false;
  requestUpdate();
}

void DaybookActivity::loadAll() {
  std::string text;
  events_.clear();
  if (readText(kEventsPath, cal::kMaxFileBytes, text)) cal::parse(text, events_);
  if (readText(kSettingsPath, kSettingsBytes, text)) {
    int first = firstDay_, weeks = weekNums_ ? 1 : 0, list = listMode_ ? 1 : 0;
    std::sscanf(text.c_str(), "first=%d\nweeks=%d\nlist=%d", &first, &weeks, &list);
    firstDay_ = (first == 0 || first == 6) ? first : 1;
    weekNums_ = weeks != 0;
    listMode_ = list != 0;
    wallpaper_ = daybook::wallpaperPath();
    agenda_ = daybook::agendaEnabled();
  }
  tasks_.clear();
  lists_.clear();
  if (readText(kTasksPath, cal::kMaxTaskFileBytes, text)) cal::parseTasks(text, tasks_);
  if (readText(kListsPath, cal::kMaxTaskFileBytes, text)) cal::parseLists(text, lists_);
  cal::repairLists(lists_, tasks_);  // the default list always exists
  clampList();
}

bool DaybookActivity::saveEvents() { return writeText(kEventsPath, cal::serialize(events_)); }
bool DaybookActivity::saveTasks() { return writeText(kTasksPath, cal::serializeTasks(tasks_)); }
bool DaybookActivity::saveLists() { return writeText(kListsPath, cal::serializeLists(lists_)); }

void DaybookActivity::saveSettings() {
  char buf[200];
  std::snprintf(buf, sizeof buf, "first=%d\nweeks=%d\nlist=%d\nagenda=%d\nwall=%s\n", firstDay_, weekNums_ ? 1 : 0,
                listMode_ ? 1 : 0, agenda_ ? 1 : 0, wallpaper_.c_str());
  writeText(kSettingsPath, buf);
}

void DaybookActivity::refreshToday() {
  const std::time_t now = std::time(nullptr);
  clockKnown_ = clockfloor::trusted(static_cast<int64_t>(now));
  if (clockKnown_) {
    std::tm t{};
    localtime_r(&now, &t);
    today_ = cal::daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  } else {
    today_ = cal::daysFromCivil(2026, 1, 1);  // the clock is not set: no "today" is drawn
  }
}

bool DaybookActivity::twelveHour() const { return SETTINGS.clockFormat == 1; }

void DaybookActivity::openMonthOf(const cal::Day day) {
  const cal::Date d = cal::civilFromDays(day);
  year_ = d.y;
  month_ = d.m;
}

void DaybookActivity::changed() {
  hitsReady_ = false;
  requestUpdate();
}

// --- Input -------------------------------------------------------------------------------------

void DaybookActivity::back() {
  switch (view_) {
    case View::Month:
    case View::List:
      shelf::leave(renderer, mappedInput);
      return;
    case View::Day:
      view_ = home();
      break;
    case View::Detail:
      confirmDelete_ = false;
      view_ = detailFrom_;
      break;
    case View::Form:
      view_ = (editIndex_ >= 0 && detailIndex_ >= 0 && detailIndex_ < static_cast<int>(events_.size())) ? View::Detail
                                                                                                          : formFrom_;
      break;
    case View::Settings:
      view_ = home();
      break;
    case View::Todo:
      view_ = home();
      break;
    case View::Lists:
      view_ = View::Todo;
      break;
    case View::TaskForm:
      confirmDelete_ = false;
      view_ = taskFrom_;
      break;
    case View::ListForm:
      confirmDelete_ = false;
      view_ = View::Lists;
      break;
    case View::Wallpapers:
      wallChoices_.clear();
      view_ = View::Settings;
      break;
  }
  changed();
}


void DaybookActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    back();
    return;
  }
  if (!hitsReady_.load()) return;

  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  if (view_ == View::Month || view_ == View::List) {
    if (swipe == MappedInputManager::SwipeDir::Left) return route(Act::NextMonth, 0);
    if (swipe == MappedInputManager::SwipeDir::Right) return route(Act::PrevMonth, 0);
  }
  if (view_ == View::Day || view_ == View::List || view_ == View::Todo) {
    if (swipe == MappedInputManager::SwipeDir::Up) return route(Act::PageNext, 0);
    if (swipe == MappedInputManager::SwipeDir::Down) return route(Act::PagePrev, 0);
  }
  if (view_ == View::Todo && lists_.size() > 1) {  // swipe sideways to change list
    const int n = static_cast<int>(lists_.size());
    const int at = static_cast<int>(curList_);
    if (swipe == MappedInputManager::SwipeDir::Left) return route(Act::SelectList, (at + 1) % n);
    if (swipe == MappedInputManager::SwipeDir::Right) return route(Act::SelectList, (at + n - 1) % n);
  }

  // Buttons act when the finger goes DOWN, not when it lifts: that is the whole difference between a tap that
  // feels instant and one that feels late. On the screens that swipe (month, list, day, to-do) the rows and day
  // cells still wait for the lift, because a swipe starts on top of them; the buttons there act on the press.
  const bool swipeScreen = view_ == View::Month || view_ == View::List || view_ == View::Day || view_ == View::Todo;
  int downX = 0;
  int downY = 0;
  if (mappedInput.wasScreenTouchPressed(downX, downY) && !HeaderBackTapTarget::contains(downX, downY)) {
    const Hit* hit = hitAt(downX, downY);
    if (hit && !(swipeScreen && isRowAct(hit->act))) {
      mappedInput.swallowCurrentTouch();  // the lift must not tap what the press just did
      route(hit->act, hit->value);
      return;
    }
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY)) return;
  if (const Hit* hit = hitAt(tapX, tapY)) route(hit->act, hit->value);
}

const DaybookActivity::Hit* DaybookActivity::hitAt(const int x, const int y) const {
  for (auto it = hits_.rbegin(); it != hits_.rend(); ++it) {
    if (x >= it->x && x < it->x + it->w && y >= it->y && y < it->y + it->h) return &*it;
  }
  return nullptr;
}

bool DaybookActivity::isRowAct(const Act act) {
  return act == Act::PickDay || act == Act::OpenEvent || act == Act::EditTask || act == Act::ToggleTask;
}

void DaybookActivity::startDraft(const int index) {
  draftTodo_ = false;
  draftList_ = cal::kMainList;
  if (index >= 0 && index < static_cast<int>(events_.size())) {
    draft_ = events_[static_cast<size_t>(index)];
    editIndex_ = index;
  } else {
    draft_ = cal::Event{};
    draft_.start = draft_.end = selected_;
    draft_.allDay = true;
    draft_.startMin = 9 * 60;
    draft_.endMin = 10 * 60;
    editIndex_ = -1;
  }
}

void DaybookActivity::route(const Act act, const int32_t value) {
  message_.clear();
  if (routeTodo(act, value)) return;
  switch (act) {
    case Act::PrevMonth:
      if (--month_ < 1) {
        month_ = 12;
        --year_;
      }
      page_ = 0;
      break;
    case Act::NextMonth:
      if (++month_ > 12) {
        month_ = 1;
        ++year_;
      }
      page_ = 0;
      break;
    case Act::Today:
      refreshToday();
      selected_ = today_;
      openMonthOf(today_);
      page_ = 0;
      break;
    case Act::ToggleList:
      listMode_ = !listMode_;
      view_ = home();
      page_ = 0;
      saveSettings();
      break;
    case Act::Settings:
      view_ = View::Settings;
      break;
    case Act::PickDay:
      selected_ = value;
      page_ = 0;
      view_ = View::Day;
      break;
    case Act::AddEvent:
      formFrom_ = view_;
      startDraft(-1);
      detailIndex_ = -1;
      view_ = View::Form;
      break;
    case Act::OpenEvent:
      detailFrom_ = view_ == View::List ? View::List : View::Day;
      detailIndex_ = value;
      confirmDelete_ = false;
      view_ = View::Detail;
      break;
    case Act::Edit:
      startDraft(detailIndex_);
      view_ = View::Form;
      break;
    case Act::Delete:
      confirmDelete_ = true;
      break;
    case Act::CancelDelete:
      confirmDelete_ = false;
      break;
    case Act::ConfirmDelete:
      if (detailIndex_ >= 0 && detailIndex_ < static_cast<int>(events_.size())) {
        events_.erase(events_.begin() + detailIndex_);
        if (!saveEvents()) message_ = "Card write failed";
      }
      detailIndex_ = -1;
      confirmDelete_ = false;
      page_ = 0;
      view_ = detailFrom_;
      break;
    case Act::Save: {
      if (draft_.title.empty()) {
        message_ = "Give it a name first";
        break;
      }
      if (draftTodo_ && editIndex_ < 0) {  // ticked "To-Do": it becomes a task due on that day
        if (!saveDraftAsTask()) break;
        detailIndex_ = -1;
        page_ = 0;
        view_ = View::Day;
        break;
      }
      if (draft_.end < draft_.start) draft_.end = draft_.start;
      if (!draft_.allDay && draft_.end == draft_.start && draft_.endMin < draft_.startMin) {
        draft_.endMin = draft_.startMin;
      }
      if (draft_.repeat == cal::Repeat::None) draft_.hasUntil = false;
      if (editIndex_ >= 0 && editIndex_ < static_cast<int>(events_.size())) {
        events_[static_cast<size_t>(editIndex_)] = draft_;
      } else {
        if (events_.size() >= cal::kMaxEvents) {
          message_ = "Calendar is full";
          break;
        }
        draft_.id = cal::nextId(events_);
        events_.push_back(draft_);
        selected_ = draft_.start;
        openMonthOf(selected_);
      }
      if (!saveEvents()) message_ = "Card write failed";
      detailIndex_ = -1;
      page_ = 0;
      view_ = View::Day;
      break;
    }
    case Act::Cancel:
      back();
      return;
    case Act::Form:
      if ((value >> 3) <= FNote) {
        askText(static_cast<Field>(value >> 3));
        return;
      }
      formTap(value >> 3, value & 7);
      break;
    case Act::PagePrev:
      if (page_ > 0) --page_;
      break;
    case Act::PageNext:
      if (page_ + 1 < pages_) ++page_;
      break;
    case Act::SetFirstDay:
      firstDay_ = firstDay_ == 1 ? 0 : (firstDay_ == 0 ? 6 : 1);
      saveSettings();
      break;
    case Act::ToggleWeek:
      weekNums_ = !weekNums_;
      saveSettings();
      break;
    case Act::ToggleAgenda:
      agenda_ = !agenda_;
      saveSettings();
      break;
    case Act::InstallPlugin:
      message_ = daybook::plugin::install();
      if (message_.empty()) message_ = "Installed. Open File Transfer, then Plugins in the web page.";
      break;
    case Act::PickWallpaper:
      wallChoices_ = daybook::wallpaperChoices();
      page_ = 0;
      view_ = View::Wallpapers;
      break;
    case Act::SetWallpaper:  // value 0 is the default, k is wallChoices_[k - 1]
      wallpaper_ = value <= 0 || static_cast<size_t>(value) > wallChoices_.size()
                       ? std::string()
                       : wallChoices_[static_cast<size_t>(value - 1)];
      saveSettings();
      wallChoices_.clear();
      view_ = View::Settings;
      break;
    case Act::OpenTodo:  // the to-do acts are all handled by routeTodo(), above
    case Act::OpenLists:
    case Act::SelectList:
    case Act::NewList:
    case Act::EditList:
    case Act::ListName:
    case Act::NewTask:
    case Act::EditTask:
    case Act::ToggleTask:
    case Act::TaskField:
      break;
    case Act::Back:
      back();
      return;
  }
  changed();
}

void DaybookActivity::askString(const char* title, const std::string& current, const size_t limit,
                                 std::function<void(const std::string&)> apply) {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, title, current, limit);
  if (!keyboard) {
    message_ = "Not enough memory for the keyboard";
    changed();
    return;
  }
  startActivityForResult(std::move(keyboard), [this, apply](const ActivityResult& result) {
    if (!result.isCancelled) {
      const std::string& text = std::get<KeyboardResult>(result.data).text;
      const size_t a = text.find_first_not_of(' ');
      const size_t b = text.find_last_not_of(' ');
      apply(a == std::string::npos ? std::string() : text.substr(a, b - a + 1));
    }
    changed();
  });
}

void DaybookActivity::askText(const Field field) {
  const char* title = field == FTitle ? "EVENT NAME" : (field == FLocation ? "PLACE" : "NOTE");
  const std::string current = field == FTitle ? draft_.title : (field == FLocation ? draft_.location : draft_.note);
  const size_t limit = field == FTitle ? cal::kMaxTitle : (field == FLocation ? cal::kMaxLocation : cal::kMaxNote);
  askString(title, current, limit, [this, field](const std::string& text) {
    if (field == FTitle) draft_.title = cal::capitalizeFirst(text);
    else if (field == FLocation) draft_.location = text;
    else draft_.note = text;
  });
}

bool DaybookActivity::saveDraftAsTask() {
  if (tasks_.size() >= cal::kMaxTasks) {
    message_ = "To-Do list is full";
    return false;
  }
  cal::Task t;
  t.id = cal::nextTaskId(tasks_);
  t.list = lists_[cal::listIndex(lists_, draftList_)].id;
  t.title = draft_.title;
  t.note = draft_.note;
  t.created = today_;
  t.hasDue = true;
  t.due = draft_.start;
  tasks_.push_back(t);
  if (!saveTasks()) message_ = "Card write failed";
  selected_ = draft_.start;
  openMonthOf(selected_);
  draftTodo_ = false;
  return true;
}

// zone: 0 = the whole row; 1..4 = "<<", "<", ">", ">>" (month/hour, day/quarter-hour)
void DaybookActivity::formTap(const int field, const int zone) {
  const int big = zone == 1 ? -1 : (zone == 4 ? 1 : 0);
  const int small = zone == 2 ? -1 : (zone == 3 ? 1 : 0);
  switch (field) {
    case FAllDay:
      draft_.allDay = !draft_.allDay;
      break;
    case FStartDate: {
      const cal::Day span = draft_.end - draft_.start;
      cal::Day next = draft_.start;
      if (big) next = addMonths(draft_.start, big);
      if (small) next = draft_.start + small;
      draft_.start = next;
      draft_.end = next + span;  // moving the start carries the end with it
      break;
    }
    case FEndDate: {
      cal::Day next = draft_.end;
      if (big) next = addMonths(draft_.end, big);
      if (small) next = draft_.end + small;
      draft_.end = std::max(next, draft_.start);
      break;
    }
    case FStartTime: {
      const int delta = big * 60 + small * 15;
      const int span = draft_.endMin - draft_.startMin;
      const int next = std::clamp(draft_.startMin + delta, 0, 1439);
      draft_.startMin = static_cast<int16_t>(next);
      if (draft_.end == draft_.start) draft_.endMin = static_cast<int16_t>(std::clamp(next + std::max(span, 0), 0, 1439));
      break;
    }
    case FEndTime: {
      const int delta = big * 60 + small * 15;
      int next = std::clamp(draft_.endMin + delta, 0, 1439);
      if (draft_.end == draft_.start) next = std::max(next, static_cast<int>(draft_.startMin));
      draft_.endMin = static_cast<int16_t>(next);
      break;
    }
    case FRepeat: {
      const int next = (static_cast<int>(draft_.repeat) + (zone == 2 ? 4 : 1)) % 5;  // "<" steps back
      draft_.repeat = static_cast<cal::Repeat>(next);
      if (draft_.repeat == cal::Repeat::None) draft_.hasUntil = false;
      break;
    }
    case FTodo:
      draftTodo_ = !draftTodo_;
      break;
    case FList: {
      const int n = static_cast<int>(lists_.size());
      const int at = static_cast<int>(cal::listIndex(lists_, draftList_));
      draftList_ = lists_[static_cast<size_t>((at + (zone == 2 ? n - 1 : 1)) % n)].id;
      break;
    }
    case FEvery: {
      const int delta = (big + small) * (big ? 5 : 1);
      draft_.every = static_cast<uint16_t>(std::clamp(static_cast<int>(draft_.every) + delta, 1, 99));
      break;
    }
    case FUntil: {
      if (!draft_.hasUntil) {
        if (big > 0 || small > 0) {
          draft_.hasUntil = true;
          draft_.until = std::max(draft_.end, draft_.start);
          if (big > 0) draft_.until = addMonths(draft_.until, 1);
          else draft_.until += 1;
        }
        break;
      }
      if (big) draft_.until = addMonths(draft_.until, big);
      if (small) draft_.until += small;
      if (draft_.until < draft_.start) draft_.hasUntil = false;  // stepped back past the start: no end
      break;
    }
    default:
      break;
  }
}

// --- Drawing helpers ---------------------------------------------------------------------------

void DaybookActivity::addHit(const int x, const int y, const int w, const int h, const Act act, const int32_t value) {
  hits_.push_back(Hit{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                      static_cast<int16_t>(h), act, value});
}

void DaybookActivity::textBox(const int fontId, const int x, const int y, const int w, const int h,
                               const std::string& text, const bool center, const bool black) const {
  const std::string shown = renderer.truncatedText(fontId, cal::foldForDisplay(text).c_str(), std::max(w - 8, 8));
  const int lh = renderer.getLineHeight(fontId);
  const int tx = center ? x + (w - renderer.getTextWidth(fontId, shown.c_str())) / 2 : x + 4;
  renderer.drawText(fontId, tx, y + (h - lh) / 2, shown.c_str(), black);
}

void DaybookActivity::button(const int x, const int y, const int w, const int h, const char* label, const Act act,
                              const int32_t value, const bool filled, const int fontId) {
  if (filled) {
    renderer.fillRect(x, y, w, h, true);
  } else {
    renderer.drawRoundedRect(x, y, w, h, 2, 8, true);
  }
  textBox(fontId < 0 ? kBold : fontId, x, y, w, h, label, true, !filled);
  addHit(x, y, w, h, act, value);
}

int DaybookActivity::drawHeader(const std::string& title, const std::string& right, const int fontId) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int top = metrics.topPadding + 6;
  const int hh = renderer.getLineHeight(kBold) + 20;
  button(kM, top, 64, hh, "<", Act::Back);
  const int rightW = right.empty() ? 0 : 60;
  textBox(fontId < 0 ? kBold : fontId, kM + 70, top, sw - 2 * kM - 70 - rightW, hh, title, true);
  if (!right.empty()) textBox(kSmall, sw - kM - rightW, top, rightW, hh, right, true);
  renderer.drawLine(kM, top + hh + 4, sw - kM, top + hh + 4);
  return top + hh + 12;
}

int DaybookActivity::bottomBar(const int count, const char* const* labels, const Act* acts) {
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const int bh = renderer.getLineHeight(kBold) + 26;
  const int y = sh - bh - kM;
  const int gap = 8;
  const int bw = (sw - 2 * kM - gap * (count - 1)) / count;
  for (int i = 0; i < count; ++i) button(kM + i * (bw + gap), y, bw, bh, labels[i], acts[i]);
  return y;
}

// Two rows of small buttons: [To-Do's] [List view / Month view], then [Today] [Settings].
int DaybookActivity::homeBar() {
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const int bh = renderer.getLineHeight(kBoldSmall) + 18;
  const int gap = 8;
  const int bw = (sw - 2 * kM - gap) / 2;
  const int y2 = sh - bh - kM;
  const int y1 = y2 - bh - gap;
  button(kM, y1, bw, bh, "To-Do's", Act::OpenTodo, 0, false, kBoldSmall);
  button(kM + bw + gap, y1, bw, bh, listMode_ ? "Month view" : "List view", Act::ToggleList, 0, false, kBoldSmall);
  button(kM, y2, bw, bh, "Today", Act::Today, 0, false, kBoldSmall);
  button(kM + bw + gap, y2, bw, bh, "Settings", Act::Settings, 0, false, kBoldSmall);
  return y1;
}

std::string DaybookActivity::whenLabel(const cal::Event& ev, const cal::Occurrence& o, const cal::Day day) const {
  const bool multi = o.end > o.start;
  const std::string span = multi ? "  " + cal::formatSpan(o.start, o.end) : std::string();
  if (ev.allDay) return "All day" + span;
  if (!multi) {
    std::string s = cal::formatMinutes(ev.startMin, twelveHour());
    if (ev.endMin > ev.startMin) s += " - " + cal::formatMinutes(ev.endMin, twelveHour());
    return s;
  }
  if (day == o.start) return cal::formatMinutes(ev.startMin, twelveHour()) + " ->" + span;
  if (day == o.end) return "-> " + cal::formatMinutes(ev.endMin, twelveHour()) + span;
  return "All day" + span;
}

std::string DaybookActivity::repeatLabel(const cal::Event& ev) const {
  if (ev.repeat == cal::Repeat::None) return std::string();  // nothing to say for a one-off
  std::string s = ev.every <= 1 ? std::string("Every ") + unitName(ev.repeat, false)
                                : "Every " + std::to_string(ev.every) + " " + unitName(ev.repeat, true);
  if (ev.hasUntil) s += ", until " + cal::formatDate(ev.until);
  return s;
}

// --- Screens -----------------------------------------------------------------------------------

int DaybookActivity::drawMonthNav() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int top = metrics.topPadding + 6;
  const int hh = renderer.getLineHeight(kBold) + 20;
  button(kM, top, 64, hh, "<", Act::PrevMonth);
  button(sw - kM - 64, top, 64, hh, ">", Act::NextMonth);
  textBox(kBold, kM + 70, top, sw - 2 * kM - 140, hh, std::string(kMonthName[month_ - 1]) + " " + std::to_string(year_),
          true);
  return top + hh + 8;
}

void DaybookActivity::drawMonth() {
  const int sw = renderer.getScreenWidth();
  int y = drawMonthNav();
  const int barY = homeBar();

  const int wkW = weekNums_ ? 34 : 0;
  const int cw = (sw - 2 * kM - wkW) / 7;
  const int gridX = kM + wkW;
  const int lhS = renderer.getLineHeight(kSmall);
  for (int i = 0; i < 7; ++i) {
    std::string name = kDow[(firstDay_ + i) % 7];
    if (renderer.getTextWidth(kSmall, name.c_str()) > cw - 2) name.resize(2);  // never cut a letter off
    const int tw = renderer.getTextWidth(kSmall, name.c_str());
    renderer.drawText(kSmall, gridX + i * cw + (cw - tw) / 2, y, name.c_str(), true);
  }
  y += lhS + 6;
  // Short rows; what is left under the grid shows the selected day.
  const int stripMin = renderer.getLineHeight(kBoldSmall) + 8 + 2 * (lhS + 8);
  const int ch = std::clamp((barY - 8 - y - stripMin) / 6, 44, 76);

  const cal::Day first = cal::daysFromCivil(year_, month_, 1);
  const int lead = (cal::weekday(first) - firstDay_ + 7) % 7;
  const cal::Day gridStart = first - lead;
  const int weeks = (lead + cal::daysInMonth(year_, month_) + 6) / 7;
  const uint64_t busy = cal::busyDays(events_, gridStart);
  uint64_t taskAny = 0;
  uint64_t taskOpen = 0;
  cal::taskBusy(tasks_, gridStart, taskAny, taskOpen);

  for (int w = 0; w < weeks; ++w) {
    const int ry = y + w * ch;
    if (weekNums_) {
      textBox(kSmall, kM, ry, wkW, ch / 2, std::to_string(cal::isoWeek(gridStart + w * 7 + 3)), true);
    }
    for (int i = 0; i < 7; ++i) {
      const cal::Day day = gridStart + w * 7 + i;
      const int bit = static_cast<int>(day - gridStart);
      const cal::Date d = cal::civilFromDays(day);
      const int cx = gridX + i * cw;
      const bool inMonth = d.m == month_;
      const bool isToday = clockKnown_ && day == today_;
      renderer.drawRect(cx, ry, cw, ch, true);
      if (day == selected_) renderer.drawRect(cx + 2, ry + 2, cw - 4, ch - 4, true);
      const std::string num = std::to_string(d.d);
      if (isToday) {
        renderer.fillRect(cx + 4, ry + 4, cw - 8, renderer.getLineHeight(kBoldSmall) + 4, true);
        textBox(kBoldSmall, cx + 4, ry + 4, cw - 8, renderer.getLineHeight(kBoldSmall) + 4, num, true, false);
      } else {
        textBox(inMonth ? kBoldSmall : kSmall, cx + 4, ry + 4, cw - 8, renderer.getLineHeight(kSmall) + 4, num, true);
      }
      if ((busy >> bit) & 1) renderer.fillRect(cx + 8, ry + ch - 10, cw - 16, 5, true);
      if ((taskAny >> bit) & 1) {  // a to-do is due: solid square while one is open, hollow when all are done
        const int ms = 10;
        if ((taskOpen >> bit) & 1) renderer.fillRect(cx + cw / 2 - ms / 2, ry + ch - 26, ms, ms, true);
        else renderer.drawRect(cx + cw / 2 - ms / 2, ry + ch - 26, ms, ms, true);
      }
      addHit(cx, ry, cw, ch, Act::PickDay, day);
    }
  }
  drawDayStrip(y + weeks * ch + 8, barY - 8);
}

// The selected day under the month grid: one line per event or task, tap to open the day.
void DaybookActivity::drawDayStrip(const int y0, const int y1) {
  const int sw = renderer.getScreenWidth();
  const int hdrH = renderer.getLineHeight(kBoldSmall) + 8;
  const int rowH = renderer.getLineHeight(kSmall) + 8;
  if (y1 - y0 < hdrH) return;
  std::vector<cal::Occurrence> occ;
  cal::occurrencesOn(events_, selected_, occ);
  std::vector<size_t> due;
  cal::tasksDueOn(tasks_, selected_, due);
  renderer.drawLine(kM, y0, sw - kM, y0);
  std::string head = dayTitle(selected_, false);
  if (occ.empty() && due.empty()) head += "  -  nothing planned";
  textBox(kBoldSmall, kM, y0 + 2, sw - 2 * kM, hdrH, head, false);
  addHit(kM, y0, sw - 2 * kM, y1 - y0, Act::PickDay, selected_);

  int y = y0 + 2 + hdrH;
  const int fit = std::max(0, (y1 - y) / rowH);
  const int total = static_cast<int>(occ.size() + due.size());
  const int shown = total > fit ? std::max(0, fit - 1) : total;  // the last slot may say "+N more"
  const int lw = renderer.getTextWidth(kSmall, twelveHour() ? "12:00 PM-12:00 PM" : "00:00-00:00") + 12;
  for (int i = 0; i < shown; ++i) {
    if (static_cast<size_t>(i) < occ.size()) {
      const cal::Occurrence& o = occ[static_cast<size_t>(i)];
      const cal::Event& ev = events_[o.event];
      textBox(kSmall, kM, y, lw, rowH, shortWhen(ev, o, twelveHour()), false);
      textBox(kBoldSmall, kM + lw, y, sw - 2 * kM - lw, rowH, ev.title, false);
    } else {
      const cal::Task& t = tasks_[due[static_cast<size_t>(i) - occ.size()]];
      drawCheck(kM + 4, y + 5, rowH - 10, t.done, true);
      textBox(kBoldSmall, kM + lw, y, sw - 2 * kM - lw, rowH, t.title, false);
    }
    y += rowH;
  }
  if (fit > 0 && shown < total) {
    textBox(kSmall, kM, y, sw - 2 * kM, rowH, "+ " + std::to_string(total - shown) + " more", false);
  }
}

// Everything from today on: late tasks first (inverted), then events and due tasks day by day, a heavy
// rule and the month's name where a new month starts. Paged.
void DaybookActivity::drawList() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int top = metrics.topPadding + 6;
  const int hh = renderer.getLineHeight(kBold) + 20;
  textBox(kBold, kM, top, sw - 2 * kM, hh, "Upcoming", true);
  renderer.drawLine(kM, top + hh + 4, sw - kM, top + hh + 4);
  const int y0 = top + hh + 12;
  const int barY = homeBar();

  // Events, a month at a time so a long list is gathered in date order and cut at a fixed size.
  constexpr size_t kListCap = 400;
  const cal::Day horizon = today_ + 366;
  std::vector<cal::Occurrence> occ;
  for (cal::Day from = today_; from <= horizon && occ.size() < kListCap; from += 31) {
    std::vector<cal::Occurrence> part;
    cal::occurrencesIn(events_, from, std::min<cal::Day>(from + 30, horizon), part);
    std::sort(part.begin(), part.end(),
              [](const cal::Occurrence& a, const cal::Occurrence& b) { return a.start < b.start; });
    for (const cal::Occurrence& o : part) {
      if (from > today_ && o.start < from) continue;  // an earlier window already has it
      if (occ.size() < kListCap) occ.push_back(o);
    }
  }

  struct Entry {
    int group;     // 0 late tasks, 1 everything else
    cal::Day day;  // the day it is listed under
    int kind;      // 0 late task, 1 event, 2 task due
    size_t idx;    // into occ (events) or tasks_ (tasks)
  };
  std::vector<Entry> entries;
  std::vector<size_t> late;
  cal::overdueTasks(tasks_, today_, late);
  for (const size_t t : late) entries.push_back(Entry{0, tasks_[t].due, 0, t});
  for (size_t i = 0; i < occ.size(); ++i) entries.push_back(Entry{1, std::max(occ[i].start, today_), 1, i});
  for (size_t t = 0; t < tasks_.size(); ++t) {
    const cal::Task& k = tasks_[t];
    if (!k.done && k.hasDue && k.due >= today_ && k.due <= horizon) entries.push_back(Entry{1, k.due, 2, t});
  }
  std::sort(entries.begin(), entries.end(), [&](const Entry& a, const Entry& b) {
    if (a.group != b.group) return a.group < b.group;
    if (a.day != b.day) return a.day < b.day;
    if (a.kind != b.kind) return a.kind < b.kind;  // events before tasks
    if (a.kind == 1) {
      const cal::Event& ea = events_[occ[a.idx].event];
      const cal::Event& eb = events_[occ[b.idx].event];
      if (ea.allDay != eb.allDay) return ea.allDay;
      if (!ea.allDay && ea.startMin != eb.startMin) return ea.startMin < eb.startMin;
      if (ea.title != eb.title) return ea.title < eb.title;
    }
    return a.idx < b.idx;
  });

  enum RowType { kLateHead, kMonthHead, kDayHead, kItem };
  struct Row {
    int type;
    cal::Day day;
    size_t entry;
  };
  std::vector<Row> rows;
  std::vector<size_t> pageStart;
  const int hdrH = renderer.getLineHeight(kBoldSmall) + 12;
  const int monthH = renderer.getLineHeight(kBoldSmall) + 18;
  const int evH = renderer.getLineHeight(kBold) + 16;
  const int pagerH = renderer.getLineHeight(kBoldSmall) + 18;
  const auto monthKey = [](const cal::Day d) {
    const cal::Date c = cal::civilFromDays(d);
    return c.y * 12 + c.m;
  };
  const auto paginate = [&](const int avail) {
    rows.clear();
    pageStart.clear();
    pageStart.push_back(0);
    int used = 0;
    int lastGroup = -1;
    int lastMonth = -1;
    cal::Day lastDay = INT32_MIN;
    for (size_t i = 0; i < entries.size(); ++i) {
      const Entry& e = entries[i];
      bool lateHead = e.group == 0 && lastGroup != 0;
      bool monthHead = e.group == 1 && monthKey(e.day) != lastMonth;
      bool dayHead = e.group == 1 && (monthHead || e.day != lastDay);
      int need = (lateHead ? hdrH : 0) + (monthHead ? monthH : 0) + (dayHead ? hdrH : 0) + evH;
      if (used > 0 && used + need > avail) {  // a new page repeats the heading it continues under
        pageStart.push_back(rows.size());
        used = 0;
        lateHead = e.group == 0;
        dayHead = e.group == 1;
        need = (lateHead ? hdrH : 0) + (monthHead ? monthH : 0) + (dayHead ? hdrH : 0) + evH;
      }
      if (lateHead) rows.push_back(Row{kLateHead, e.day, i});
      if (monthHead) rows.push_back(Row{kMonthHead, e.day, i});
      if (dayHead) rows.push_back(Row{kDayHead, e.day, i});
      rows.push_back(Row{kItem, e.day, i});
      used += need;
      lastGroup = e.group;
      lastDay = e.day;
      if (e.group == 1) lastMonth = monthKey(e.day);
    }
  };
  int avail = barY - 8 - y0;
  paginate(avail);
  if (pageStart.size() > 1) {  // room for the Prev / Next row
    avail -= pagerH + 6;
    paginate(avail);
  }
  pages_ = static_cast<int>(pageStart.size());
  page_ = std::clamp(page_, 0, pages_ - 1);

  if (rows.empty()) textBox(kBody, 0, y0 + 20, sw, evH, "Nothing coming up", true);

  const size_t from = pageStart[static_cast<size_t>(page_)];
  const size_t to = page_ + 1 < pages_ ? pageStart[static_cast<size_t>(page_ + 1)] : rows.size();
  const int lw = renderer.getTextWidth(kSmall, twelveHour() ? "12:00 PM-12:00 PM" : "00:00-00:00") + 12;
  int y = y0;
  for (size_t r = from; r < to; ++r) {
    const Row& row = rows[r];
    if (row.type == kLateHead) {
      textBox(kBoldSmall, kM, y, sw - 2 * kM, hdrH, "Overdue", false);
      renderer.drawLine(kM, y + hdrH - 3, sw - kM, y + hdrH - 3);
      y += hdrH;
    } else if (row.type == kMonthHead) {
      const cal::Date d = cal::civilFromDays(row.day);
      renderer.drawLine(kM, y + 4, sw - kM, y + 4, 3, true);
      textBox(kBoldSmall, kM, y + 6, sw - 2 * kM, monthH - 6, std::string(kMonthName[d.m - 1]) + " " + std::to_string(d.y),
              false);
      y += monthH;
    } else if (row.type == kDayHead) {
      std::string head = dayTitle(row.day, false);
      if (clockKnown_ && row.day == today_) head += "  -  today";
      textBox(kBoldSmall, kM, y, sw - 2 * kM, hdrH, head, false);
      renderer.drawLine(kM, y + hdrH - 3, sw - kM, y + hdrH - 3);
      y += hdrH;
    } else {
      const Entry& e = entries[row.entry];
      if (e.kind == 0) {  // a late task, inverted
        taskRow(y, evH - 4, e.idx, true, false, "",
                "due " + shortDate(tasks_[e.idx].due) + " - " + listName(tasks_[e.idx].list));
      } else if (e.kind == 2) {
        taskRow(y, evH - 4, e.idx, false, false, "", listName(tasks_[e.idx].list));
      } else {
        const cal::Occurrence& o = occ[e.idx];
        const cal::Event& ev = events_[o.event];
        renderer.drawRect(kM, y, sw - 2 * kM, evH - 4, true);
        textBox(kSmall, kM + 4, y, lw, evH - 4, shortWhen(ev, o, twelveHour()), false);
        textBox(kBold, kM + 4 + lw, y, sw - 2 * kM - 4 - lw, evH - 4, ev.title, false);
        addHit(kM, y, sw - 2 * kM, evH - 4, Act::OpenEvent, static_cast<int32_t>(o.event));
      }
      y += evH;
    }
  }

  if (pages_ > 1) {
    const int py = barY - 6 - pagerH;
    const int pw = 120;
    button(kM, py, pw, pagerH, "Prev", Act::PagePrev, 0, false, kBoldSmall);
    button(sw - kM - pw, py, pw, pagerH, "Next", Act::PageNext, 0, false, kBoldSmall);
    textBox(kBoldSmall, kM + pw, py, sw - 2 * kM - 2 * pw, pagerH,
            std::to_string(page_ + 1) + "/" + std::to_string(pages_), true);
  }
}

void DaybookActivity::drawDay() {
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  int y = drawHeader(dayTitle(selected_), weekNums_ ? "W" + std::to_string(cal::isoWeek(selected_)) : "", kBoldSmall);

  std::vector<cal::Occurrence> occ;
  cal::occurrencesOn(events_, selected_, occ);
  std::vector<size_t> due;
  cal::tasksDueOn(tasks_, selected_, due);
  const size_t total = occ.size() + due.size();

  const int rowH = renderer.getLineHeight(kSmall) + renderer.getLineHeight(kBold) + 22;
  const int bh = renderer.getLineHeight(kBold) + 26;
  const int listBottom = sh - bh - kM - 8;
  const int perPage = std::max(1, (listBottom - y) / rowH);
  pages_ = std::max(1, (static_cast<int>(total) + perPage - 1) / perPage);
  page_ = std::clamp(page_, 0, pages_ - 1);

  if (total == 0) {
    textBox(kBody, 0, y + 20, sw, rowH, "Nothing planned", true);
  }
  for (int i = 0; i < perPage; ++i) {
    const size_t idx = static_cast<size_t>(page_ * perPage + i);
    if (idx >= total) break;
    const int ry = y + i * rowH;
    if (idx >= occ.size()) {  // a to-do due this day
      const cal::Task& t = tasks_[due[idx - occ.size()]];
      std::string small = listName(t.list);
      if (t.done) small += "   done " + shortDate(t.doneOn);
      taskRow(ry, rowH - 6, due[idx - occ.size()], false, true, small);
      continue;
    }
    const cal::Event& ev = events_[occ[idx].event];
    renderer.drawRect(kM, ry, sw - 2 * kM, rowH - 6, true);
    std::string line1 = whenLabel(ev, occ[idx], selected_);
    if (!ev.location.empty()) line1 += "   @ " + ev.location;
    textBox(kSmall, kM, ry + 4, sw - 2 * kM, renderer.getLineHeight(kSmall) + 4, line1, false);
    textBox(kBold, kM, ry + 6 + renderer.getLineHeight(kSmall), sw - 2 * kM, renderer.getLineHeight(kBold) + 8,
            ev.title, false);
    addHit(kM, ry, sw - 2 * kM, rowH - 6, Act::OpenEvent, static_cast<int32_t>(occ[idx].event));
  }

  if (pages_ > 1) {
    const char* labels[3] = {"Prev", "+ Event", "Next"};
    const Act acts[3] = {Act::PagePrev, Act::AddEvent, Act::PageNext};
    bottomBar(3, labels, acts);
  } else {
    const char* labels[1] = {"+ Event"};
    const Act acts[1] = {Act::AddEvent};
    bottomBar(1, labels, acts);
  }
}

void DaybookActivity::drawDetail() {
  const int sw = renderer.getScreenWidth();
  if (detailIndex_ < 0 || detailIndex_ >= static_cast<int>(events_.size())) {
    view_ = View::Day;
    drawDay();
    return;
  }
  const cal::Event& ev = events_[static_cast<size_t>(detailIndex_)];
  int y = drawHeader(dayTitle(ev.start), "", kBoldSmall);  // the date heads the screen
  const int lhB = renderer.getLineHeight(kBold);
  const int lh = renderer.getLineHeight(kBody);
  const std::string title = cal::foldForDisplay(ev.title);
  for (const std::string& line : renderer.wrappedText(kBold, title.c_str(), sw - 2 * kM, 3)) {
    renderer.drawText(kBold, kM, y, line.c_str(), true);
    y += lhB + 2;
  }
  y += 10;
  const auto put = [&](const std::string& s) {
    const std::string shown = cal::foldForDisplay(s);
    for (const std::string& line : renderer.wrappedText(kBody, shown.c_str(), sw - 2 * kM, 6)) {
      renderer.drawText(kBody, kM, y, line.c_str(), true);
      y += lh + 2;
    }
    y += 8;
  };
  if (ev.end > ev.start) put("Until " + dayTitle(ev.end));
  if (ev.allDay) {
    put("All day");
  } else {
    put(cal::formatMinutes(ev.startMin, twelveHour()) + " - " + cal::formatMinutes(ev.endMin, twelveHour()));
  }
  const std::string rep = repeatLabel(ev);
  if (!rep.empty()) put(rep);
  if (!ev.location.empty()) put("Place: " + ev.location);
  if (!ev.note.empty()) put(ev.note);

  if (confirmDelete_) {
    const char* labels[2] = {"Keep it", "Delete"};
    const Act acts[2] = {Act::CancelDelete, Act::ConfirmDelete};
    const int barY = bottomBar(2, labels, acts);
    const std::string question =
        ev.repeat == cal::Repeat::None ? "Delete this event?" : "Delete event and all repeats?";
    const int lhQ = renderer.getLineHeight(kBoldSmall) + 2;
    const auto lines = renderer.wrappedText(kBoldSmall, question.c_str(), sw - 2 * kM, 2);
    int qy = barY - 12 - static_cast<int>(lines.size()) * lhQ;
    for (const std::string& line : lines) {
      renderer.drawText(kBoldSmall, (sw - renderer.getTextWidth(kBoldSmall, line.c_str())) / 2, qy, line.c_str(), true);
      qy += lhQ;
    }
  } else {
    const char* labels[2] = {"Edit", "Delete"};
    const Act acts[2] = {Act::Edit, Act::Delete};
    bottomBar(2, labels, acts);
  }
}

// --- Form rows. The value column starts at rx; the arrows sit in fixed columns so rows line up. -------

void DaybookActivity::rowBox(const int rx, const int ry, const int rw, const int h, const Act act, const int base,
                              const std::string& value) {
  renderer.drawRoundedRect(rx, ry, rw, h, 2, 8, true);
  textBox(kBody, rx, ry, rw, h, value, false);
  addHit(rx, ry, rw, h, act, base);
}

void DaybookActivity::rowCycler(const int rx, const int ry, const int rw, const int h, const Act act, const int base,
                                 const std::string& value) {
  const int bw = 46;
  addHit(rx, ry, rw, h, act, base);  // the row itself: next value. The arrows, added after, win.
  button(rx + bw + 4, ry, bw, h, "<", act, base | 2);
  button(rx + rw - 2 * bw - 4, ry, bw, h, ">", act, base | 3);
  textBox(kBody, rx + 2 * bw + 8, ry, rw - 4 * bw - 16, h, value, true);
}

void DaybookActivity::rowStepper(const int rx, const int ry, const int rw, const int h, const Act act,
                                  const int base, const std::string& value, const std::string& sub,
                                  const bool centerHit) {
  const int bw = 46;
  if (centerHit) addHit(rx + 2 * bw + 8, ry, rw - 4 * bw - 16, h, act, base);
  button(rx, ry, bw, h, "<<", act, base | 1);
  button(rx + bw + 4, ry, bw, h, "<", act, base | 2);
  button(rx + rw - 2 * bw - 4, ry, bw, h, ">", act, base | 3);
  button(rx + rw - bw, ry, bw, h, ">>", act, base | 4);
  const int vx = rx + 2 * bw + 8;
  const int vw = rw - 4 * bw - 16;
  const int lhS = renderer.getLineHeight(kSmall);
  if (!sub.empty() && h >= renderer.getLineHeight(kBody) + lhS + 4) {  // the weekday, small, under the date
    const int top = (h - renderer.getLineHeight(kBody) - lhS) / 2;
    textBox(kBody, vx, ry + top - 2, vw, renderer.getLineHeight(kBody) + 2, value, true);
    textBox(kSmall, vx, ry + top + renderer.getLineHeight(kBody), vw, lhS + 2, sub, true);
  } else {
    textBox(kBody, vx, ry, vw, h, value, true);
  }
}

void DaybookActivity::drawForm() {
  const int sw = renderer.getScreenWidth();
  const bool asTask = draftTodo_ && editIndex_ < 0;
  int y = drawHeader(asTask ? "New to-do" : (editIndex_ >= 0 ? "Edit event" : "New event"), "");

  std::vector<Field> rows;
  if (asTask) {  // a to-do needs a name, a note, a due day and a list: nothing else
    rows = {FTitle, FNote, FTodo, FStartDate, FList};
  } else {
    rows = {FTitle, FLocation, FNote};
    if (editIndex_ < 0) rows.push_back(FTodo);
    rows.push_back(FAllDay);
    rows.push_back(FStartDate);
    rows.push_back(FEndDate);
    if (!draft_.allDay) {
      rows.push_back(FStartTime);
      rows.push_back(FEndTime);
    }
    rows.push_back(FRepeat);
    if (draft_.repeat != cal::Repeat::None) {
      rows.push_back(FEvery);
      rows.push_back(FUntil);
    }
  }

  const char* labels[2] = {"Cancel", "Save"};
  const Act acts[2] = {Act::Cancel, Act::Save};
  const int barY = bottomBar(2, labels, acts);
  const int msgH = renderer.getLineHeight(kSmall) + 6;
  const int avail = barY - 6 - y - msgH;
  const int rowH = std::min(64, avail / static_cast<int>(rows.size()));
  static constexpr const char* kLabel[FCount] = {"Name",  "Place", "Note",   "All day", "From",  "To", "Start",
                                                 "End",   "Repeat", "Every", "Until",   "To-Do", "List"};
  const int labelW = 84;
  const int rx = kM + labelW;
  const int rw = sw - kM - rx;

  for (size_t r = 0; r < rows.size(); ++r) {
    const Field f = rows[r];
    const int ry = y + static_cast<int>(r) * rowH;
    const int h = rowH - 6;
    textBox(kSmall, kM, ry, labelW, h, asTask && f == FStartDate ? "Due" : kLabel[f], false);
    const int base = static_cast<int>(f) << 3;
    std::string value;
    std::string sub;  // the weekday under a date
    switch (f) {
      case FTitle:
        value = draft_.title.empty() ? "tap to type" : draft_.title;
        break;
      case FLocation:
        value = draft_.location.empty() ? "-" : draft_.location;
        break;
      case FNote:
        value = draft_.note.empty() ? "-" : draft_.note;
        break;
      case FAllDay:
        value = draft_.allDay ? "Yes" : "No";
        break;
      case FStartDate:
        value = cal::formatDate(draft_.start);
        sub = kDowLong[cal::weekday(draft_.start)];
        break;
      case FEndDate:
        value = cal::formatDate(draft_.end);
        sub = kDowLong[cal::weekday(draft_.end)];
        break;
      case FStartTime:
        value = cal::formatMinutes(draft_.startMin, twelveHour());
        break;
      case FEndTime:
        value = cal::formatMinutes(draft_.endMin, twelveHour());
        break;
      case FRepeat:
        value = kRepeatName[static_cast<int>(draft_.repeat)];
        break;
      case FEvery:
        value = std::to_string(draft_.every) + " " + unitName(draft_.repeat, draft_.every != 1);
        break;
      case FUntil:
        value = draft_.hasUntil ? cal::formatDate(draft_.until) : "Never";
        if (draft_.hasUntil) sub = kDowLong[cal::weekday(draft_.until)];
        break;
      case FTodo:
        value = draftTodo_ ? "Yes" : "No";
        break;
      case FList:
        value = listName(draftList_);
        break;
      default:
        break;
    }
    switch (f) {
      case FTitle:
      case FLocation:
      case FNote:
        rowBox(rx, ry, rw, h, Act::Form, base, value);
        break;
      case FAllDay:
      case FRepeat:
      case FList:
        rowCycler(rx, ry, rw, h, Act::Form, base, value);
        break;
      case FTodo: {
        rowCycler(rx, ry, rw, h, Act::Form, base, value);
        const int bs = std::min(28, h - 8);  // the tick box beside the word
        const int tw = renderer.getTextWidth(kBody, value.c_str());
        drawCheck(rx + rw / 2 - tw / 2 - bs - 12, ry + (h - bs) / 2, bs, draftTodo_, true);
        break;
      }
      default:  // dates, times, every, until
        rowStepper(rx, ry, rw, h, Act::Form, base, value, sub, false);
        break;
    }
  }
  if (!message_.empty()) textBox(kBoldSmall, 0, barY - msgH - 2, sw, msgH, message_, true);
}

void DaybookActivity::drawSettings() {
  const int sw = renderer.getScreenWidth();
  int y = drawHeader("Settings", "");
  // Two lines a row, the setting's name small over its value, so neither is cut short by the other.
  const int lhS = renderer.getLineHeight(kSmall);
  const int lhB = renderer.getLineHeight(kBold);
  const int rowH = lhS + lhB + 18;
  const auto row = [&](const char* label, const std::string& value, const Act act) {
    renderer.drawRoundedRect(kM, y, sw - 2 * kM, rowH, 2, 8, true);
    textBox(kSmall, kM + 6, y + 4, sw - 2 * kM - 12, lhS + 2, label, false);
    textBox(kBold, kM + 6, y + 6 + lhS, sw - 2 * kM - 12, lhB + 6, value, false);
    addHit(kM, y, sw - 2 * kM, rowH, act);
    y += rowH + 8;
  };
  row("First day of week", firstDay_ == 1 ? "Monday" : (firstDay_ == 0 ? "Sunday" : "Saturday"), Act::SetFirstDay);
  row("Week numbers", weekNums_ ? "On" : "Off", Act::ToggleWeek);
  row("Agenda on the sleep screen", agenda_ ? "On" : "Off", Act::ToggleAgenda);
  {
    // The name after the last '/', so "/sleepscreen/forest.bmp" reads "forest.bmp".
    const size_t slash = wallpaper_.rfind('/');
    row("Sleep wallpaper", wallpaper_.empty() ? std::string("Default (/sleep.bmp)") : wallpaper_.substr(slash + 1),
        Act::PickWallpaper);
  }
  {
    const daybook::plugin::State ps = daybook::plugin::state();
    row("Web page for phone or PC (plugin)", ps == daybook::plugin::State::Missing ? "Install" : (ps == daybook::plugin::State::Outdated ? "Update" : "Installed"),
        Act::InstallPlugin);
  }
  y += 6;
  std::string note = "The agenda shows when Sleep screen is set to Custom. Clock format follows System settings.";
  if (!message_.empty()) {
    note = message_;
  } else if (daybook::plugin::state() != daybook::plugin::State::Missing && !daybook::plugin::pluginsEnabled()) {
    note = "The web page needs Settings > Connectivity > Plugins turned on.";
  }
  for (const std::string& line : renderer.wrappedText(kSmall, note.c_str(), sw - 2 * kM, 4)) {
    renderer.drawText(kSmall, kM, y, line.c_str(), true);
    y += renderer.getLineHeight(kSmall) + 2;
  }
  if (!clockKnown_) {
    y += 8;
    textBox(kBoldSmall, 0, y, sw, renderer.getLineHeight(kBoldSmall) + 8, "Clock not set: no 'today' shown", true);
  }
}

// The picker behind "Sleep wallpaper": Default, then every .bmp the card holds, a page at a time.
void DaybookActivity::drawWallpapers() {
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  int y = drawHeader("Sleep wallpaper", "");
  const int bh = renderer.getLineHeight(kBold) + 26;
  const int rowH = renderer.getLineHeight(kBold) + 22;
  const int bottom = sh - bh - kM - 8;
  const int total = static_cast<int>(wallChoices_.size()) + 1;  // + the default
  const int perPage = std::max(1, (bottom - y) / (rowH + 6));
  pages_ = std::max(1, (total + perPage - 1) / perPage);
  page_ = std::clamp(page_, 0, pages_ - 1);
  for (int i = 0; i < perPage; ++i) {
    const int idx = page_ * perPage + i;
    if (idx >= total) break;
    const int ry = y + i * (rowH + 6);
    const std::string path = idx == 0 ? std::string() : wallChoices_[static_cast<size_t>(idx - 1)];
    const bool chosen = path == wallpaper_;
    renderer.drawRoundedRect(kM, ry, sw - 2 * kM, rowH, 2, 8, true);
    if (chosen) renderer.drawRoundedRect(kM + 3, ry + 3, sw - 2 * kM - 6, rowH - 6, 1, 6, true);
    // "/sleepscreen/forest.bmp" shown as it is on the card, without the leading slash.
    const std::string shown = idx == 0 ? std::string("Default (/sleep.bmp)") : path.substr(path[0] == '/' ? 1 : 0);
    textBox(kBold, kM + 8, ry, sw - 2 * kM - 16, rowH, shown, false);
    addHit(kM, ry, sw - 2 * kM, rowH, Act::SetWallpaper, idx);
  }
  if (wallChoices_.empty()) {
    textBox(kSmall, 0, bottom - renderer.getLineHeight(kSmall) - 4, sw, renderer.getLineHeight(kSmall) + 4,
            "No .bmp files found on the card", true);
  }
  if (pages_ > 1) {
    const int pw = 120;
    const int py = sh - bh - kM;
    button(kM, py, pw, bh, "Prev", Act::PagePrev, 0);
    button(sw - kM - pw, py, pw, bh, "Next", Act::PageNext, 0);
    textBox(kBoldSmall, kM + pw, py, sw - 2 * kM - 2 * pw, bh, std::to_string(page_ + 1) + "/" + std::to_string(pages_),
            true);
  }
}

void DaybookActivity::render(RenderLock&&) {
  // After deep sleep the clock can still be unset when the app reopens; pick it up as soon as it is
  // there, and roll "today" over at midnight.
  const bool wasKnown = clockKnown_;
  refreshToday();
  if (clockKnown_ && !wasKnown) {
    selected_ = today_;
    openMonthOf(today_);
  }
  hits_.clear();
  renderer.clearScreen();
  switch (view_) {
    case View::Month:
      drawMonth();
      break;
    case View::List:
      drawList();
      break;
    case View::Day:
      drawDay();
      break;
    case View::Detail:
      drawDetail();
      break;
    case View::Form:
      drawForm();
      break;
    case View::Settings:
      drawSettings();
      break;
    case View::Todo:
      drawTodo();
      break;
    case View::Lists:
      drawLists();
      break;
    case View::TaskForm:
      drawTaskForm();
      break;
    case View::ListForm:
      drawListForm();
      break;
    case View::Wallpapers:
      drawWallpapers();
      break;
  }
  hitsReady_ = true;
  renderer.displayBuffer();
}
