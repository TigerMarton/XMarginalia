#include <cassert>
#include <cstdio>
#include <cstring>

#include "TabletopFlow.h"

namespace {

void testFlow() {
  using namespace tabletop;

  assert(leavesApp(Screen::Lobby) == true);
  assert(leavesApp(Screen::Reversi) == false);
  assert(leavesApp(Screen::Mancala) == false);
  assert(leavesApp(Screen::Morris) == false);
  assert(leavesApp(Screen::Santorini) == false);
  assert(leavesApp(Screen::Quoridor) == false);
  assert(leavesApp(Screen::Onitama) == false);
  assert(leavesApp(Screen::Dobutsu) == false);
  assert(leavesApp(Screen::Dominoes) == false);
  assert(leavesApp(Screen::LiarsDice) == false);
  assert(leavesApp(Screen::Backgammon) == false);
  assert(leavesApp(Screen::Sandbox) == false);
  assert(leavesApp(Screen::About) == false);

  assert(back(Screen::Lobby) == Screen::Lobby);
  assert(back(Screen::Reversi) == Screen::Lobby);
  assert(back(Screen::Mancala) == Screen::Lobby);
  assert(back(Screen::Morris) == Screen::Lobby);
  assert(back(Screen::Santorini) == Screen::Lobby);
  assert(back(Screen::Quoridor) == Screen::Lobby);
  assert(back(Screen::Onitama) == Screen::Lobby);
  assert(back(Screen::Dobutsu) == Screen::Lobby);
  assert(back(Screen::Dominoes) == Screen::Lobby);
  assert(back(Screen::LiarsDice) == Screen::Lobby);
  assert(back(Screen::Backgammon) == Screen::Lobby);
  assert(back(Screen::Sandbox) == Screen::Lobby);
  assert(back(Screen::About) == Screen::Lobby);

  assert(static_cast<int>(LobbyRow::Count) == 12);
}

}  // namespace

int main() {
  testFlow();
  std::printf("test_tabletop: all flow tests passed\n");
  return 0;
}
