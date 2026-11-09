#include "BackgammonCore.h"

namespace backgammon {

namespace {

inline uint32_t xorshift32(uint32_t& state) {
  if (state == 0) state = 0x87654321;
  uint32_t x = state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}

}  // namespace

void initGame(Game& game) {
  for (int i = 0; i < kPoints; ++i) {
    game.board[i] = 0;
  }
  game.bar[0] = game.bar[1] = 0;
  game.borneOff[0] = game.borneOff[1] = 0;

  // White (+): moves 23 -> 0
  game.board[23] = 2;
  game.board[12] = 5;
  game.board[7] = 3;
  game.board[5] = 5;

  // Black (-): moves 0 -> 23
  game.board[0] = -2;
  game.board[11] = -5;
  game.board[16] = -3;
  game.board[18] = -5;

  game.dice[0] = game.dice[1] = 0;
  game.moveCount = 0;
  game.turn = Player::White;
  game.isRolled = false;
  game.isOver = false;
  game.winner = Player::White;
  game.winMultiplier = 1;
}

void rollDice(Game& game, uint32_t seed) {
  if (game.isOver) return;

  uint32_t rng = seed;
  const uint8_t d1 = static_cast<uint8_t>(1 + (xorshift32(rng) % 6));
  const uint8_t d2 = static_cast<uint8_t>(1 + (xorshift32(rng) % 6));

  game.dice[0] = d1;
  game.dice[1] = d2;

  if (d1 == d2) {
    game.movesLeft[0] = d1;
    game.movesLeft[1] = d1;
    game.movesLeft[2] = d1;
    game.movesLeft[3] = d1;
    game.moveCount = 4;
  } else {
    game.movesLeft[0] = d1;
    game.movesLeft[1] = d2;
    game.moveCount = 2;
  }
  game.isRolled = true;
}

bool canBearOff(const Game& game, Player p) {
  const int pIdx = static_cast<int>(p);
  if (game.bar[pIdx] > 0) return false;

  if (p == Player::White) {
    // White home board is 0..5. Check if any White checkers exist on 6..23
    for (int pt = 6; pt < kPoints; ++pt) {
      if (game.board[pt] > 0) return false;
    }
    return true;
  } else {
    // Black home board is 18..23. Check if any Black checkers exist on 0..17
    for (int pt = 0; pt < 18; ++pt) {
      if (game.board[pt] < 0) return false;
    }
    return true;
  }
}

bool isLegalMove(const Game& game, const Move& move) {
  if (game.isOver || !game.isRolled || game.moveCount == 0) return false;

  // Check if move.die is available in movesLeft
  bool hasDie = false;
  for (uint8_t i = 0; i < game.moveCount; ++i) {
    if (game.movesLeft[i] == move.die) {
      hasDie = true;
      break;
    }
  }
  if (!hasDie) return false;

  const int pIdx = static_cast<int>(game.turn);

  // 1. Must re-enter from bar if bar has checkers
  if (game.bar[pIdx] > 0) {
    if (move.from != kFromBar) return false;

    if (game.turn == Player::White) {
      const int8_t target = static_cast<int8_t>(24 - move.die);
      if (move.to != target) return false;
      if (game.board[target] < -1) return false;  // Blocked by 2+ Black
      return true;
    } else {
      const int8_t target = static_cast<int8_t>(move.die - 1);
      if (move.to != target) return false;
      if (game.board[target] > 1) return false;   // Blocked by 2+ White
      return true;
    }
  }

  // 2. Normal moves (bar is empty)
  if (move.from < 0 || move.from >= kPoints) return false;

  if (game.turn == Player::White) {
    if (game.board[move.from] <= 0) return false;  // No White checker

    // Bearing off
    if (move.to == kToOff) {
      if (!canBearOff(game, Player::White)) return false;
      const int target = move.from - move.die;
      if (target == -1) return true;  // Exact off
      if (target < -1) {
        // Overshoot: only legal if no checkers on points higher than move.from in home board
        for (int pt = move.from + 1; pt <= 5; ++pt) {
          if (game.board[pt] > 0) return false;
        }
        return true;
      }
      return false;
    }

    // Board move
    const int8_t target = static_cast<int8_t>(move.from - move.die);
    if (move.to != target || target < 0) return false;
    if (game.board[target] < -1) return false;  // Blocked by 2+ Black
    return true;
  } else {
    if (game.board[move.from] >= 0) return false;  // No Black checker

    // Bearing off
    if (move.to == kToOff) {
      if (!canBearOff(game, Player::Black)) return false;
      const int target = move.from + move.die;
      if (target == 24) return true;  // Exact off
      if (target > 24) {
        // Overshoot: only legal if no checkers on points lower than move.from in home board
        for (int pt = 18; pt < move.from; ++pt) {
          if (game.board[pt] < 0) return false;
        }
        return true;
      }
      return false;
    }

    // Board move
    const int8_t target = static_cast<int8_t>(move.from + move.die);
    if (move.to != target || target >= kPoints) return false;
    if (game.board[target] > 1) return false;   // Blocked by 2+ White
    return true;
  }
}

size_t generateLegalMoves(const Game& game, Move* outMoves, size_t maxMoves) {
  if (game.isOver || !game.isRolled || game.moveCount == 0 || !outMoves || maxMoves == 0) {
    return 0;
  }

  size_t count = 0;
  const int pIdx = static_cast<int>(game.turn);

  // Collect distinct dice values to avoid generating duplicate moves
  uint8_t distinctDice[4];
  size_t nDistinct = 0;
  for (uint8_t i = 0; i < game.moveCount; ++i) {
    const uint8_t d = game.movesLeft[i];
    bool found = false;
    for (size_t j = 0; j < nDistinct; ++j) {
      if (distinctDice[j] == d) { found = true; break; }
    }
    if (!found) distinctDice[nDistinct++] = d;
  }

  // 1. If bar has checkers, must move from bar
  if (game.bar[pIdx] > 0) {
    for (size_t di = 0; di < nDistinct; ++di) {
      const uint8_t d = distinctDice[di];
      const int8_t target = (game.turn == Player::White) ? static_cast<int8_t>(24 - d)
                                                         : static_cast<int8_t>(d - 1);
      const Move m{kFromBar, target, d};
      if (isLegalMove(game, m)) {
        if (count < maxMoves) outMoves[count++] = m;
      }
    }
    return count;
  }

  // 2. Board moves and bear-offs
  for (int8_t pt = 0; pt < kPoints; ++pt) {
    const bool hasChecker = (game.turn == Player::White) ? (game.board[pt] > 0)
                                                         : (game.board[pt] < 0);
    if (!hasChecker) continue;

    for (size_t di = 0; di < nDistinct; ++di) {
      const uint8_t d = distinctDice[di];

      // Try regular move
      const int8_t target = (game.turn == Player::White) ? static_cast<int8_t>(pt - d)
                                                         : static_cast<int8_t>(pt + d);
      const Move mRegular{pt, target, d};
      if (isLegalMove(game, mRegular)) {
        if (count < maxMoves) outMoves[count++] = mRegular;
      }

      // Try bear-off
      const Move mOff{pt, kToOff, d};
      if (isLegalMove(game, mOff)) {
        if (count < maxMoves) outMoves[count++] = mOff;
      }
    }
  }

  return count;
}

bool applyMove(Game& game, const Move& move) {
  if (!isLegalMove(game, move)) return false;

  const int pIdx = static_cast<int>(game.turn);
  const int oppIdx = 1 - pIdx;

  // Consume die
  for (uint8_t i = 0; i < game.moveCount; ++i) {
    if (game.movesLeft[i] == move.die) {
      for (uint8_t j = i; j + 1 < game.moveCount; ++j) {
        game.movesLeft[j] = game.movesLeft[j + 1];
      }
      game.moveCount--;
      break;
    }
  }

  // Source deduction
  if (move.from == kFromBar) {
    game.bar[pIdx]--;
  } else {
    if (game.turn == Player::White) {
      game.board[move.from]--;
    } else {
      game.board[move.from]++;
    }
  }

  // Destination placement
  if (move.to == kToOff) {
    game.borneOff[pIdx]++;
    if (game.borneOff[pIdx] == kCheckersPerPlayer) {
      // Game Over!
      game.isOver = true;
      game.winner = game.turn;

      // Check Gammon / Backgammon
      if (game.borneOff[oppIdx] == 0) {
        bool inHomeOrBar = (game.bar[oppIdx] > 0);
        if (!inHomeOrBar) {
          if (game.turn == Player::White) {
            // Check if Black has checkers in White's home board (0..5)
            for (int pt = 0; pt <= 5; ++pt) {
              if (game.board[pt] < 0) { inHomeOrBar = true; break; }
            }
          } else {
            // Check if White has checkers in Black's home board (18..23)
            for (int pt = 18; pt <= 23; ++pt) {
              if (game.board[pt] > 0) { inHomeOrBar = true; break; }
            }
          }
        }
        game.winMultiplier = inHomeOrBar ? 3 : 2;
      } else {
        game.winMultiplier = 1;
      }
      return true;
    }
  } else {
    // Normal point destination
    if (game.turn == Player::White) {
      if (game.board[move.to] == -1) {
        // Hit Black blot!
        game.board[move.to] = 1;
        game.bar[1]++;
      } else {
        game.board[move.to]++;
      }
    } else {
      if (game.board[move.to] == 1) {
        // Hit White blot!
        game.board[move.to] = -1;
        game.bar[0]++;
      } else {
        game.board[move.to]--;
      }
    }
  }

  // Check if turn should pass
  if (game.moveCount == 0) {
    passTurn(game);
  } else {
    Move nextMoves[64];
    if (generateLegalMoves(game, nextMoves, 64) == 0) {
      passTurn(game);
    }
  }

  return true;
}

void passTurn(Game& game) {
  game.turn = opponent(game.turn);
  game.isRolled = false;
  game.moveCount = 0;
  game.dice[0] = game.dice[1] = 0;
}

int calculatePipCount(const Game& game, Player p) {
  int pips = 0;
  if (p == Player::White) {
    pips += static_cast<int>(game.bar[0]) * 25;
    for (int pt = 0; pt < kPoints; ++pt) {
      if (game.board[pt] > 0) {
        pips += game.board[pt] * (pt + 1);
      }
    }
  } else {
    pips += static_cast<int>(game.bar[1]) * 25;
    for (int pt = 0; pt < kPoints; ++pt) {
      if (game.board[pt] < 0) {
        pips += (-game.board[pt]) * (24 - pt);
      }
    }
  }
  return pips;
}

}  // namespace backgammon
