#pragma once

#include "LiarsDiceCore.h"

namespace liarsdice {

enum class BotActionType : uint8_t {
  Bid = 0,
  Challenge
};

struct BotDecision {
  BotActionType action = BotActionType::Bid;
  Bid bid{};
};

double calculateBidProbability(const Game& game, uint8_t botPlayerIdx, const Bid& bid);

BotDecision decideBotMove(const Game& game, uint8_t botPlayerIdx, uint32_t seed = 0);

}  // namespace liarsdice
