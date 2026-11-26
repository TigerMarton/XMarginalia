// Generates / checks Daybook files with the device's own core, for the browser plugin's parity test.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <random>
#include <iostream>
#include "DaybookCore.h"
using namespace daybook;
static std::string slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }
static void spit(const std::string& p, const std::string& t) { std::ofstream f(p, std::ios::binary); f << t; }
static std::string rtext(std::mt19937& g, int max) {
  static const char* bits[] = {"Dentist", " ", "meeting", "\t", "\n", "\\", "\xC5\x91r", "\xC3\xA9", "x", "Bday", "a\\tb", "50%", "\xE2\x82\xAC"};
  std::string s; int n = g() % 6;
  for (int i = 0; i < n; i++) s += bits[g() % 13];
  if (s.size() > (size_t)max) s.resize(max);
  return s;
}
int main(int, char** argv) {
  std::string mode = argv[1], dir = argv[2];
  if (mode == "gen") {
    std::mt19937 g(atoi(argv[3]));
    std::vector<Event> ev; std::vector<Task> ts; std::vector<TaskList> ls;
    for (int i = 0; i < 120; i++) {
      Event e; e.id = i * 3 + 1; e.title = "T" + rtext(g, 50); e.location = rtext(g, 40); e.note = rtext(g, 200);
      e.start = daysFromCivil(2024 + g() % 5, 1 + g() % 12, 1 + g() % 31 % 28 + (g() % 4 == 0 ? 3 : 0));
      if (!parseDate(formatDate(e.start), e.start)) continue;
      e.end = e.start + (g() % 3 == 0 ? g() % 6 : 0);
      e.allDay = g() % 2; e.startMin = g() % 1440; e.endMin = g() % 3 ? g() % 1440 : 0;
      if (e.allDay) e.startMin = e.endMin = 0;
      e.repeat = static_cast<Repeat>(g() % 5); e.every = 1 + (g() % 4 == 0 ? g() % 40 : g() % 3);
      e.hasUntil = e.repeat != Repeat::None && g() % 3 == 0; e.until = e.start + g() % 900;
      ev.push_back(e);
    }
    ls.push_back({1, "Main To-Do's"}); ls.push_back({2, "Szerel\xC3\xA9s"}); ls.push_back({5, "Shop\ttab"});
    for (int i = 0; i < 60; i++) {
      Task t; t.id = i + 1; t.list = ls[g() % 3].id; t.title = "k" + rtext(g, 40); t.note = rtext(g, 100);
      t.created = daysFromCivil(2026, 1 + g() % 12, 1 + g() % 28);
      t.hasDue = g() % 2; t.due = t.created + g() % 30; t.done = g() % 3 == 0; t.doneOn = t.created + g() % 9;
      ts.push_back(t);
    }
    spit(dir + "/events.txt", serialize(ev)); spit(dir + "/tasks.txt", serializeTasks(ts)); spit(dir + "/lists.txt", serializeLists(ls));
    return 0;
  }
  // check: round trip + occurrences for the windows in argv[3]
  std::string e = slurp(dir + "/events.txt"), t = slurp(dir + "/tasks.txt"), l = slurp(dir + "/lists.txt");
  std::vector<Event> ev; std::vector<Task> ts; std::vector<TaskList> ls;
  parse(e, ev); parseTasks(t, ts); parseLists(l, ls);
  std::printf("RT events %s tasks %s lists %s\n", serialize(ev) == e ? "ok" : "DIFF", serializeTasks(ts) == t ? "ok" : "DIFF", serializeLists(ls) == l ? "ok" : "DIFF");
  std::printf("N %zu %zu %zu\n", ev.size(), ts.size(), ls.size());
  std::ifstream w(argv[3]); long a, b;
  while (w >> a >> b) {
    std::vector<Occurrence> o; occurrencesIn(ev, a, b, o);
    std::printf("W %ld %ld %zu\n", a, b, o.size());
    for (auto& x : o) std::printf("%u %d %d\n", ev[x.event].id, x.start, x.end);
  }
  return 0;
}
