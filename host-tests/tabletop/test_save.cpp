#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/tabletop/TabletopSave.h"

using namespace tabletop::save;
using namespace dobutsu;

void test_save_roundtrip() {
  Save original{};
  original.lastScreen = 1;  // Reversi
  original.reversiMode = 1;
  original.reversiFlip = true;
  original.reversiGame.turn = reversi::kWhite;
  original.reversiGame.black = 0x0000000810000000ULL;
  original.reversiGame.white = 0x0000001008000000ULL;
  original.reversiGame.isOver = false;

  original.mancalaMode = 1;
  original.mancalaFlip = true;
  original.mancalaGame.turn = mancala::kPlayer2;
  original.mancalaGame.extraTurn = true;
  for (int i = 0; i < 14; ++i) {
    original.mancalaGame.pits[i] = static_cast<uint8_t>(i + 1);
  }

  original.morrisMode = 1;
  original.morrisFlip = true;
  original.morrisGame.turn = morris::kBlack;
  original.morrisGame.pendingRemoval = true;
  original.morrisGame.placed[morris::kWhite] = 2;
  original.morrisGame.placed[morris::kBlack] = 3;
  original.morrisGame.pieces[morris::kWhite] = (1U << 0) | (1U << 1) | (1U << 2);
  original.morrisGame.pieces[morris::kBlack] = (1U << 9) | (1U << 10);
  original.morrisGame.selected = 9;

  original.onitamaMode = 1;
  original.onitamaFlip = true;
  original.onitamaGame = onitama::initGame(99);
  original.onitamaGame.turn = onitama::kPlayer2;
  original.onitamaGame.cells[12] = onitama::kP1Master;

  original.liarsDiceMode = 1;
  liarsdice::initGame(original.liarsDiceGame, 2, 777);
  original.liarsDiceGame.currentBid = liarsdice::Bid{3, 4, 0};
  original.liarsDiceGame.phase = liarsdice::Phase::Bidding;

  original.dobutsuMode = 1;
  original.dobutsuFlip = true;
  initGame(original.dobutsuGame);
  original.dobutsuGame.turn = Player::P2;
  original.dobutsuGame.reserves[0][reserveIndex(PieceType::Chick)] = 1;
  original.dobutsuGame.lionTryPlayer = 1;

  original.backgammonMode = 1;
  original.backgammonFlip = true;
  backgammon::initGame(original.backgammonGame);
  original.backgammonGame.turn = backgammon::Player::Black;
  original.backgammonGame.isRolled = true;
  original.backgammonGame.dice[0] = 5;
  original.backgammonGame.dice[1] = 2;
  original.backgammonGame.movesLeft[0] = 5;
  original.backgammonGame.movesLeft[1] = 2;
  original.backgammonGame.moveCount = 2;
  original.backgammonGame.bar[0] = 1;
  original.backgammonGame.borneOff[1] = 3;

  santorini::initGame(original.santoriniState);
  original.santoriniState.turn = santorini::Player::Black;
  original.santoriniState.flip = true;
  original.santoriniState.board[12] = 2;
  original.santoriniState.workers[0][0] = 6;

  quoridor::initGame(original.quoridorState);
  original.quoridorState.turn = quoridor::Player::Black;
  original.quoridorState.pawn[0] = 15;
  original.quoridorState.wallsLeft[0] = 7;
  original.quoridorState.hWalls = (1ULL << 10);
  original.quoridorState.selectedGx = 2;
  original.quoridorState.selectedGy = 3;

  dominoes::initGame(original.dominoesState, 1234);
  original.dominoesState.turn = dominoes::Player::P2;
  original.dominoesState.trainCount = 1;
  original.dominoesState.train[0] = {6, {6, 6}};
  original.dominoesState.leftEnd = 6;
  original.dominoesState.rightEnd = 6;
  original.dominoesState.score[0] = 12;

  original.sandboxState = sandbox::makeDefault(0x12345678);
  original.sandboxState.mode = sandbox::ToolMode::Cards;
  original.sandboxState.handHidden = true;
  original.sandboxState.scores[0] = 19;
  original.sandboxState.scores[1] = -5;
  original.sandboxState.tokenCount = 2;
  original.sandboxState.tokens[0] = {12, 0};
  original.sandboxState.tokens[1] = {45, 1};

  char buffer[kMaxLine] = {};
  const int bytes = pack(original, buffer, sizeof(buffer));
  assert(bytes > 0);
  assert(bytes < kMaxLine);

  Save loaded{};
  const bool ok = unpack(buffer, loaded);
  assert(ok);

  assert(loaded.lastScreen == original.lastScreen);
  assert(loaded.reversiMode == original.reversiMode);
  assert(loaded.reversiFlip == original.reversiFlip);
  assert(loaded.reversiGame.turn == original.reversiGame.turn);
  assert(loaded.reversiGame.black == original.reversiGame.black);
  assert(loaded.reversiGame.white == original.reversiGame.white);
  assert(loaded.reversiGame.isOver == original.reversiGame.isOver);

  assert(loaded.mancalaMode == original.mancalaMode);
  assert(loaded.mancalaFlip == original.mancalaFlip);
  assert(loaded.mancalaGame.turn == original.mancalaGame.turn);
  assert(loaded.mancalaGame.extraTurn == original.mancalaGame.extraTurn);
  for (int i = 0; i < 14; ++i) {
    assert(loaded.mancalaGame.pits[i] == original.mancalaGame.pits[i]);
  }

  assert(loaded.morrisMode == original.morrisMode);
  assert(loaded.morrisFlip == original.morrisFlip);
  assert(loaded.morrisGame.turn == original.morrisGame.turn);
  assert(loaded.morrisGame.pendingRemoval == original.morrisGame.pendingRemoval);
  assert(loaded.morrisGame.placed[morris::kWhite] == original.morrisGame.placed[morris::kWhite]);
  assert(loaded.morrisGame.placed[morris::kBlack] == original.morrisGame.placed[morris::kBlack]);
  assert(loaded.morrisGame.pieces[morris::kWhite] == original.morrisGame.pieces[morris::kWhite]);
  assert(loaded.morrisGame.pieces[morris::kBlack] == original.morrisGame.pieces[morris::kBlack]);
  assert(loaded.morrisGame.selected == original.morrisGame.selected);

  assert(loaded.onitamaMode == original.onitamaMode);
  assert(loaded.onitamaFlip == original.onitamaFlip);
  assert(loaded.onitamaGame.turn == original.onitamaGame.turn);
  assert(loaded.onitamaGame.cells[12] == onitama::kP1Master);
  assert(loaded.onitamaGame.tableCard == original.onitamaGame.tableCard);

  assert(loaded.liarsDiceMode == original.liarsDiceMode);
  assert(loaded.liarsDiceGame.currentBid.count == 3);
  assert(loaded.liarsDiceGame.currentBid.face == 4);

  assert(loaded.dobutsuMode == original.dobutsuMode);
  assert(loaded.dobutsuFlip == original.dobutsuFlip);
  assert(loaded.dobutsuGame.turn == Player::P2);
  assert(loaded.dobutsuGame.reserves[0][reserveIndex(PieceType::Chick)] == 1);
  assert(loaded.dobutsuGame.lionTryPlayer == 1);

  assert(loaded.backgammonMode == original.backgammonMode);
  assert(loaded.backgammonFlip == original.backgammonFlip);
  assert(loaded.backgammonGame.turn == backgammon::Player::Black);
  assert(loaded.backgammonGame.isRolled == true);
  assert(loaded.backgammonGame.dice[0] == 5 && loaded.backgammonGame.dice[1] == 2);
  assert(loaded.backgammonGame.moveCount == 2);
  assert(loaded.backgammonGame.bar[0] == 1);
  assert(loaded.backgammonGame.borneOff[1] == 3);
  assert(loaded.backgammonGame.board[23] == 2);

  assert(loaded.santoriniState.turn == santorini::Player::Black);
  assert(loaded.santoriniState.flip == true);
  assert(loaded.santoriniState.board[12] == 2);
  assert(loaded.santoriniState.workers[0][0] == 6);

  assert(loaded.quoridorState.turn == quoridor::Player::Black);
  assert(loaded.quoridorState.pawn[0] == 15);
  assert(loaded.quoridorState.wallsLeft[0] == 7);
  assert(loaded.quoridorState.hWalls == (1ULL << 10));
  assert(loaded.quoridorState.selectedGx == 2);
  assert(loaded.quoridorState.selectedGy == 3);

  assert(loaded.dominoesState.turn == dominoes::Player::P2);
  assert(loaded.dominoesState.trainCount == 1);
  assert(loaded.dominoesState.train[0].tileIdx == 6);
  assert(loaded.dominoesState.train[0].pips[0] == 6);
  assert(loaded.dominoesState.train[0].pips[1] == 6);
  assert(loaded.dominoesState.leftEnd == 6);
  assert(loaded.dominoesState.rightEnd == 6);
  assert(loaded.dominoesState.score[0] == 12);

  assert(loaded.sandboxState.mode == original.sandboxState.mode);
  assert(loaded.sandboxState.handHidden == original.sandboxState.handHidden);
  assert(loaded.sandboxState.scores[0] == 19);
  assert(loaded.sandboxState.scores[1] == -5);
  assert(loaded.sandboxState.tokenCount == 2);
  assert(loaded.sandboxState.tokens[0].pos == 12);
  assert(loaded.sandboxState.tokens[0].color == 0);
  assert(loaded.sandboxState.tokens[1].pos == 45);
  assert(loaded.sandboxState.tokens[1].color == 1);

  // Rejection tests
  assert(!unpack(nullptr, loaded));
  assert(!unpack("TT 99 0\n", loaded));       // Wrong version
  assert(!unpack("BAD MAGIC 1 0\n", loaded));  // Bad magic
  assert(!unpack("TT 1 0\nREV 0\n", loaded));  // Truncated line

  std::printf("test_save_roundtrip passed\n");
}

int main() {
  test_save_roundtrip();
  std::printf("test_save: all tests passed\n");
  return 0;
}
