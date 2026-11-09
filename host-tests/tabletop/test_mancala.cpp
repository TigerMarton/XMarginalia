#include <cassert>
#include <cstdio>

#include "games/mancala/MancalaBrain.h"
#include "games/mancala/MancalaCore.h"
#include "games/mancala/MancalaScreens.h"

namespace {

void testStart() {
  using namespace mancala;
  Game g;
  start(g);

  assert(!g.isOver);
  assert(!g.extraTurn);
  assert(g.turn == kPlayer1);

  for (int i = 0; i < 6; ++i) {
    assert(g.pits[i] == 4);
    assert(g.pits[7 + i] == 4);
  }
  assert(g.pits[kStore1] == 0);
  assert(g.pits[kStore2] == 0);

  assert(playerPitSeeds(g, kPlayer1) == 24);
  assert(playerPitSeeds(g, kPlayer2) == 24);
  assert(hasLegalMoves(g, kPlayer1));
  assert(hasLegalMoves(g, kPlayer2));
}

void testBasicSowing() {
  using namespace mancala;
  Game g;
  start(g);

  // Play pit 0 (contains 4 seeds: will sow into 1, 2, 3, 4)
  assert(playMove(g, 0));
  assert(g.pits[0] == 0);
  assert(g.pits[1] == 5);
  assert(g.pits[2] == 5);
  assert(g.pits[3] == 5);
  assert(g.pits[4] == 5);
  assert(g.pits[5] == 4);
  assert(g.pits[kStore1] == 0);

  assert(!g.extraTurn);
  assert(g.turn == kPlayer2);  // Switched turn to Player 2
}

void testExtraTurn() {
  using namespace mancala;
  Game g;
  start(g);

  // Pit 2 has 4 seeds: sows into 3, 4, 5, and store 6!
  assert(playMove(g, 2));
  assert(g.pits[2] == 0);
  assert(g.pits[3] == 5);
  assert(g.pits[4] == 5);
  assert(g.pits[5] == 5);
  assert(g.pits[kStore1] == 1);

  assert(g.extraTurn);
  assert(g.turn == kPlayer1);  // Retains turn!
}

void testCapture() {
  using namespace mancala;
  Game g;
  start(g);

  // Clear pit 4, put 1 seed in pit 3, put 5 seeds in opposite pit 8 (12 - 4 = 8)
  g.pits[4] = 0;
  g.pits[3] = 1;
  g.pits[8] = 5;
  g.pits[kStore1] = 0;
  g.turn = kPlayer1;

  // Play pit 3: 1 seed moves to pit 4 (empty friendly pit!)
  assert(playMove(g, 3));
  // Captured: 1 from pit 4 + 5 from pit 8 = 6 seeds into store 1!
  assert(g.pits[4] == 0);
  assert(g.pits[8] == 0);
  assert(g.pits[kStore1] == 6);
}

void testGameOverSweep() {
  using namespace mancala;
  Game g;
  start(g);

  // Set all Player 1 pits to 0 except pit 5 with 1 seed
  for (int i = 0; i < 5; ++i) g.pits[i] = 0;
  g.pits[5] = 1;
  g.pits[kStore1] = 10;
  g.pits[kStore2] = 10;
  g.turn = kPlayer1;

  // Play pit 5: 1 seed goes to Store 1. Player 1's pits become completely empty!
  assert(playMove(g, 5));
  assert(g.isOver);
  // Player 2's pits (24 seeds) must be swept into Store 2
  assert(g.pits[kStore1] == 11);
  assert(g.pits[kStore2] == 10 + 24);
  assert(outcome(g) == Outcome::Player2Won);
}

void testAi() {
  using namespace mancala;
  Game g;
  start(g);

  BestMove bCasual = findBestMove(g, kPlayer1, Difficulty::Casual);
  assert(bCasual.pit >= 0 && bCasual.pit < 6);

  BestMove bStd = findBestMove(g, kPlayer1, Difficulty::Standard);
  assert(bStd.pit >= 0 && bStd.pit < 6);
  assert(g.pits[bStd.pit] > 0);
}

void testGeometry() {
  using namespace mancalaui;
  const auto board = boardRect();
  assert(board.width == 448);
  assert(board.height == 356);

  // Stores
  assert(pitAt(30, 200) == mancala::kStore2);
  assert(pitAt(420, 200) == mancala::kStore1);

  // Out of bounds
  assert(pitAt(0, 0) == -1);
  assert(pitAt(10, 100) == -1);

  // South pit 0 (bottom row, leftmost)
  assert(pitAt(85, 350) == 0);
  // South pit 5 (bottom row, rightmost)
  assert(pitAt(360, 350) == 5);

  // North pit 12 (top row, leftmost)
  assert(pitAt(85, 200) == 12);
  // North pit 7 (top row, rightmost)
  assert(pitAt(360, 200) == 7);

  // Flipped board: Store 1 at left, Store 2 at right
  assert(pitAt(30, 200, true) == mancala::kStore1);
  assert(pitAt(420, 200, true) == mancala::kStore2);

  // Flipped board: North pits 7..12 at bottom (7 leftmost, 12 rightmost)
  assert(pitAt(85, 350, true) == 7);
  assert(pitAt(360, 350, true) == 12);

  // Flipped board: South pits 5..0 at top (5 leftmost, 0 rightmost)
  assert(pitAt(85, 200, true) == 5);
  assert(pitAt(360, 200, true) == 0);
}

}  // namespace

int main() {
  testStart();
  testBasicSowing();
  testExtraTurn();
  testCapture();
  testGameOverSweep();
  testAi();
  testGeometry();
  std::printf("test_mancala: all tests passed\n");
  return 0;
}
