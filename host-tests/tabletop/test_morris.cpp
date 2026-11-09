#include <cassert>
#include <cstdio>

#include "games/morris/MorrisBrain.h"
#include "games/morris/MorrisCore.h"
#include "games/morris/MorrisScreens.h"

namespace {

void testStart() {
  using namespace morris;
  Game g;
  start(g);

  assert(!g.isOver);
  assert(!g.pendingRemoval);
  assert(g.turn == kWhite);
  assert(g.pieces[kWhite] == 0);
  assert(g.pieces[kBlack] == 0);
  assert(g.placed[kWhite] == 0);
  assert(g.placed[kBlack] == 0);
  assert(playerPhase(g, kWhite) == Phase::Placing);
  assert(playerPhase(g, kBlack) == Phase::Placing);
  assert(hasLegalMoves(g, kWhite));

  for (int v = 0; v < 24; ++v) {
    assert(pieceAt(g, v) == kNone);
  }
}

void testPlacementAndMills() {
  using namespace morris;
  Game g;
  start(g);

  // White: 0
  assert(placePiece(g, 0));
  assert(g.placed[kWhite] == 1);
  assert(g.turn == kBlack);
  assert(!g.pendingRemoval);

  // Black: 8
  assert(placePiece(g, 8));
  assert(g.placed[kBlack] == 1);
  assert(g.turn == kWhite);

  // White: 1
  assert(placePiece(g, 1));
  assert(g.turn == kBlack);

  // Black: 10
  assert(placePiece(g, 10));
  assert(g.turn == kWhite);

  // White: 2 -> forms outer top mill {0, 1, 2}!
  assert(placePiece(g, 2));
  assert(g.pendingRemoval);
  assert(isPartOfMill(g.pieces[kWhite], 0));
  assert(isPartOfMill(g.pieces[kWhite], 1));
  assert(isPartOfMill(g.pieces[kWhite], 2));
  assert(g.turn == kWhite);  // Turn stays White to remove an enemy piece
}

void testRemoval() {
  using namespace morris;
  Game g;
  start(g);

  // Create situation where White formed mill {0, 1, 2}
  g.pieces[kWhite] = (1U << 0) | (1U << 1) | (1U << 2);
  g.pieces[kBlack] = (1U << 8) | (1U << 10);
  g.placed[kWhite] = 3;
  g.placed[kBlack] = 2;
  g.turn = kWhite;
  g.pendingRemoval = true;

  // Cannot place or move while removal pending
  assert(!placePiece(g, 5));
  assert(!movePiece(g, 0, 7));

  // Cannot remove empty or friendly piece
  assert(!removePiece(g, 5));
  assert(!removePiece(g, 0));

  // Can remove enemy piece at 8
  assert(canRemovePiece(g, kBlack, 8));
  assert(removePiece(g, 8));
  assert(!g.pendingRemoval);
  assert(pieceAt(g, 8) == kNone);
  assert(pieceCount(g.pieces[kBlack]) == 1);
  assert(g.turn == kBlack);  // Now Black's turn
}

void testCannotRemoveMillPieceUnlessAllInMills() {
  using namespace morris;
  Game g;
  start(g);

  // Black has mill {8, 9, 10} AND an un-milled piece at 12
  g.pieces[kBlack] = (1U << 8) | (1U << 9) | (1U << 10) | (1U << 12);
  g.turn = kWhite;
  g.pendingRemoval = true;

  // Piece 8 is in a mill, and 12 is NOT in a mill. Cannot remove 8!
  assert(!canRemovePiece(g, kBlack, 8));
  assert(!canRemovePiece(g, kBlack, 9));
  // Piece 12 is NOT in a mill. CAN remove 12!
  assert(canRemovePiece(g, kBlack, 12));

  // If piece 12 is removed, then ALL Black pieces {8, 9, 10} are in mills.
  // Now pieces in mills CAN be removed!
  g.pieces[kBlack] = (1U << 8) | (1U << 9) | (1U << 10);
  assert(canRemovePiece(g, kBlack, 8));
  assert(canRemovePiece(g, kBlack, 9));
  assert(canRemovePiece(g, kBlack, 10));
}

void testMovementAndFlying() {
  using namespace morris;
  Game g;
  start(g);

  // 9 men placed each
  g.placed[kWhite] = 9;
  g.placed[kBlack] = 9;
  g.pieces[kWhite] = (1U << 0) | (1U << 4) | (1U << 8) | (1U << 12);
  g.pieces[kBlack] = (1U << 2) | (1U << 6) | (1U << 10) | (1U << 14);
  g.turn = kWhite;

  assert(playerPhase(g, kWhite) == Phase::Moving);

  // Legal slide: from 0 to adjacent empty 1
  assert(movePiece(g, 0, 1));
  assert(pieceAt(g, 0) == kNone);
  assert(pieceAt(g, 1) == kWhite);
  assert(g.turn == kBlack);

  // Reduce White to 3 pieces -> Flying phase!
  g.pieces[kWhite] = (1U << 1) | (1U << 4) | (1U << 8);
  assert(playerPhase(g, kWhite) == Phase::Flying);
  g.turn = kWhite;

  // Flying jump: from 1 to non-adjacent empty 20!
  assert(movePiece(g, 1, 20));
  assert(pieceAt(g, 1) == kNone);
  assert(pieceAt(g, 20) == kWhite);
}

void testGameOver() {
  using namespace morris;
  Game g;
  start(g);

  // 9 placed each, Black reduced to 2 pieces after removal
  g.placed[kWhite] = 9;
  g.placed[kBlack] = 9;
  g.pieces[kWhite] = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3);
  g.pieces[kBlack] = (1U << 8) | (1U << 10) | (1U << 12);
  g.turn = kWhite;
  g.pendingRemoval = true;

  // White removes piece 12 -> Black left with only 2 pieces!
  assert(removePiece(g, 12));
  assert(g.isOver);
  assert(outcome(g) == Outcome::WhiteWon);
}

void testAi() {
  using namespace morris;
  Game g;
  start(g);

  // In placing phase
  BestMove bPlace = findBestMove(g, kWhite, Difficulty::Standard);
  assert(bPlace.kind == BestMove::Place);
  assert(bPlace.to >= 0 && bPlace.to < 24);

  // In removal phase
  g.pendingRemoval = true;
  g.pieces[kBlack] = (1U << 5) | (1U << 11);
  BestMove bRem = findBestMove(g, kWhite, Difficulty::Standard);
  assert(bRem.kind == BestMove::Remove);
  assert(bRem.to == 5 || bRem.to == 11);
}

void testGeometry() {
  using namespace morrisui;
  const auto board = boardRect();
  assert(board.width == 448);
  assert(board.height == 448);

  // Out of bounds
  assert(vertexAt(0, 0) == -1);

  // Vertex 0 at (40, 168)
  assert(vertexAt(40, 168) == 0);
  assert(vertexAt(44, 165) == 0);

  // Center radial vertex 9 at (240, 232)
  assert(vertexAt(240, 232) == 9);

  // Center hole has no vertex
  assert(vertexAt(240, 368) == -1);

  // Flipped board: vertex 0 (normally top-left at 40, 168) is mapped to (440, 568)
  assert(vertexAt(440, 568, true) == 0);
  // And (40, 168) on a flipped board maps to vertex 4 (normally bottom-right)
  assert(vertexAt(40, 168, true) == 4);
}

}  // namespace

int main() {
  testStart();
  testPlacementAndMills();
  testRemoval();
  testCannotRemoveMillPieceUnlessAllInMills();
  testMovementAndFlying();
  testGameOver();
  testAi();
  testGeometry();
  std::printf("test_morris: all tests passed\n");
  return 0;
}
