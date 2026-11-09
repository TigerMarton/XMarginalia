#include <cassert>
#include <cstdio>

#include "../../src/apps_local/tabletop/games/quoridor/QuoridorBrain.h"
#include "../../src/apps_local/tabletop/games/quoridor/QuoridorCore.h"
#include "../../src/apps_local/tabletop/games/quoridor/QuoridorScreens.h"

void test_initialization() {
  quoridor::State state;
  quoridor::initGame(state);

  assert(state.pawn[0] == 76);  // White (4, 8)
  assert(state.pawn[1] == 4);   // Black (4, 0)
  assert(state.wallsLeft[0] == 10);
  assert(state.wallsLeft[1] == 10);
  assert(state.hWalls == 0);
  assert(state.vWalls == 0);
  assert(state.turn == quoridor::Player::White);
  assert(state.winner == quoridor::Player::None);

  printf("test_initialization passed\n");
}

void test_pawn_movement_rules() {
  quoridor::State state;
  quoridor::initGame(state);

  // White at (4, 8)
  int8_t moves[8];
  int count = quoridor::getLegalPawnMoves(state, moves, 8);
  assert(count == 3);  // (4, 7), (3, 8), (5, 8)

  // Direct move
  assert(quoridor::executePawnMove(state, 67));  // (4, 7)
  assert(state.pawn[0] == 67);
  assert(state.turn == quoridor::Player::Black);

  // Direct Jump test:
  // Set White at (4, 4) = 40, Black at (4, 3) = 31
  state.pawn[0] = 40;
  state.pawn[1] = 31;
  state.turn = quoridor::Player::White;

  count = quoridor::getLegalPawnMoves(state, moves, 8);
  // White can jump directly over Black to (4, 2) = 22
  bool hasJump = false;
  for (int i = 0; i < count; ++i) {
    if (moves[i] == 22) hasJump = true;
  }
  assert(hasJump);

  // Diagonal Jump test:
  // Place wall behind Black to block direct jump: H-wall at (3, 2) or (4, 2)
  state.hWalls |= (1ULL << quoridor::interIdx(4, 2));
  count = quoridor::getLegalPawnMoves(state, moves, 8);
  // Direct jump 22 blocked, diagonal jumps to (3, 3) = 30 and (5, 3) = 32 available
  bool hasDiagLeft = false;
  bool hasDiagRight = false;
  for (int i = 0; i < count; ++i) {
    if (moves[i] == 30) hasDiagLeft = true;
    if (moves[i] == 32) hasDiagRight = true;
  }
  assert(hasDiagLeft);
  assert(hasDiagRight);

  printf("test_pawn_movement_rules passed\n");
}

void test_wall_placement_and_overlap() {
  quoridor::State state;
  quoridor::initGame(state);

  // Place H-wall at (3, 3)
  assert(quoridor::canPlaceWall(state, 3, 3, quoridor::WallOrientation::Horizontal));
  assert(quoridor::executeWallPlacement(state, 3, 3, quoridor::WallOrientation::Horizontal));
  assert(state.wallsLeft[0] == 9);
  assert(state.turn == quoridor::Player::Black);

  state.turn = quoridor::Player::White;
  // Overlap checks:
  assert(!quoridor::canPlaceWall(state, 3, 3, quoridor::WallOrientation::Horizontal));  // exact same
  assert(!quoridor::canPlaceWall(state, 2, 3, quoridor::WallOrientation::Horizontal));  // left overlap
  assert(!quoridor::canPlaceWall(state, 4, 3, quoridor::WallOrientation::Horizontal));  // right overlap
  assert(!quoridor::canPlaceWall(state, 3, 3, quoridor::WallOrientation::Vertical));    // cross at center

  // Non-overlapping vertical wall at (1, 1) is fine
  assert(quoridor::canPlaceWall(state, 1, 1, quoridor::WallOrientation::Vertical));
  assert(quoridor::executeWallPlacement(state, 1, 1, quoridor::WallOrientation::Vertical));
  assert(state.wallsLeft[0] == 8);

  printf("test_wall_placement_and_overlap passed\n");
}

void test_fair_play_path_validation() {
  quoridor::State state;
  quoridor::initGame(state);

  // Move White to corner (0, 8) = 72
  state.pawn[0] = 72;
  state.turn = quoridor::Player::White;

  // Place H-wall at (0, 7): blocks (0, 8)->(0, 7) and (1, 8)->(1, 7)
  assert(quoridor::canPlaceWall(state, 0, 7, quoridor::WallOrientation::Horizontal));
  assert(quoridor::executeWallPlacement(state, 0, 7, quoridor::WallOrientation::Horizontal));

  // Now White can only escape via (1, 8) -> (2, 8).
  // Placing a V-wall at (1, 7) blocks (1, 8) -> (2, 8), which completely traps White!
  state.turn = quoridor::Player::White;
  assert(!quoridor::canPlaceWall(state, 1, 7, quoridor::WallOrientation::Vertical));

  printf("test_fair_play_path_validation passed\n");
}

void test_win_condition() {
  quoridor::State state;
  quoridor::initGame(state);

  // Put Black pawn safely elsewhere
  state.pawn[1] = 60;

  // Put White at (4, 1) = 13
  state.pawn[0] = 13;
  state.turn = quoridor::Player::White;

  // Move White to (4, 0) = 4
  assert(quoridor::executePawnMove(state, 4));
  assert(state.winner == quoridor::Player::White);

  printf("test_win_condition passed\n");
}

void test_bot_ai() {
  quoridor::State state;
  quoridor::initGame(state);
  state.turn = quoridor::Player::Black;

  quoridor::Move bestMove;
  bool found = quoridor::QuoridorBrain::findBestMove(state, bestMove, 2);
  assert(found);

  // Execute the AI move
  assert(quoridor::executeMove(state, bestMove));

  printf("test_bot_ai passed\n");
}

void test_geometry() {
  const int bx = quoridorui::kBoardLeft;
  const int by = quoridorui::kBoardTop;

  // Cell (0, 0)
  assert(quoridorui::cellAt(bx + 10, by + 10, false) == 0);
  // Cell (1, 0)
  assert(quoridorui::cellAt(bx + 42 + 10, by + 10, false) == 1);
  // Cell (8, 8) = 80
  assert(quoridorui::cellAt(bx + 8 * 42 + 10, by + 8 * 42 + 10, false) == 80);

  // Groove hit test at intersection (0, 0): centered at (bx + 36, by + 36)
  int8_t gx = -1, gy = -1;
  assert(quoridorui::grooveAt(bx + 38, by + 38, gx, gy, false));
  assert(gx == 0 && gy == 0);

  // Outside bounds
  assert(quoridorui::cellAt(10, 10, false) == -1);

  printf("test_geometry passed\n");
}

int main() {
  test_initialization();
  test_pawn_movement_rules();
  test_wall_placement_and_overlap();
  test_fair_play_path_validation();
  test_win_condition();
  test_bot_ai();
  test_geometry();
  printf("test_quoridor: all tests passed\n");
  return 0;
}
