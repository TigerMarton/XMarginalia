#include <cassert>
#include <cstdio>
#include <cstring>

#include "games/onitama/OnitamaBrain.h"
#include "games/onitama/OnitamaCore.h"
#include "games/onitama/OnitamaScreens.h"

void test_initial_state() {
  onitama::Game g = onitama::initGame(42);
  assert(!g.isOver);
  assert(g.winner == 0);
  assert(g.turn == onitama::kPlayer1);

  // Check piece counts
  int p1Students = 0, p1Masters = 0, p2Students = 0, p2Masters = 0;
  for (int i = 0; i < onitama::kBoardCells; ++i) {
    if (g.cells[i] == onitama::kP1Student) ++p1Students;
    else if (g.cells[i] == onitama::kP1Master) ++p1Masters;
    else if (g.cells[i] == onitama::kP2Student) ++p2Students;
    else if (g.cells[i] == onitama::kP2Master) ++p2Masters;
  }
  assert(p1Students == 4);
  assert(p1Masters == 1);
  assert(p2Students == 4);
  assert(p2Masters == 1);
  assert(g.cells[onitama::kP1Temple] == onitama::kP1Master);
  assert(g.cells[onitama::kP2Temple] == onitama::kP2Master);

  // Check 5 distinct cards
  uint8_t cards[5] = {g.p1Cards[0], g.p1Cards[1], g.p2Cards[0], g.p2Cards[1], g.tableCard};
  for (int i = 0; i < 5; ++i) {
    assert(cards[i] < 16);
    for (int j = i + 1; j < 5; ++j) {
      assert(cards[i] != cards[j]);
    }
  }

  std::printf("test_initial_state passed\n");
}

void test_legal_moves() {
  onitama::Game g = onitama::initGame(42);
  onitama::Move moves[64];
  const int count = onitama::legalMoves(g, moves, 64);
  assert(count > 0);
  assert(count <= 64);

  // All moves must be from P1 pieces and not land on P1 pieces
  for (int i = 0; i < count; ++i) {
    assert(onitama::isP1(g.cells[moves[i].from]));
    assert(!onitama::isP1(g.cells[moves[i].to]));
    assert(moves[i].cardIdx <= 1);
    assert(onitama::isLegalMove(g, moves[i]));
  }

  std::printf("test_legal_moves passed\n");
}

void test_card_rotation() {
  onitama::Game g = onitama::initGame(42);
  const uint8_t initialP1Card0 = g.p1Cards[0];
  const uint8_t initialTableCard = g.tableCard;

  onitama::Move moves[64];
  const int count = onitama::legalMoves(g, moves, 64);
  assert(count > 0);

  // Find a move using cardIdx 0
  int moveIdx = -1;
  for (int i = 0; i < count; ++i) {
    if (moves[i].cardIdx == 0) {
      moveIdx = i;
      break;
    }
  }
  assert(moveIdx >= 0);

  const bool ok = onitama::applyMove(g, moves[moveIdx]);
  assert(ok);
  assert(g.turn == onitama::kPlayer2);

  // Used card became the table card, old table card went to player's hand
  assert(g.tableCard == initialP1Card0);
  assert(g.p1Cards[0] == initialTableCard);

  std::printf("test_card_rotation passed\n");
}

void test_stone_win() {
  onitama::Game g = onitama::initGame(42);
  // Set up an instant capture of P2's master
  g.cells[12] = onitama::kP1Master; // (2, 2)
  g.cells[7] = onitama::kP2Master;  // (2, 1) right in front
  // Force Tiger card on P1 (dx=0, dy=1 or dy=2)
  // Tiger has move (0, 2) and (0, -1). Let's give Crab (dx=0, dy=1)
  g.p1Cards[0] = 4; // Crab: (0, 1), (-2, 0), (2, 0)

  onitama::Move m = {12, 7, 0};
  assert(onitama::isLegalMove(g, m));
  const bool ok = onitama::applyMove(g, m);
  assert(ok);
  assert(g.isOver);
  assert(g.winner == onitama::kPlayer1);
  assert(g.winReason == onitama::WinReason::Stone);

  std::printf("test_stone_win passed\n");
}

void test_stream_win() {
  onitama::Game g = onitama::initGame(42);
  // Place P1 Master 1 step away from P2 Temple (2, 0)
  g.cells[7] = onitama::kP1Master; // (2, 1)
  g.cells[onitama::kP2Temple] = onitama::kEmpty; // Temple is vacant
  g.p1Cards[0] = 4; // Crab: (0, 1)

  onitama::Move m = {7, onitama::kP2Temple, 0};
  assert(onitama::isLegalMove(g, m));
  const bool ok = onitama::applyMove(g, m);
  assert(ok);
  assert(g.isOver);
  assert(g.winner == onitama::kPlayer1);
  assert(g.winReason == onitama::WinReason::Stream);

  std::printf("test_stream_win passed\n");
}

void test_ai_move() {
  onitama::Game g = onitama::initGame(42);
  const onitama::Move m = onitama::chooseMove(g, onitama::Difficulty::Casual);
  assert(onitama::isLegalMove(g, m));

  const bool ok = onitama::applyMove(g, m);
  assert(ok);
  assert(g.turn == onitama::kPlayer2);

  // P2 (AI) chooses move
  const onitama::Move aiMove = onitama::chooseMove(g, onitama::Difficulty::Casual);
  assert(onitama::isLegalMove(g, aiMove));

  std::printf("test_ai_move passed\n");
}

void test_geometry() {
  // Center cell (2, 2) -> index 12
  // Cell size 70, left 65, top 175
  // (2, 2) box is x: 65 + 2*70 = 205..275, y: 175 + 2*70 = 315..385
  assert(onitamaui::cellAt(240, 350, false) == 12);
  assert(onitamaui::cellAt(240, 350, true) == 12); // Flipped center is still center

  // Cell (0, 0) -> index 0 (top-left)
  assert(onitamaui::cellAt(70, 180, false) == 0);
  assert(onitamaui::cellAt(70, 180, true) == 24); // Flipped top-left is bottom-right

  // Cards hit testing
  assert(onitamaui::cardAt(50, 560) == 0);
  assert(onitamaui::cardAt(200, 560) == 1);
  assert(onitamaui::cardAt(350, 560) == -1); // Next card

  std::printf("test_geometry passed\n");
}

int main() {
  test_initial_state();
  test_legal_moves();
  test_card_rotation();
  test_stone_win();
  test_stream_win();
  test_ai_move();
  test_geometry();
  std::printf("test_onitama: all tests passed\n");
  return 0;
}
