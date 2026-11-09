#include <cassert>
#include <cstdio>
#include "../../src/apps_local/tabletop/games/liarsdice/LiarsDiceCore.h"
#include "../../src/apps_local/tabletop/games/liarsdice/LiarsDiceBrain.h"

using namespace liarsdice;

void test_initialization() {
  Game g;
  initGame(g, 2, 12345);
  assert(g.playerCount == 2);
  assert(g.players[0].diceCount == 5);
  assert(g.players[1].diceCount == 5);
  assert(countTotalActiveDice(g) == 10);
  assert(g.phase == Phase::Bidding);
  assert(g.currentBid.count == 0);
  assert(g.activePlayer == 0);

  for (int p = 0; p < 2; ++p) {
    for (int d = 0; d < 5; ++d) {
      assert(g.players[p].dice[d] >= 1 && g.players[p].dice[d] <= 6);
    }
  }
  std::printf("test_initialization passed\n");
}

void test_bidding_rules() {
  Game g;
  initGame(g, 2, 12345);

  // Opening bid
  assert(isValidBid(g, Bid{1, 3, 0}));
  assert(isValidBid(g, Bid{2, 6, 0}));
  assert(!isValidBid(g, Bid{0, 3, 0}));   // count 0 invalid
  assert(!isValidBid(g, Bid{11, 3, 0}));  // > total dice invalid
  assert(!isValidBid(g, Bid{1, 7, 0}));   // face 7 invalid

  // Make bid: 2 x 3s
  bool ok = makeBid(g, Bid{2, 3, 0});
  assert(ok);
  assert(g.currentBid.count == 2);
  assert(g.currentBid.face == 3);
  assert(g.activePlayer == 1);

  // Next bid must escalate
  assert(!isValidBid(g, Bid{2, 3, 1}));  // same bid invalid
  assert(!isValidBid(g, Bid{2, 2, 1}));  // lower face invalid
  assert(!isValidBid(g, Bid{1, 4, 1}));  // lower count invalid
  assert(isValidBid(g, Bid{2, 4, 1}));   // same count, higher face valid
  assert(isValidBid(g, Bid{3, 2, 1}));   // higher count, lower face valid

  // Shifting to 1s (Aces) from 4 x 4s
  g.currentBid = Bid{4, 4, 0};
  assert(!isValidBid(g, Bid{1, 1, 1}));  // ceil(4/2) = 2, so 1 is invalid
  assert(isValidBid(g, Bid{2, 1, 1}));   // 2 x 1s is valid
  assert(isValidBid(g, Bid{3, 1, 1}));   // 3 x 1s is valid

  // Shifting from 1s to regular numbers
  g.currentBid = Bid{2, 1, 0};
  assert(!isValidBid(g, Bid{4, 4, 1}));  // 2*2 + 1 = 5, so 4 is invalid
  assert(isValidBid(g, Bid{5, 2, 1}));   // 5 x 2s is valid
  assert(isValidBid(g, Bid{3, 1, 1}));   // higher count of 1s is valid

  std::printf("test_bidding_rules passed\n");
}

void test_challenge_and_showdown() {
  Game g;
  initGame(g, 2, 12345);

  // Setup known dice
  // P0: 2, 2, 3, 4, 1  (two 2s, one 1)
  // P1: 2, 5, 6, 1, 3  (one 2, one 1)
  // Total 2s + 1s = 3 + 2 = 5 matching.
  g.players[0].dice[0] = 2;
  g.players[0].dice[1] = 2;
  g.players[0].dice[2] = 3;
  g.players[0].dice[3] = 4;
  g.players[0].dice[4] = 1;

  g.players[1].dice[0] = 2;
  g.players[1].dice[1] = 5;
  g.players[1].dice[2] = 6;
  g.players[1].dice[3] = 1;
  g.players[1].dice[4] = 3;

  assert(countTotalMatching(g, 2) == 5);
  assert(countTotalMatching(g, 1) == 2);  // only 1s count when face is 1

  // Case A: Bid was 4 x 2s by P0, P1 challenges. Actual is 5.
  // Bid was safe! Challenger (P1) loses 1 die.
  g.currentBid = Bid{4, 2, 0};
  g.activePlayer = 1;
  challenge(g);
  assert(!g.challengeSuccessful);
  assert(g.roundLoser == 1);
  assert(g.players[1].diceCount == 4);
  assert(g.phase == Phase::Showdown);

  // Start new round
  startNewRound(g, 1111);
  assert(g.phase == Phase::Bidding);
  assert(g.activePlayer == 1);  // Loser starts
  assert(g.currentBid.count == 0);

  // Case B: Bid was 7 x 2s by P1, P0 challenges. Actual is at most 9 dice total.
  // Let's set dice count: P1 has 1 die left, bids 5 x 2s.
  g.players[0].diceCount = 1;
  g.players[1].diceCount = 1;
  g.players[0].dice[0] = 3;
  g.players[1].dice[0] = 4;
  g.currentBid = Bid{2, 2, 1};  // P1 bids 2 x 2s
  g.activePlayer = 0;           // P0 challenges
  challenge(g);
  assert(g.challengeSuccessful);  // Actual is 0 matching
  assert(g.roundLoser == 1);
  assert(g.players[1].diceCount == 0);
  assert(g.phase == Phase::GameOver);
  assert(g.winner == 0);

  std::printf("test_challenge_and_showdown passed\n");
}

void test_bot_ai() {
  Game g;
  initGame(g, 2, 54321);

  // Opening bid decision
  BotDecision dec = decideBotMove(g, 1, 999);
  assert(dec.action == BotActionType::Bid);
  assert(dec.bid.count >= 1);
  assert(dec.bid.face >= 1 && dec.bid.face <= 6);

  // Faced with impossible bid (10 x 6s)
  g.currentBid = Bid{10, 6, 0};
  g.activePlayer = 1;
  BotDecision chalDec = decideBotMove(g, 1, 999);
  assert(chalDec.action == BotActionType::Challenge);

  std::printf("test_bot_ai passed\n");
}

int main() {
  test_initialization();
  test_bidding_rules();
  test_challenge_and_showdown();
  test_bot_ai();
  std::printf("test_liarsdice: all tests passed\n");
  return 0;
}
