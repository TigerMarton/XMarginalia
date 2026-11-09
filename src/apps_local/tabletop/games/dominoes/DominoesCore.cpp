#include "DominoesCore.h"

namespace dominoes {

int getHandPipSum(const State& state, Player p) {
  int sum = 0;
  int pIdx = static_cast<int>(p);
  for (int i = 0; i < state.handCount[pIdx]; ++i) {
    int idx = state.hand[pIdx][i];
    sum += kTiles[idx].pips[0] + kTiles[idx].pips[1];
  }
  return sum;
}

void initGame(State& state, uint32_t seed) {
  int8_t deck[kTotalTiles];
  for (int i = 0; i < kTotalTiles; ++i) {
    deck[i] = static_cast<int8_t>(i);
  }

  // Shuffle deck
  uint32_t rng = seed;
  for (int i = kTotalTiles - 1; i > 0; --i) {
    rng = rng * 1664525u + 1013904223u;
    int j = rng % (i + 1);
    int8_t tmp = deck[i];
    deck[i] = deck[j];
    deck[j] = tmp;
  }

  // Deal 7 to P1, 7 to P2
  state.handCount[0] = 0;
  state.handCount[1] = 0;
  int dIdx = 0;

  for (int i = 0; i < kHandSize; ++i) {
    state.hand[0][state.handCount[0]++] = deck[dIdx++];
  }
  for (int i = 0; i < kHandSize; ++i) {
    state.hand[1][state.handCount[1]++] = deck[dIdx++];
  }

  // Remaining 14 tiles to boneyard
  state.boneyardCount = 0;
  while (dIdx < kTotalTiles) {
    state.boneyard[state.boneyardCount++] = deck[dIdx++];
  }

  state.trainCount = 0;
  state.leftEnd = -1;
  state.rightEnd = -1;
  state.winner = Player::None;
  state.mode = Mode::Solo;
  state.privacyConcealed = false;
  state.selectedHandIdx = -1;
  state.roundOver = false;
  state.score[0] = 0;
  state.score[1] = 0;
  state.consecutivePasses = 0;

  // Determine opener: player with highest double, or highest pip sum
  int bestDouble[2] = {-1, -1};
  int bestPip[2] = {-1, -1};
  int bestDoubleHandIdx[2] = {-1, -1};
  int bestPipHandIdx[2] = {-1, -1};

  for (int p = 0; p < 2; ++p) {
    for (int i = 0; i < state.handCount[p]; ++i) {
      int tIdx = state.hand[p][i];
      const auto& t = kTiles[tIdx];
      int sum = t.pips[0] + t.pips[1];
      if (t.pips[0] == t.pips[1] && t.pips[0] > bestDouble[p]) {
        bestDouble[p] = t.pips[0];
        bestDoubleHandIdx[p] = i;
      }
      if (sum > bestPip[p]) {
        bestPip[p] = sum;
        bestPipHandIdx[p] = i;
      }
    }
  }

  Player opener = Player::P1;
  int openerHandIdx = 0;

  if (bestDouble[0] >= 0 || bestDouble[1] >= 0) {
    if (bestDouble[0] > bestDouble[1]) {
      opener = Player::P1;
      openerHandIdx = bestDoubleHandIdx[0];
    } else {
      opener = Player::P2;
      openerHandIdx = bestDoubleHandIdx[1];
    }
  } else {
    if (bestPip[0] >= bestPip[1]) {
      opener = Player::P1;
      openerHandIdx = bestPipHandIdx[0];
    } else {
      opener = Player::P2;
      openerHandIdx = bestPipHandIdx[1];
    }
  }

  // Opener plays opening tile to table center
  int oIdx = static_cast<int>(opener);
  int8_t openTile = state.hand[oIdx][openerHandIdx];
  const auto& t = kTiles[openTile];

  state.train[0] = {openTile, {t.pips[0], t.pips[1]}};
  state.trainCount = 1;
  state.leftEnd = t.pips[0];
  state.rightEnd = t.pips[1];

  // Remove opening tile from hand
  for (int i = openerHandIdx; i < state.handCount[oIdx] - 1; ++i) {
    state.hand[oIdx][i] = state.hand[oIdx][i + 1];
  }
  state.handCount[oIdx]--;

  state.turn = opponent(opener);
}

bool canPlayLeft(const State& state, int8_t tileIdx) {
  if (state.roundOver || tileIdx < 0 || tileIdx >= kTotalTiles) return false;
  const auto& t = kTiles[tileIdx];
  return (t.pips[0] == state.leftEnd || t.pips[1] == state.leftEnd);
}

bool canPlayRight(const State& state, int8_t tileIdx) {
  if (state.roundOver || tileIdx < 0 || tileIdx >= kTotalTiles) return false;
  const auto& t = kTiles[tileIdx];
  return (t.pips[0] == state.rightEnd || t.pips[1] == state.rightEnd);
}

bool hasAnyLegalPlay(const State& state, Player p) {
  if (state.roundOver) return false;
  int pIdx = static_cast<int>(p);
  for (int i = 0; i < state.handCount[pIdx]; ++i) {
    int8_t tIdx = state.hand[pIdx][i];
    if (canPlayLeft(state, tIdx) || canPlayRight(state, tIdx)) {
      return true;
    }
  }
  return false;
}

bool drawFromBoneyard(State& state) {
  if (state.roundOver || state.boneyardCount == 0) return false;
  int8_t tile = state.boneyard[--state.boneyardCount];
  int pIdx = static_cast<int>(state.turn);
  state.hand[pIdx][state.handCount[pIdx]++] = tile;
  state.consecutivePasses = 0;
  return true;
}

bool passTurn(State& state) {
  if (state.roundOver) return false;
  if (hasAnyLegalPlay(state, state.turn)) return false;
  if (state.boneyardCount > 0) return false;

  state.consecutivePasses++;
  if (state.consecutivePasses >= 2) {
    // Blocked game!
    state.roundOver = true;
    int sum0 = getHandPipSum(state, Player::P1);
    int sum1 = getHandPipSum(state, Player::P2);
    if (sum0 < sum1) {
      state.winner = Player::P1;
      state.score[0] += (sum1 - sum0);
    } else if (sum1 < sum0) {
      state.winner = Player::P2;
      state.score[1] += (sum0 - sum1);
    } else {
      state.winner = Player::None;  // Tie
    }
    return true;
  }

  state.turn = opponent(state.turn);
  state.selectedHandIdx = -1;
  return true;
}

bool playTile(State& state, int handIdx, PlaySide side) {
  if (state.roundOver) return false;
  int pIdx = static_cast<int>(state.turn);
  if (handIdx < 0 || handIdx >= state.handCount[pIdx]) return false;

  int8_t tileIdx = state.hand[pIdx][handIdx];
  const auto& t = kTiles[tileIdx];

  if (side == PlaySide::Left) {
    if (!canPlayLeft(state, tileIdx)) return false;
    int8_t p0, p1;
    if (t.pips[1] == state.leftEnd) {
      p0 = t.pips[0];
      p1 = t.pips[1];
    } else {
      p0 = t.pips[1];
      p1 = t.pips[0];
    }
    // Shift train right by 1
    for (int i = state.trainCount; i > 0; --i) {
      state.train[i] = state.train[i - 1];
    }
    state.train[0] = {tileIdx, {p0, p1}};
    state.leftEnd = p0;
  } else {
    if (!canPlayRight(state, tileIdx)) return false;
    int8_t p0, p1;
    if (t.pips[0] == state.rightEnd) {
      p0 = t.pips[0];
      p1 = t.pips[1];
    } else {
      p0 = t.pips[1];
      p1 = t.pips[0];
    }
    state.train[state.trainCount] = {tileIdx, {p0, p1}};
    state.rightEnd = p1;
  }

  // Remove tile from hand
  for (int i = handIdx; i < state.handCount[pIdx] - 1; ++i) {
    state.hand[pIdx][i] = state.hand[pIdx][i + 1];
  }
  state.handCount[pIdx]--;
  state.trainCount++;
  state.consecutivePasses = 0;

  // Check Domino! win
  if (state.handCount[pIdx] == 0) {
    state.roundOver = true;
    state.winner = state.turn;
    int score = getHandPipSum(state, opponent(state.turn));
    state.score[pIdx] += score;
    return true;
  }

  state.turn = opponent(state.turn);
  state.selectedHandIdx = -1;
  return true;
}

}  // namespace dominoes
