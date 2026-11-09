#include <cassert>
#include <cstdio>

#include "games/reversi/ReversiBrain.h"
#include "games/reversi/ReversiCore.h"
#include "games/reversi/ReversiScreens.h"

namespace {

void testStart() {
  using namespace reversi;
  Game g;
  start(g);

  assert(!g.isOver);
  assert(g.turn == kBlack);
  assert(blackCount(g) == 2);
  assert(whiteCount(g) == 2);
  assert(cellAt(g, 3, 3) == kWhite);  // D4
  assert(cellAt(g, 4, 3) == kBlack);  // E4
  assert(cellAt(g, 3, 4) == kBlack);  // D5
  assert(cellAt(g, 4, 4) == kWhite);  // E5
  assert(cellAt(g, 0, 0) == kEmpty);

  // Black has 4 legal moves at start
  const uint64_t moves = legalMoves(g, kBlack);
  assert(countDiscs(moves) == 4);
  assert(moves & (1ULL << (3 * 8 + 2)));  // C4 (col 2, row 3)
  assert(moves & (1ULL << (2 * 8 + 3)));  // D3 (col 3, row 2)
  assert(moves & (1ULL << (5 * 8 + 4)));  // E6 (col 4, row 5)
  assert(moves & (1ULL << (4 * 8 + 5)));  // F5 (col 5, row 4)

  assert(hasLegalMoves(g, kBlack));
}

void testMoveAndFlips() {
  using namespace reversi;
  Game g;
  start(g);

  // Playing illegal move fails
  assert(!playMove(g, 0, 0));
  assert(!playMove(g, 3, 3));  // Occupied

  // Play legal move at C4 (col 2, row 3)
  assert(playMove(g, 2, 3));
  assert(cellAt(g, 2, 3) == kBlack);
  assert(cellAt(g, 3, 3) == kBlack);  // Flipped D4 from White to Black!
  assert(blackCount(g) == 4);
  assert(whiteCount(g) == 1);
  assert(g.turn == kWhite);

  // White now has legal moves
  assert(hasLegalMoves(g, kWhite));
  const uint64_t whiteMoves = legalMoves(g, kWhite);
  assert(countDiscs(whiteMoves) == 3);
}

void testAi() {
  using namespace reversi;
  Game g;
  start(g);

  BestMove bCasual = findBestMove(g, kBlack, Difficulty::Casual);
  assert(bCasual.col >= 0 && bCasual.row >= 0);
  const uint64_t moves = legalMoves(g, kBlack);
  assert(moves & (1ULL << (bCasual.row * 8 + bCasual.col)));

  BestMove bStd = findBestMove(g, kBlack, Difficulty::Standard);
  assert(bStd.col >= 0 && bStd.row >= 0);
  assert(moves & (1ULL << (bStd.row * 8 + bStd.col)));
}

void testOutcome() {
  using namespace reversi;
  Game g;
  start(g);
  assert(outcome(g) == Outcome::InProgress);

  g.isOver = true;
  g.black = 0x0F;  // 4 discs
  g.white = 0xFF;  // 8 discs
  assert(outcome(g) == Outcome::WhiteWon);

  g.white = 0x0F;  // 4 discs
  assert(outcome(g) == Outcome::Draw);

  g.black = 0xFF;  // 8 discs
  assert(outcome(g) == Outcome::BlackWon);
}

void testGeometry() {
  using namespace reversiui;
  const auto board = boardRect();
  assert(board.width == 448);
  assert(board.height == 448);

  int c = -1, r = -1;
  // Out of bounds
  assert(cellAt(0, 0, c, r) == -1);
  assert(cellAt(10, 100, c, r) == -1);

  // Top-left cell (0, 0)
  assert(cellAt(board.x + 2, board.y + 2, c, r) == 0);
  assert(c == 0 && r == 0);

  // Bottom-right cell (7, 7)
  assert(cellAt(board.right() - 2, board.bottom() - 2, c, r) == 63);
  assert(c == 7 && r == 7);

  // Flipped board: top-left pixel maps to (7, 7)
  assert(cellAt(board.x + 2, board.y + 2, c, r, true) == 63);
  assert(c == 7 && r == 7);

  // Flipped board: bottom-right pixel maps to (0, 0)
  assert(cellAt(board.right() - 2, board.bottom() - 2, c, r, true) == 0);
  assert(c == 0 && r == 0);
}

}  // namespace

int main() {
  testStart();
  testMoveAndFlips();
  testAi();
  testOutcome();
  testGeometry();
  std::printf("test_reversi: all tests passed\n");
  return 0;
}
