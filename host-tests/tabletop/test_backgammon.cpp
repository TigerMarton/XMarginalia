#include <cassert>
#include <cstdio>
#include "../../src/apps_local/tabletop/games/backgammon/BackgammonCore.h"
#include "../../src/apps_local/tabletop/games/backgammon/BackgammonBrain.h"
#include "../../src/apps_local/tabletop/games/backgammon/BackgammonScreens.h"

using namespace backgammon;
using namespace backgammonui;

void test_initialization() {
  Game g;
  initGame(g);

  assert(!g.isOver);
  assert(!g.isRolled);
  assert(g.turn == Player::White);
  assert(g.bar[0] == 0 && g.bar[1] == 0);
  assert(g.borneOff[0] == 0 && g.borneOff[1] == 0);

  // Check 15 checkers per player
  int whiteCount = 0, blackCount = 0;
  for (int i = 0; i < kPoints; ++i) {
    if (g.board[i] > 0) whiteCount += g.board[i];
    if (g.board[i] < 0) blackCount += (-g.board[i]);
  }
  assert(whiteCount == 15);
  assert(blackCount == 15);

  // Standard starting points
  assert(g.board[23] == 2);
  assert(g.board[12] == 5);
  assert(g.board[7] == 3);
  assert(g.board[5] == 5);

  assert(g.board[0] == -2);
  assert(g.board[11] == -5);
  assert(g.board[16] == -3);
  assert(g.board[18] == -5);

  // Initial pip count
  // White: 2*24 + 5*13 + 3*8 + 5*6 = 48 + 65 + 24 + 30 = 167
  // Black: 2*24 + 5*13 + 3*8 + 5*6 = 167
  assert(calculatePipCount(g, Player::White) == 167);
  assert(calculatePipCount(g, Player::Black) == 167);

  std::printf("test_initialization passed\n");
}

void test_rolling_and_moves() {
  Game g;
  initGame(g);

  rollDice(g, 100);
  assert(g.isRolled);
  assert(g.moveCount == 2 || g.moveCount == 4);
  assert(g.dice[0] >= 1 && g.dice[0] <= 6);
  assert(g.dice[1] >= 1 && g.dice[1] <= 6);

  // Generate legal opening moves
  Move moves[64];
  size_t nMoves = generateLegalMoves(g, moves, 64);
  assert(nMoves > 0);

  // Every generated move must pass isLegalMove
  for (size_t i = 0; i < nMoves; ++i) {
    assert(isLegalMove(g, moves[i]));
  }

  std::printf("test_rolling_and_moves passed\n");
}

void test_hitting_and_bar() {
  Game g;
  initGame(g);

  // Clear board
  for (int i = 0; i < kPoints; ++i) g.board[i] = 0;

  // Put a single Black blot at point 10
  g.board[10] = -1;
  // Put a White checker at point 13
  g.board[13] = 1;

  g.turn = Player::White;
  g.isRolled = true;
  g.dice[0] = 3; g.dice[1] = 5;
  g.movesLeft[0] = 3; g.movesLeft[1] = 5;
  g.moveCount = 2;

  // White moves 13 -> 10 with die 3 (hits Black blot!)
  Move hitMove{13, 10, 3};
  assert(isLegalMove(g, hitMove));
  assert(applyMove(g, hitMove));

  assert(g.board[13] == 0);
  assert(g.board[10] == 1);  // Now White checker
  assert(g.bar[1] == 1);     // Black checker sent to bar!
  assert(g.moveCount == 1);  // 1 die left (5)

  // Black's turn: Black on bar must enter into points 0..5
  passTurn(g);
  assert(g.turn == Player::Black);
  g.isRolled = true;
  g.dice[0] = 4; g.dice[1] = 2;
  g.movesLeft[0] = 4; g.movesLeft[1] = 2;
  g.moveCount = 2;

  // Black must re-enter from bar
  Move nonBarMove{0, 2, 2};
  assert(!isLegalMove(g, nonBarMove));

  // Re-enter with die 4 -> point 3 (target = die - 1 = 3)
  Move reEnterMove{kFromBar, 3, 4};
  assert(isLegalMove(g, reEnterMove));
  assert(applyMove(g, reEnterMove));

  assert(g.bar[1] == 0);
  assert(g.board[3] == -1);

  std::printf("test_hitting_and_bar passed\n");
}

void test_bearing_off() {
  Game g;
  initGame(g);

  // Clear board and place all White checkers in home board (0..5)
  for (int i = 0; i < kPoints; ++i) g.board[i] = 0;
  g.board[0] = 5;
  g.board[1] = 5;
  g.board[2] = 5;
  assert(canBearOff(g, Player::White));

  g.turn = Player::White;
  g.isRolled = true;
  g.dice[0] = 3; g.dice[1] = 1;
  g.movesLeft[0] = 3; g.movesLeft[1] = 1;
  g.moveCount = 2;

  // Point 2 with die 3 -> exact bear-off: 2 - 3 = -1!
  Move bearExact{2, kToOff, 3};
  assert(isLegalMove(g, bearExact));
  assert(applyMove(g, bearExact));

  assert(g.board[2] == 4);
  assert(g.borneOff[0] == 1);

  // Point 0 with die 1 -> exact bear-off: 0 - 1 = -1!
  Move bearExact0{0, kToOff, 1};
  assert(isLegalMove(g, bearExact0));
  assert(applyMove(g, bearExact0));

  assert(g.board[0] == 4);
  assert(g.borneOff[0] == 2);

  std::printf("test_bearing_off passed\n");
}

void test_bot_ai() {
  Game g;
  initGame(g);
  rollDice(g, 999);

  Move m = chooseBotMove(g, Difficulty::Standard, 42);
  assert(isLegalMove(g, m));

  // If a hitting move is available, bot should strongly prefer it
  for (int i = 0; i < kPoints; ++i) g.board[i] = 0;
  g.board[10] = -1;  // Black blot
  g.board[13] = 1;   // White checker
  g.turn = Player::White;
  g.isRolled = true;
  g.dice[0] = 3; g.dice[1] = 6;
  g.movesLeft[0] = 3; g.movesLeft[1] = 6;
  g.moveCount = 2;

  Move best = chooseBotMove(g, Difficulty::Standard, 42);
  assert(best.from == 13 && best.to == 10 && best.die == 3);

  std::printf("test_bot_ai passed\n");
}

void test_geometry() {
  const fui::Rect b = boardRect();
  assert(b.width == 456 && b.height == 460);

  const fui::Rect bar = barRect();
  assert(bar.x == 204 && bar.width == 32);

  // Bar hit testing
  assert(pointAt(210, 300, false) == kFromBar);

  // Bear-off hit testing
  assert(pointAt(440, 200, false) == kToOff);

  // Point hit testing (unflipped)
  // Top left (col 0): point 12
  assert(pointAt(20, 200, false) == 12);
  // Top right (col 11): point 23
  assert(pointAt(410, 200, false) == 23);
  // Bottom left (col 0): point 11
  assert(pointAt(20, 500, false) == 11);
  // Bottom right (col 11): point 0
  assert(pointAt(410, 500, false) == 0);

  // Point hit testing (flipped)
  // Top left (col 0): point 0
  assert(pointAt(20, 200, true) == 0);
  // Bottom right (col 11): point 12
  assert(pointAt(410, 500, true) == 12);

  // Out of bounds
  assert(pointAt(0, 0, false) == -99);
  assert(pointAt(500, 500, false) == -99);

  std::printf("test_geometry passed\n");
}

int main() {
  test_initialization();
  test_rolling_and_moves();
  test_hitting_and_bar();
  test_bearing_off();
  test_bot_ai();
  test_geometry();
  std::printf("test_backgammon: all tests passed\n");
  return 0;
}
