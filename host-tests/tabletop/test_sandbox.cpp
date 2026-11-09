#include <cassert>
#include <cstdio>
#include <cstring>

#include "sandbox/SandboxCore.h"
#include "sandbox/SandboxScreens.h"

static void test_initial_state() {
  const sandbox::SandboxState s = sandbox::makeDefault(0xCAFEBABE);
  assert(s.mode == sandbox::ToolMode::Dice);
  assert(s.diceCount == 2);
  assert(s.dieType == sandbox::DieType::D6);
  assert(s.diceValues[0] >= 1 && s.diceValues[0] <= 6);
  assert(s.diceValues[1] >= 1 && s.diceValues[1] <= 6);
  assert(s.deckCount == 52);
  assert(s.handCount == 0);
  assert(s.discardTop == cards::kNoCard);
  for (int i = 0; i < 4; ++i) {
    assert(s.scores[i] == 20);
  }
  assert(s.activePlayer == 0);
  assert(s.tokenCount == 0);
  assert(s.selectedToken == -1);
  std::printf("test_initial_state passed\n");
}

static void test_dice_subsystem() {
  sandbox::SandboxState s = sandbox::makeDefault(0x12345678);

  // Test rolling dice
  sandbox::rollDice(s);
  assert(s.diceValues[0] >= 1 && s.diceValues[0] <= 6);
  assert(s.diceValues[1] >= 1 && s.diceValues[1] <= 6);
  assert(s.lastRollSum == s.diceValues[0] + s.diceValues[1]);
  assert(s.rollHistory[0] == s.lastRollSum);

  // Test holding a die
  sandbox::toggleHoldDie(s, 0);
  assert(s.heldMask == 1);
  const uint8_t heldVal = s.diceValues[0];
  for (int i = 0; i < 10; ++i) {
    sandbox::rollDice(s);
    assert(s.diceValues[0] == heldVal);  // held value unchanged
  }
  sandbox::toggleHoldDie(s, 0);
  assert(s.heldMask == 0);

  // Test add / remove die
  while (s.diceCount < sandbox::kMaxDice) {
    const uint8_t prev = s.diceCount;
    sandbox::addDie(s);
    assert(s.diceCount == prev + 1);
  }
  sandbox::addDie(s);  // at max, should not exceed
  assert(s.diceCount == sandbox::kMaxDice);

  while (s.diceCount > 1) {
    const uint8_t prev = s.diceCount;
    sandbox::removeDie(s);
    assert(s.diceCount == prev - 1);
  }
  sandbox::removeDie(s);  // at min, should not decrease
  assert(s.diceCount == 1);

  // Test cycling die type
  assert(s.dieType == sandbox::DieType::D6);
  sandbox::cycleDieType(s);
  assert(s.dieType == sandbox::DieType::D8);
  sandbox::cycleDieType(s);
  assert(s.dieType == sandbox::DieType::D10);
  sandbox::cycleDieType(s);
  assert(s.dieType == sandbox::DieType::D12);
  sandbox::cycleDieType(s);
  assert(s.dieType == sandbox::DieType::D20);
  sandbox::cycleDieType(s);
  assert(s.dieType == sandbox::DieType::D4);
  sandbox::cycleDieType(s);
  assert(s.dieType == sandbox::DieType::D6);

  std::printf("test_dice_subsystem passed\n");
}

static void test_cards_subsystem() {
  sandbox::SandboxState s = sandbox::makeDefault(0x87654321);
  assert(s.deckCount == 52);

  // Verify all 52 cards are distinct standard cards
  uint64_t seen = 0;
  for (int i = 0; i < 52; ++i) {
    const uint8_t c = s.deck[i];
    const int r = cards::rankOf(c);
    const int suit = static_cast<int>(cards::suitOf(c));
    assert(r >= 0 && r < cards::kRanks);
    assert(suit >= 0 && suit < cards::kSuits);
    const int id = suit * 13 + r;
    assert((seen & (1ULL << id)) == 0);
    seen |= (1ULL << id);
  }
  const uint64_t expectedMask = (1ULL << 52) - 1ULL;
  assert(seen == expectedMask);

  // Draw 5 cards
  for (int i = 0; i < 5; ++i) {
    assert(sandbox::drawCard(s));
    assert(s.handCount == i + 1);
    assert(s.deckCount == 52 - (i + 1));
  }
  // 6th draw should fail (hand limit 5)
  assert(!sandbox::drawCard(s));
  assert(s.handCount == 5);

  // Discard card at index 2
  const uint8_t discarded = s.hand[2];
  assert(sandbox::discardCard(s, 2));
  assert(s.handCount == 4);
  assert(cards::faceUp(discarded) == s.discardTop);

  // Discard remaining hand
  sandbox::discardHand(s);
  assert(s.handCount == 0);

  // Reset deck
  sandbox::initDeck(s);
  assert(s.deckCount == 52);
  assert(s.handCount == 0);
  assert(s.discardTop == cards::kNoCard);
  assert(!s.handHidden);

  // Toggle hand hidden
  sandbox::toggleHandHidden(s);
  assert(s.handHidden);
  sandbox::toggleHandHidden(s);
  assert(!s.handHidden);

  std::printf("test_cards_subsystem passed\n");
}

static void test_counters_subsystem() {
  sandbox::SandboxState s = sandbox::makeDefault(0x99999999);
  assert(s.scores[0] == 20);

  sandbox::changeScore(s, 0, 5);
  assert(s.scores[0] == 25);
  sandbox::changeScore(s, 0, -1);
  assert(s.scores[0] == 24);

  sandbox::nextPlayer(s);
  assert(s.activePlayer == 1);
  sandbox::nextPlayer(s);
  assert(s.activePlayer == 2);
  sandbox::nextPlayer(s);
  assert(s.activePlayer == 3);
  sandbox::nextPlayer(s);
  assert(s.activePlayer == 0);

  sandbox::resetScores(s, 0);
  for (int i = 0; i < 4; ++i) {
    assert(s.scores[i] == 0);
  }

  std::printf("test_counters_subsystem passed\n");
}

static void test_board_subsystem() {
  sandbox::SandboxState s = sandbox::makeDefault(0xAAAAAAAA);
  assert(s.tokenCount == 0);

  // Spawn white at pos 10, black at pos 20
  assert(sandbox::spawnToken(s, 10, 0));
  assert(sandbox::spawnToken(s, 20, 1));
  assert(s.tokenCount == 2);
  assert(sandbox::findTokenAt(s, 10) == 0);
  assert(sandbox::findTokenAt(s, 20) == 1);

  // Cannot spawn at occupied pos
  assert(!sandbox::spawnToken(s, 10, 0));

  // Move token 0 from pos 10 to pos 18
  assert(sandbox::moveToken(s, 0, 18));
  assert(s.tokens[0].pos == 18);
  assert(sandbox::findTokenAt(s, 10) == -1);
  assert(sandbox::findTokenAt(s, 18) == 0);

  // Move token 0 to pos 20 (captures token 1)
  assert(sandbox::moveToken(s, 0, 20));
  assert(s.tokenCount == 1);
  assert(s.tokens[0].pos == 20);

  // Checkers preset
  sandbox::setupCheckersPreset(s);
  assert(s.tokenCount == 24);

  // Clear
  sandbox::clearTokens(s);
  assert(s.tokenCount == 0);

  std::printf("test_board_subsystem passed\n");
}

static void test_geometry() {
  using namespace sandboxui;
  const auto board = boardRect();
  assert(board.width == 400);
  assert(board.height == 400);

  // Out of bounds
  assert(squareAt(0, 0) == -1);
  assert(squareAt(10, 100) == -1);

  // Top-left square (0, 0) -> pos 0
  assert(squareAt(board.x + 2, board.y + 2) == 0);

  // Bottom-right square (7, 7) -> pos 63
  assert(squareAt(board.right() - 2, board.bottom() - 2) == 63);

  // Center squares
  assert(squareAt(board.x + 2 * 50 + 5, board.y + 3 * 50 + 5) == 3 * 8 + 2);

  // Die rects
  const auto d0 = dieRect(0, 2);
  const auto d1 = dieRect(1, 2);
  assert(d0.width == 84 && d0.height == 84);
  assert(d1.width == 84 && d1.height == 84);
  assert(d1.x > d0.right());
  assert(dieAt(d0.x + 5, d0.y + 5, 2) == 0);
  assert(dieAt(d1.x + 5, d1.y + 5, 2) == 1);
  assert(dieAt(0, 0, 2) == -1);

  // Hand card rects
  const auto c0 = handCardRect(0);
  const auto c1 = handCardRect(1);
  assert(c0.width == 80 && c0.height == 114);
  assert(c1.width == 80 && c1.height == 114);
  assert(c1.x > c0.right());
  assert(handCardAt(c0.x + 5, c0.y + 5, 2) == 0);
  assert(handCardAt(c1.x + 5, c1.y + 5, 2) == 1);
  assert(handCardAt(c1.x + 5, c1.y + 5, 1) == -1);

  // Draw pile rect
  const auto dp = drawPileRect();
  assert(dp.width == 92 && dp.height == 122);

  std::printf("test_geometry passed\n");
}

int main() {
  test_initial_state();
  test_dice_subsystem();
  test_cards_subsystem();
  test_counters_subsystem();
  test_board_subsystem();
  test_geometry();
  std::printf("test_sandbox: all tests passed\n");
  return 0;
}
