#pragma once

// Calendar rules and storage format. Freestanding C++17: no Arduino, no
// renderer, no SD card, so host-tests/calendar drives all of it on a PC.
//
// A day is a number: days since 1970-01-01 in the device's LOCAL calendar.
// The activity turns the clock into one (via localtime_r); nothing in here
// knows about timezones, so DST can never move an event.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace daybook {

using Day = int32_t;

struct Date {
  int y = 1970;
  int m = 1;  // 1..12
  int d = 1;  // 1..31
};

Day daysFromCivil(int y, int m, int d);
Date civilFromDays(Day day);
int weekday(Day day);  // 0 = Sunday
int daysInMonth(int y, int m);
int isoWeek(Day day);  // 1..53, ISO 8601
// The same day number `months` later (or earlier); the day clamps to the month end (31st -> 30th/28th).
Day addMonths(Day day, int months);

enum class Repeat : uint8_t { None, Daily, Weekly, Monthly, Yearly };

constexpr size_t kMaxEvents = 400;
constexpr size_t kMaxTitle = 60;
constexpr size_t kMaxLocation = 60;
constexpr size_t kMaxNote = 240;
constexpr size_t kMaxFileBytes = 96 * 1024;

struct Event {
  uint32_t id = 0;
  std::string title;
  std::string location;
  std::string note;
  Day start = 0;  // first day
  Day end = 0;    // last day, inclusive; never before start
  bool allDay = true;
  int16_t startMin = 0;  // minutes after midnight, used when !allDay
  int16_t endMin = 0;    // 0 with !allDay means "no end time"
  Repeat repeat = Repeat::None;
  uint16_t every = 1;  // every N days / weeks / months / years
  bool hasUntil = false;
  Day until = 0;  // last day an occurrence may START on
};

// One showing of an event: [start, end] in days. Repeats shift both.
struct Occurrence {
  size_t event = 0;  // index into the event list
  Day start = 0;
  Day end = 0;
};

// Every occurrence overlapping [from, to], unsorted.
void occurrencesIn(const std::vector<Event>& events, Day from, Day to, std::vector<Occurrence>& out);

// The occurrences touching `day`: all-day first, then by time, then title.
void occurrencesOn(const std::vector<Event>& events, Day day, std::vector<Occurrence>& out);

// A fixed-size answer for a month grid: bit i of the result is set when
// day (first + i) has at least one occurrence. Covers 42 days.
uint64_t busyDays(const std::vector<Event>& events, Day first);

// "09:30" or "9:30 AM".
std::string formatMinutes(int minutes, bool twelveHour);
// "2026-11-25".
std::string formatDate(Day day);
// "11.20-11.24" style short span for a multi-day event.
std::string formatSpan(Day start, Day end);
// Parses "YYYY-MM-DD"; false when it is not a real date.
bool parseDate(const std::string& text, Day& out);

// What the title fonts can draw: Latin-1 is kept; o and u with double acute
// (Hungarian) become o and u with umlaut; anything else past Latin-1 becomes
// '?'. Display only: what is stored keeps the real letters.
std::string foldForDisplay(const std::string& utf8);

// Text-file form, one event per line, tab separated, text escaped.
std::string serialize(const std::vector<Event>& events);
// Reads that form. Bad lines are skipped, never fatal; returns the count kept.
size_t parse(const std::string& text, std::vector<Event>& out);

// The text with its first letter in capitals: a-z, Latin-1 lowercase, and the
// Hungarian double-acute o and u. Anything else is left alone. Applied when a
// title is saved, so a title always starts with a capital.
std::string capitalizeFirst(const std::string& text);

// The next unused id.
uint32_t nextId(const std::vector<Event>& events);

// --- To-dos -------------------------------------------------------------------
//
// A task belongs to one list. It has a created day, an optional due day, and
// a done day once it is ticked. A task with a due day also shows on the
// calendar; an open one whose due day has passed is "overdue".

constexpr size_t kMaxTasks = 500;
constexpr size_t kMaxLists = 8;
constexpr size_t kMaxTaskTitle = 60;
constexpr size_t kMaxTaskNote = 240;
constexpr size_t kMaxListName = 30;
constexpr size_t kMaxTaskFileBytes = 160 * 1024;
constexpr uint32_t kMainList = 1;  // the default list: can be renamed, never removed
constexpr const char* kMainListName = "Main To-Do's";

struct Task {
  uint32_t id = 0;
  uint32_t list = kMainList;
  std::string title;
  std::string note;
  Day created = 0;
  bool hasDue = false;
  Day due = 0;
  bool done = false;
  Day doneOn = 0;
};

struct TaskList {
  uint32_t id = 0;
  std::string name;
};

bool isOverdue(const Task& task, Day today);  // open, has a due day, due day before today

std::string serializeTasks(const std::vector<Task>& tasks);
size_t parseTasks(const std::string& text, std::vector<Task>& out);
std::string serializeLists(const std::vector<TaskList>& lists);
size_t parseLists(const std::string& text, std::vector<TaskList>& out);

// Makes the pair safe to use: the default list exists (first, named
// kMainListName if the file had none), names are not empty, and a task whose
// list is gone moves to the default list.
void repairLists(std::vector<TaskList>& lists, std::vector<Task>& tasks);

uint32_t nextTaskId(const std::vector<Task>& tasks);
uint32_t nextListId(const std::vector<TaskList>& lists);
// Index of the list with this id, or 0 when there is none.
size_t listIndex(const std::vector<TaskList>& lists, uint32_t id);

// Indexes into `tasks`. One list in screen order: open ones (due soonest first,
// undated last), then done ones (latest first).
void tasksOfList(const std::vector<Task>& tasks, uint32_t list, std::vector<size_t>& out);
// Every task due on `day`, open ones first.
void tasksDueOn(const std::vector<Task>& tasks, Day day, std::vector<size_t>& out);
// Open tasks past their due day, oldest due first.
void overdueTasks(const std::vector<Task>& tasks, Day today, std::vector<size_t>& out);
// Like busyDays for tasks: bit i of `any` is set when a task is due on
// (first + i); bit i of `open` when one of them is not done. 42 days.
void taskBusy(const std::vector<Task>& tasks, Day first, uint64_t& any, uint64_t& open);

}  // namespace daybook
