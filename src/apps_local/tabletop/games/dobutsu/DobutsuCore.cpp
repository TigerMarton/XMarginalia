#include "DobutsuCore.h"

namespace dobutsu {

void initGame(Game& game) {
  for (int i = 0; i < kCells; ++i) {
    game.board[i] = 0;
  }
  for (int p = 0; p < 2; ++p) {
    for (int r = 0; r < 3; ++r) {
      game.reserves[p][r] = 0;
    }
  }

  // Row 0 (P2 home / North back rank)
  game.board[0] = makePiece(Player::P2, PieceType::Giraffe);
  game.board[1] = makePiece(Player::P2, PieceType::Lion);
  game.board[2] = makePiece(Player::P2, PieceType::Elephant);

  // Row 1 (P2 front)
  game.board[3] = 0;
  game.board[4] = makePiece(Player::P2, PieceType::Chick);
  game.board[5] = 0;

  // Row 2 (P1 front)
  game.board[6] = 0;
  game.board[7] = makePiece(Player::P1, PieceType::Chick);
  game.board[8] = 0;

  // Row 3 (P1 home / South back rank)
  game.board[9] = makePiece(Player::P1, PieceType::Elephant);
  game.board[10] = makePiece(Player::P1, PieceType::Lion);
  game.board[11] = makePiece(Player::P1, PieceType::Giraffe);

  game.turn = Player::P1;
  game.winner = Player::None;
  game.isOver = false;
  game.lionTryPlayer = 0;
}

namespace {

struct Step {
  int8_t dx;
  int8_t dy;
};

// Returns number of step vectors for a piece type and player
size_t getStepVectors(PieceType type, Player p, const Step** outSteps) {
  static const Step kLionSteps[] = {
      {-1, -1}, {0, -1}, {1, -1},
      {-1,  0},          {1,  0},
      {-1,  1}, {0,  1}, {1,  1}
  };
  static const Step kGiraffeSteps[] = {
      {0, -1}, {0, 1}, {-1, 0}, {1, 0}
  };
  static const Step kElephantSteps[] = {
      {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
  };
  static const Step kP1ChickSteps[] = {
      {0, -1}
  };
  static const Step kP2ChickSteps[] = {
      {0, 1}
  };
  static const Step kP1HenSteps[] = {
      {0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, -1}, {1, -1}
  };
  static const Step kP2HenSteps[] = {
      {0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, 1}, {1, 1}
  };

  switch (type) {
    case PieceType::Lion:
      *outSteps = kLionSteps;
      return 8;
    case PieceType::Giraffe:
      *outSteps = kGiraffeSteps;
      return 4;
    case PieceType::Elephant:
      *outSteps = kElephantSteps;
      return 4;
    case PieceType::Chick:
      if (p == Player::P1) {
        *outSteps = kP1ChickSteps;
        return 1;
      } else {
        *outSteps = kP2ChickSteps;
        return 1;
      }
    case PieceType::Hen:
      if (p == Player::P1) {
        *outSteps = kP1HenSteps;
        return 6;
      } else {
        *outSteps = kP2HenSteps;
        return 6;
      }
    default:
      *outSteps = nullptr;
      return 0;
  }
}

}  // namespace

bool canAttackSquare(const Game& game, Player attacker, int8_t targetSq) {
  if (targetSq < 0 || targetSq >= kCells) return false;
  const int8_t tx = static_cast<int8_t>(targetSq % kCols);
  const int8_t ty = static_cast<int8_t>(targetSq / kCols);

  for (int8_t sq = 0; sq < kCells; ++sq) {
    const Cell c = game.board[sq];
    if (c == 0 || pieceOwner(c) != attacker) continue;

    const PieceType pt = pieceType(c);
    const Step* steps = nullptr;
    const size_t nSteps = getStepVectors(pt, attacker, &steps);

    const int8_t sx = static_cast<int8_t>(sq % kCols);
    const int8_t sy = static_cast<int8_t>(sq / kCols);

    for (size_t i = 0; i < nSteps; ++i) {
      if (sx + steps[i].dx == tx && sy + steps[i].dy == ty) {
        return true;
      }
    }
  }
  return false;
}

bool isLegalMove(const Game& game, const Move& move) {
  if (game.isOver) return false;
  if (move.to < 0 || move.to >= kCells) return false;

  const int pIdx = (game.turn == Player::P1) ? 0 : 1;

  if (move.isDrop()) {
    // Drop piece into empty square
    if (game.board[move.to] != 0) return false;
    const PieceType pt = dropTypeFromIndex(move.from);
    if (pt == PieceType::None) return false;
    const size_t rIdx = reserveIndex(pt);
    if (game.reserves[pIdx][rIdx] == 0) return false;
    return true;
  }

  // Board move
  if (move.from < 0 || move.from >= kCells) return false;
  if (move.from == move.to) return false;

  const Cell piece = game.board[move.from];
  if (piece == 0 || pieceOwner(piece) != game.turn) return false;

  const Cell destPiece = game.board[move.to];
  if (destPiece != 0 && pieceOwner(destPiece) == game.turn) {
    return false;  // Cannot capture own piece
  }

  const int8_t fx = static_cast<int8_t>(move.from % kCols);
  const int8_t fy = static_cast<int8_t>(move.from / kCols);
  const int8_t tx = static_cast<int8_t>(move.to % kCols);
  const int8_t ty = static_cast<int8_t>(move.to / kCols);

  const Step* steps = nullptr;
  const size_t nSteps = getStepVectors(pieceType(piece), game.turn, &steps);

  for (size_t i = 0; i < nSteps; ++i) {
    if (fx + steps[i].dx == tx && fy + steps[i].dy == ty) {
      return true;
    }
  }
  return false;
}

size_t generateLegalMoves(const Game& game, Move* outMoves, size_t maxMoves) {
  if (game.isOver || !outMoves || maxMoves == 0) return 0;

  size_t count = 0;
  const int pIdx = (game.turn == Player::P1) ? 0 : 1;

  // 1. Board moves
  for (int8_t sq = 0; sq < kCells; ++sq) {
    const Cell c = game.board[sq];
    if (c == 0 || pieceOwner(c) != game.turn) continue;

    const PieceType pt = pieceType(c);
    const Step* steps = nullptr;
    const size_t nSteps = getStepVectors(pt, game.turn, &steps);

    const int8_t fx = static_cast<int8_t>(sq % kCols);
    const int8_t fy = static_cast<int8_t>(sq / kCols);

    for (size_t i = 0; i < nSteps; ++i) {
      const int8_t tx = static_cast<int8_t>(fx + steps[i].dx);
      const int8_t ty = static_cast<int8_t>(fy + steps[i].dy);

      if (tx < 0 || tx >= kCols || ty < 0 || ty >= kRows) continue;

      const int8_t toSq = static_cast<int8_t>(ty * kCols + tx);
      const Cell dest = game.board[toSq];
      if (dest != 0 && pieceOwner(dest) == game.turn) continue;

      bool promotes = false;
      if (pt == PieceType::Chick) {
        if ((game.turn == Player::P1 && ty == 0) ||
            (game.turn == Player::P2 && ty == 3)) {
          promotes = true;
        }
      }

      if (count < maxMoves) {
        outMoves[count++] = Move{sq, toSq, promotes};
      }
    }
  }

  // 2. Drop moves from reserve
  const int8_t dropIndices[] = {kDropGiraffe, kDropElephant, kDropChick};

  for (size_t d = 0; d < 3; ++d) {
    if (game.reserves[pIdx][d] > 0) {
      for (int8_t toSq = 0; toSq < kCells; ++toSq) {
        if (game.board[toSq] == 0) {
          if (count < maxMoves) {
            outMoves[count++] = Move{dropIndices[d], toSq, false};
          }
        }
      }
    }
  }

  return count;
}

bool applyMove(Game& game, const Move& move) {
  if (!isLegalMove(game, move)) return false;

  const int pIdx = (game.turn == Player::P1) ? 0 : 1;
  const Player opp = opponent(game.turn);

  // Check if opponent had an active Lion Try pending
  if (game.lionTryPlayer == static_cast<int8_t>(opp)) {
    // If this move does NOT capture the opponent's Lion, the opponent wins via Try!
    const Cell captured = move.isDrop() ? 0 : game.board[move.to];
    if (pieceType(captured) != PieceType::Lion) {
      game.winner = opp;
      game.isOver = true;
      return true;
    }
  }
  game.lionTryPlayer = 0;

  if (move.isDrop()) {
    const PieceType pt = dropTypeFromIndex(move.from);
    const size_t rIdx = reserveIndex(pt);
    game.reserves[pIdx][rIdx]--;
    game.board[move.to] = makePiece(game.turn, pt);
  } else {
    const Cell srcPiece = game.board[move.from];
    const Cell destPiece = game.board[move.to];

    // Capture handling
    if (destPiece != 0) {
      const PieceType capType = pieceType(destPiece);
      if (capType == PieceType::Lion) {
        // "Catch the Lion" - instant victory!
        game.board[move.to] = srcPiece;
        game.board[move.from] = 0;
        game.winner = game.turn;
        game.isOver = true;
        return true;
      }
      // Hens revert to Chicks upon capture
      const PieceType addedType = (capType == PieceType::Hen) ? PieceType::Chick : capType;
      const size_t rIdx = reserveIndex(addedType);
      game.reserves[pIdx][rIdx]++;
    }

    // Moving piece placement & promotion
    PieceType newType = pieceType(srcPiece);
    const int8_t destY = static_cast<int8_t>(move.to / kCols);

    if (newType == PieceType::Chick) {
      if ((game.turn == Player::P1 && destY == 0) ||
          (game.turn == Player::P2 && destY == 3)) {
        newType = PieceType::Hen;
      }
    }

    game.board[move.to] = makePiece(game.turn, newType);
    game.board[move.from] = 0;

    // Check Lion Try (reaching back rank)
    if (newType == PieceType::Lion) {
      const bool reachedTry = (game.turn == Player::P1 && destY == 0) ||
                              (game.turn == Player::P2 && destY == 3);
      if (reachedTry) {
        if (!canAttackSquare(game, opp, move.to)) {
          // Unattacked: instant win!
          game.winner = game.turn;
          game.isOver = true;
          return true;
        } else {
          // Attacked: opponent has one turn to respond
          game.lionTryPlayer = static_cast<int8_t>(game.turn);
        }
      }
    }
  }

  game.turn = opp;
  return true;
}

}  // namespace dobutsu
