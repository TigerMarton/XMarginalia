#include <cassert>
#include <cstdio>

#include "../../src/apps_local/tabletop/games/santorini/SantoriniBrain.h"
#include "../../src/apps_local/tabletop/games/santorini/SantoriniCore.h"
#include "../../src/apps_local/tabletop/games/santorini/SantoriniScreens.h"

void test_initialization() {
  santorini::State state;
  santorini::initGame(state);

  assert(state.turn == santorini::Player::White);
  assert(state.phase == santorini::Phase::SelectWorker);
  assert(state.winner == santorini::Player::None);

  for (int i = 0; i < santorini::kCells; ++i) {
    assert(state.board[i] == 0);
  }

  assert(state.workers[0][0] == 6);   // (1, 1)
  assert(state.workers[0][1] == 18);  // (3, 3)
  assert(state.workers[1][0] == 8);   // (3, 1)
  assert(state.workers[1][1] == 16);  // (1, 3)

  santorini::Player owner;
  assert(santorini::workerAt(state, 6, owner) == 0 && owner == santorini::Player::White);
  assert(santorini::workerAt(state, 18, owner) == 1 && owner == santorini::Player::White);
  assert(santorini::workerAt(state, 8, owner) == 0 && owner == santorini::Player::Black);
  assert(santorini::workerAt(state, 16, owner) == 1 && owner == santorini::Player::Black);
  assert(santorini::workerAt(state, 0, owner) == -1);

  printf("test_initialization passed\n");
}

void test_worker_movement_rules() {
  santorini::State state;
  santorini::initGame(state);

  // Worker 0 is at 6 (x=1, y=1)
  assert(santorini::selectWorker(state, 0));
  assert(state.phase == santorini::Phase::SelectMove);
  assert(state.selectedWorker == 0);

  // Non-adjacent move should fail
  assert(!santorini::executeMove(state, 24));

  // Move to cell 8 (occupied by Black worker 0) should fail
  assert(!santorini::executeMove(state, 8));

  // Height climb restriction:
  // Set cell 7 (x=2, y=1) to level 2 (from ground level 0, +2 is illegal)
  state.board[7] = 2;
  assert(!santorini::executeMove(state, 7));

  // Set cell 7 to level 1 (+1 is legal)
  state.board[7] = 1;
  assert(santorini::executeMove(state, 7));
  assert(state.workers[0][0] == 7);
  assert(state.phase == santorini::Phase::SelectBuild);

  printf("test_worker_movement_rules passed\n");
}

void test_building_rules() {
  santorini::State state;
  santorini::initGame(state);

  // Select worker 0 at 6 and move to 1
  assert(santorini::selectWorker(state, 0));
  assert(santorini::executeMove(state, 1));
  assert(state.phase == santorini::Phase::SelectBuild);

  // Non-adjacent build to (4, 4) = 24 should fail
  assert(!santorini::executeBuild(state, 24));

  // Build on cell 0 (adjacent to 1)
  assert(state.board[0] == 0);
  assert(santorini::executeBuild(state, 0));
  assert(state.board[0] == 1);

  // Turn should now pass to Black
  assert(state.turn == santorini::Player::Black);
  assert(state.phase == santorini::Phase::SelectWorker);
  assert(state.selectedWorker == -1);

  printf("test_building_rules passed\n");
}

void test_win_by_climbing_level_3() {
  santorini::State state;
  santorini::initGame(state);

  // Setup: worker 0 at 6 is at level 2, adjacent cell 7 is level 3
  state.board[6] = 2;
  state.board[7] = 3;

  assert(santorini::selectWorker(state, 0));
  assert(santorini::executeMove(state, 7));

  // Should win immediately without building!
  assert(state.phase == santorini::Phase::GameOver);
  assert(state.winner == santorini::Player::White);

  printf("test_win_by_climbing_level_3 passed\n");
}

void test_win_by_trapping() {
  santorini::State state;
  santorini::initGame(state);

  // Trap all Black workers:
  // Black worker 0 is at 8 (x=3, y=1), worker 1 is at 16 (x=1, y=3)
  // Surround 8 with domes (level 4)
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) continue;
      state.board[santorini::cellIdx(3 + dx, 1 + dy)] = 4;
    }
  }
  // Surround 16 with domes
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) continue;
      state.board[santorini::cellIdx(1 + dx, 3 + dy)] = 4;
    }
  }

  // Black has no legal moves
  assert(!santorini::hasAnyLegalMoves(state, santorini::Player::Black));

  // White makes a move and builds
  assert(santorini::selectWorker(state, 0));
  assert(santorini::executeMove(state, 0));
  assert(santorini::executeBuild(state, 5));

  // White wins because Black is trapped!
  assert(state.phase == santorini::Phase::GameOver);
  assert(state.winner == santorini::Player::White);

  printf("test_win_by_trapping passed\n");
}

void test_bot_ai() {
  santorini::State state;
  santorini::initGame(state);
  state.turn = santorini::Player::Black;

  // Give Black an immediate winning move: worker 0 at 8 can move to 7 (level 3)
  state.board[8] = 2;
  state.board[7] = 3;

  santorini::Move bestMove;
  bool found = santorini::SantoriniBrain::findBestMove(state, bestMove, 2);
  assert(found);
  assert(bestMove.worker == 0);
  assert(bestMove.moveCell == 7);

  printf("test_bot_ai passed\n");
}

void test_geometry() {
  // Test cellAt using board constants
  const int bx = santoriniui::kBoardLeft;
  const int by = santoriniui::kBoardTop;
  assert(santoriniui::cellAt(bx + 10, by + 10, false) == 0);
  assert(santoriniui::cellAt(bx + 75, by + 10, false) == 1);
  assert(santoriniui::cellAt(bx + 10, by + 75, false) == 5);
  assert(santoriniui::cellAt(bx + 340, by + 340, false) == 24);

  // Outside bounds
  assert(santoriniui::cellAt(10, 10, false) == -1);
  assert(santoriniui::cellAt(450, by, false) == -1);

  // Flip 180
  assert(santoriniui::cellAt(bx + 10, by + 10, true) == 24);
  assert(santoriniui::cellAt(bx + 340, by + 340, true) == 0);

  printf("test_geometry passed\n");
}

int main() {
  test_initialization();
  test_worker_movement_rules();
  test_building_rules();
  test_win_by_climbing_level_3();
  test_win_by_trapping();
  test_bot_ai();
  test_geometry();
  printf("test_santorini: all tests passed\n");
  return 0;
}
