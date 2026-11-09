#include <cassert>
#include <cstdio>

#include "../../src/apps_local/tabletop/games/dominoes/DominoesBrain.h"
#include "../../src/apps_local/tabletop/games/dominoes/DominoesCore.h"
#include "../../src/apps_local/tabletop/games/dominoes/DominoesScreens.h"

void test_initialization() {
  dominoes::State state;
  dominoes::initGame(state, 12345);

  assert(state.trainCount == 1);
  assert(state.leftEnd >= 0 && state.leftEnd <= 6);
  assert(state.rightEnd >= 0 && state.rightEnd <= 6);
  assert(state.boneyardCount == 14);

  // Opener played 1 tile, so one player has 6 and the other has 7
  int totalHand = state.handCount[0] + state.handCount[1];
  assert(totalHand == 13);
  assert(state.handCount[0] == 6 || state.handCount[0] == 7);
  assert(state.handCount[1] == 6 || state.handCount[1] == 7);

  printf("test_initialization passed\n");
}

void test_legal_plays() {
  dominoes::State state;
  dominoes::initGame(state, 12345);

  // Set predictable ends: leftEnd = 3, rightEnd = 5
  state.leftEnd = 3;
  state.rightEnd = 5;
  state.turn = dominoes::Player::P1;

  // Put specific tile in P1's hand: [3|1] is index 9
  state.hand[0][0] = 9;  // {1, 3}
  state.handCount[0] = 1;

  // Can play Left (3 matches leftEnd 3)
  assert(dominoes::canPlayLeft(state, 9));
  // Cannot play Right (1 and 3 don't match 5)
  assert(!dominoes::canPlayRight(state, 9));

  // Play Left
  int prevTrain = state.trainCount;
  assert(dominoes::playTile(state, 0, dominoes::PlaySide::Left));

  // Train count incremented, new leftEnd is 1!
  assert(state.trainCount == prevTrain + 1);
  assert(state.leftEnd == 1);
  assert(state.train[0].pips[0] == 1 && state.train[0].pips[1] == 3);

  // P1 played their last tile -> Domino win!
  assert(state.roundOver);
  assert(state.winner == dominoes::Player::P1);

  printf("test_legal_plays passed\n");
}

void test_boneyard_drawing() {
  dominoes::State state;
  dominoes::initGame(state, 12345);
  state.roundOver = false;

  int prevBone = state.boneyardCount;
  int prevHand = state.handCount[static_cast<int>(state.turn)];

  assert(dominoes::drawFromBoneyard(state));
  assert(state.boneyardCount == prevBone - 1);
  assert(state.handCount[static_cast<int>(state.turn)] == prevHand + 1);

  printf("test_boneyard_drawing passed\n");
}

void test_blocked_game() {
  dominoes::State state;
  dominoes::initGame(state, 12345);
  state.roundOver = false;

  // Empty boneyard
  state.boneyardCount = 0;

  // Set ends to 6 and 6
  state.leftEnd = 6;
  state.rightEnd = 6;

  // Give P1 only {0, 0} (index 0, pip sum 0)
  state.hand[0][0] = 0;
  state.handCount[0] = 1;

  // Give P2 only {1, 1} (index 7, pip sum 2)
  state.hand[1][0] = 7;
  state.handCount[1] = 1;

  state.turn = dominoes::Player::P1;

  // Neither player can play
  assert(!dominoes::hasAnyLegalPlay(state, dominoes::Player::P1));
  assert(!dominoes::hasAnyLegalPlay(state, dominoes::Player::P2));

  // P1 passes
  assert(dominoes::passTurn(state));
  assert(!state.roundOver);
  assert(state.turn == dominoes::Player::P2);

  // P2 passes -> Game blocked!
  assert(dominoes::passTurn(state));
  assert(state.roundOver);
  // P1 has 0 pips, P2 has 2 pips -> P1 wins the difference (2 pts)!
  assert(state.winner == dominoes::Player::P1);
  assert(state.score[0] == 2);

  printf("test_blocked_game passed\n");
}

void test_bot_ai() {
  dominoes::State state;
  dominoes::initGame(state, 12345);
  state.roundOver = false;
  state.turn = dominoes::Player::P2;

  // Give P2 a playable tile matching rightEnd
  // If rightEnd is R, give tile {R, 2}
  int r = state.rightEnd;
  int8_t tileIdx = -1;
  for (int i = 0; i < dominoes::kTotalTiles; ++i) {
    if (dominoes::kTiles[i].pips[0] == r || dominoes::kTiles[i].pips[1] == r) {
      tileIdx = static_cast<int8_t>(i);
      break;
    }
  }
  assert(tileIdx != -1);
  state.hand[1][0] = tileIdx;
  state.handCount[1] = 1;

  dominoes::Move move;
  assert(dominoes::DominoesBrain::findBestMove(state, move));
  assert(move.handIdx == 0);

  // Execute AI move
  assert(dominoes::playTile(state, move.handIdx, move.side));

  printf("test_bot_ai passed\n");
}

void test_geometry() {
  // Hand area Y = 410, 7 tiles
  // Width = 48, spacing = 54
  int startX = (480 - 7 * 54) / 2;  // 51

  assert(dominoesui::handIdxAt(startX + 10, 420, 7) == 0);
  assert(dominoesui::handIdxAt(startX + 54 + 10, 420, 7) == 1);
  assert(dominoesui::handIdxAt(startX + 6 * 54 + 10, 420, 7) == 6);

  // Tap in gap between tiles
  assert(dominoesui::handIdxAt(startX + 50, 420, 7) == -1);

  // Tap outside bounds
  assert(dominoesui::handIdxAt(10, 10, 7) == -1);

  printf("test_geometry passed\n");
}

int main() {
  test_initialization();
  test_legal_plays();
  test_boneyard_drawing();
  test_blocked_game();
  test_bot_ai();
  test_geometry();
  printf("test_dominoes: all tests passed\n");
  return 0;
}
