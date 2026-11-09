#include <cassert>
#include <cstdio>
#include "../../src/apps_local/tabletop/games/dobutsu/DobutsuCore.h"
#include "../../src/apps_local/tabletop/games/dobutsu/DobutsuBrain.h"
#include "../../src/apps_local/tabletop/games/dobutsu/DobutsuScreens.h"

using namespace dobutsu;
using namespace dobutsuui;
namespace fui = freeink::ui;

void test_initial_board() {
  Game g;
  initGame(g);
  assert(!g.isOver);
  assert(g.turn == Player::P1);
  assert(g.winner == Player::None);

  // Check P2 row 0
  assert(pieceOwner(g.board[0]) == Player::P2 && pieceType(g.board[0]) == PieceType::Giraffe);
  assert(pieceOwner(g.board[1]) == Player::P2 && pieceType(g.board[1]) == PieceType::Lion);
  assert(pieceOwner(g.board[2]) == Player::P2 && pieceType(g.board[2]) == PieceType::Elephant);

  // Check Chicks
  assert(pieceOwner(g.board[4]) == Player::P2 && pieceType(g.board[4]) == PieceType::Chick);
  assert(pieceOwner(g.board[7]) == Player::P1 && pieceType(g.board[7]) == PieceType::Chick);

  // Check P1 row 3
  assert(pieceOwner(g.board[9]) == Player::P1 && pieceType(g.board[9]) == PieceType::Elephant);
  assert(pieceOwner(g.board[10]) == Player::P1 && pieceType(g.board[10]) == PieceType::Lion);
  assert(pieceOwner(g.board[11]) == Player::P1 && pieceType(g.board[11]) == PieceType::Giraffe);

  // Reserves empty
  for (int p = 0; p < 2; ++p) {
    for (int r = 0; r < 3; ++r) {
      assert(g.reserves[p][r] == 0);
    }
  }
  std::printf("test_initial_board passed\n");
}

void test_chick_advance_and_capture() {
  Game g;
  initGame(g);

  // P1 moves Chick from 7 to 4 (capturing P2's Chick at 4)
  Move m1{7, 4, false};
  assert(isLegalMove(g, m1));
  assert(applyMove(g, m1));

  // P1 captured chick -> P1 reserve chick count = 1
  assert(g.reserves[0][reserveIndex(PieceType::Chick)] == 1);
  assert(g.board[7] == 0);
  assert(pieceOwner(g.board[4]) == Player::P1 && pieceType(g.board[4]) == PieceType::Chick);
  assert(g.turn == Player::P2);

  // P2 lion at 1 can capture P1 chick at 4
  Move m2{1, 4, false};
  assert(isLegalMove(g, m2));
  assert(applyMove(g, m2));

  // P2 captured chick -> P2 reserve chick count = 1
  assert(g.reserves[1][reserveIndex(PieceType::Chick)] == 1);
  assert(g.board[1] == 0);
  assert(pieceOwner(g.board[4]) == Player::P2 && pieceType(g.board[4]) == PieceType::Lion);
  assert(g.turn == Player::P1);

  std::printf("test_chick_advance_and_capture passed\n");
}

void test_promotion_and_drop() {
  Game g;
  initGame(g);

  // Clear board for a clean promotion test
  for (int i = 0; i < kCells; ++i) g.board[i] = 0;
  g.board[10] = makePiece(Player::P1, PieceType::Lion);
  g.board[1] = makePiece(Player::P2, PieceType::Lion);

  // Put a P1 chick at sq 3 (row 1, col 0)
  g.board[3] = makePiece(Player::P1, PieceType::Chick);
  g.turn = Player::P1;

  // Move chick to sq 0 (row 0, col 0) -> promotes to Hen!
  Move m1{3, 0, true};
  assert(isLegalMove(g, m1));
  assert(applyMove(g, m1));

  assert(pieceOwner(g.board[0]) == Player::P1);
  assert(pieceType(g.board[0]) == PieceType::Hen);

  // Give P2 a chick in reserve and drop it at sq 4
  g.reserves[1][reserveIndex(PieceType::Chick)] = 1;
  Move mDrop{kDropChick, 4, false};
  assert(isLegalMove(g, mDrop));
  assert(applyMove(g, mDrop));

  assert(g.reserves[1][reserveIndex(PieceType::Chick)] == 0);
  assert(pieceOwner(g.board[4]) == Player::P2);
  assert(pieceType(g.board[4]) == PieceType::Chick);

  std::printf("test_promotion_and_drop passed\n");
}

void test_catch_the_lion_win() {
  Game g;
  initGame(g);

  // P1 giraffe at 11 captures P2 lion directly (setup custom position)
  for (int i = 0; i < kCells; ++i) g.board[i] = 0;
  g.board[4] = makePiece(Player::P1, PieceType::Giraffe);
  g.board[1] = makePiece(Player::P2, PieceType::Lion);
  g.board[10] = makePiece(Player::P1, PieceType::Lion);
  g.turn = Player::P1;

  Move mCap{4, 1, false};
  assert(isLegalMove(g, mCap));
  assert(applyMove(g, mCap));

  assert(g.isOver);
  assert(g.winner == Player::P1);

  std::printf("test_catch_the_lion_win passed\n");
}

void test_bot_ai() {
  Game g;
  initGame(g);

  // Bot should find a legal opening move
  Move m = chooseBotMove(g, Difficulty::Standard, 12345);
  assert(isLegalMove(g, m));

  // If a winning capture is available, bot must take it
  for (int i = 0; i < kCells; ++i) g.board[i] = 0;
  g.board[4] = makePiece(Player::P1, PieceType::Giraffe);
  g.board[1] = makePiece(Player::P2, PieceType::Lion);
  g.board[10] = makePiece(Player::P1, PieceType::Lion);
  g.turn = Player::P1;

  Move winMove = chooseBotMove(g, Difficulty::Standard, 12345);
  assert(winMove.from == 4 && winMove.to == 1);

  std::printf("test_bot_ai passed\n");
}

void test_geometry() {
  const fui::Rect content = fui::makeRect(16, 24, 448, 700);
  const fui::Rect grid = boardGridRect(content);
  assert(grid.width == 300);
  assert(grid.height == 368);

  // Top-left cell without flip is sq 0
  const fui::Rect c0 = cellRect(grid, 0, false);
  assert(c0.x == grid.x && c0.y == grid.y);
  assert(c0.width == 100 && c0.height == 92);

  // With flip, sq 0 is bottom-right: col 2, row 3
  const fui::Rect c0Flipped = cellRect(grid, 0, true);
  assert(c0Flipped.x == grid.x + 200 && c0Flipped.y == grid.y + 3 * 92);

  std::printf("test_geometry passed\n");
}

int main() {
  test_initial_board();
  test_chick_advance_and_capture();
  test_promotion_and_drop();
  test_catch_the_lion_win();
  test_bot_ai();
  test_geometry();
  std::printf("test_dobutsu: all tests passed\n");
  return 0;
}
