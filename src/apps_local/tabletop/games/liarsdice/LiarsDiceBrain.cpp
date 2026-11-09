#include "LiarsDiceBrain.h"

#include <cmath>

namespace liarsdice {

namespace {

uint64_t nChooseK(uint8_t n, uint8_t k) {
  if (k > n) return 0;
  if (k == 0 || k == n) return 1;
  if (k > n - k) k = n - k;
  uint64_t res = 1;
  for (uint8_t i = 1; i <= k; ++i) {
    res = res * (n - k + i) / i;
  }
  return res;
}

uint8_t countBotMatching(const Player& bot, uint8_t face) {
  uint8_t matching = 0;
  for (uint8_t d = 0; d < bot.diceCount; ++d) {
    const uint8_t val = bot.dice[d];
    if (face == 1) {
      if (val == 1) matching++;
    } else {
      if (val == face || val == 1) matching++;
    }
  }
  return matching;
}

}  // namespace

double calculateBidProbability(const Game& game, uint8_t botPlayerIdx, const Bid& bid) {
  if (botPlayerIdx >= game.playerCount) return 0.0;
  if (bid.face < 1 || bid.face > 6 || bid.count == 0) return 0.0;

  const Player& bot = game.players[botPlayerIdx];
  const uint8_t known = countBotMatching(bot, bid.face);

  if (bid.count <= known) {
    return 1.0;
  }

  const uint8_t totalDice = countTotalActiveDice(game);
  if (totalDice <= bot.diceCount) {
    return 0.0;
  }

  const uint8_t unknownDice = static_cast<uint8_t>(totalDice - bot.diceCount);
  const uint8_t needed = static_cast<uint8_t>(bid.count - known);

  if (needed > unknownDice) {
    return 0.0;
  }

  const double p = (bid.face == 1) ? (1.0 / 6.0) : (1.0 / 3.0);
  const double q = 1.0 - p;

  double totalProb = 0.0;
  for (uint8_t j = needed; j <= unknownDice; ++j) {
    const double comb = static_cast<double>(nChooseK(unknownDice, j));
    totalProb += comb * std::pow(p, j) * std::pow(q, unknownDice - j);
  }

  return totalProb;
}

BotDecision decideBotMove(const Game& game, uint8_t botPlayerIdx, uint32_t seed) {
  BotDecision decision;

  // Opening bid of round
  if (game.currentBid.count == 0) {
    const Player& bot = game.players[botPlayerIdx];
    // Find bot's most frequent face (2..6)
    uint8_t faceFreq[7] = {0};
    for (uint8_t d = 0; d < bot.diceCount; ++d) {
      faceFreq[bot.dice[d]]++;
    }

    uint8_t bestFace = 2;
    uint8_t maxCount = 0;
    for (uint8_t f = 2; f <= 6; ++f) {
      const uint8_t totalForFace = faceFreq[f] + faceFreq[1];
      if (totalForFace > maxCount) {
        maxCount = totalForFace;
        bestFace = f;
      }
    }

    // Conservative opening bid: expected matching dice
    const uint8_t totalDice = countTotalActiveDice(game);
    uint8_t openCount = static_cast<uint8_t>((totalDice / 3) + (maxCount > 2 ? 1 : 0));
    if (openCount < 1) openCount = 1;

    decision.action = BotActionType::Bid;
    decision.bid = Bid{openCount, bestFace, botPlayerIdx};
    return decision;
  }

  // Evaluate current bid probability
  const double curProb = calculateBidProbability(game, botPlayerIdx, game.currentBid);

  // If probability is very low, challenge "LIAR!"
  // Slightly adjust threshold with seed for natural bluff variance
  const double bluffThreshold = 0.28 + ((seed % 10) * 0.01 - 0.05);  // 0.23 .. 0.32
  if (curProb < bluffThreshold) {
    decision.action = BotActionType::Challenge;
    return decision;
  }

  // Generate candidate valid bids and score their probabilities
  Bid bestCandidate{};
  double bestProb = -1.0;

  const Bid& cur = game.currentBid;
  const uint8_t totalDice = countTotalActiveDice(game);

  // Check candidate faces 2..6 and 1
  for (uint8_t f = 1; f <= 6; ++f) {
    for (uint8_t c = cur.count; c <= cur.count + 2 && c <= totalDice; ++c) {
      Bid cand{c, f, botPlayerIdx};
      if (isValidBid(game, cand)) {
        const double p = calculateBidProbability(game, botPlayerIdx, cand);
        // Prefer bids that are probable (> 0.32) and minimize count escalation
        const double score = p - (c - cur.count) * 0.12;
        if (score > bestProb) {
          bestProb = score;
          bestCandidate = cand;
        }
      }
    }
  }

  // If best candidate is plausible (prob >= 0.22), bid it!
  if (bestProb > 0.0 && calculateBidProbability(game, botPlayerIdx, bestCandidate) >= 0.20) {
    decision.action = BotActionType::Bid;
    decision.bid = bestCandidate;
    return decision;
  }

  // Otherwise, all bids are suicidal -> challenge!
  decision.action = BotActionType::Challenge;
  return decision;
}

}  // namespace liarsdice
