#include "TabletopSave.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace tabletop {
namespace save {

int pack(const Save& save, char* out, const int capacity) {
  if (out == nullptr || capacity < 256) return 0;

  int written = 0;
  auto append = [&](const char* fmt, ...) -> bool {
    va_list args;
    va_start(args, fmt);
    const int rem = capacity - written;
    if (rem <= 0) {
      va_end(args);
      return false;
    }
    const int n = std::vsnprintf(out + written, static_cast<size_t>(rem), fmt, args);
    va_end(args);
    if (n < 0 || n >= rem) return false;
    written += n;
    return true;
  };

  // Header
  if (!append("TT %d %u\n", kVersion, static_cast<unsigned>(save.lastScreen))) return 0;

  // Reversi
  const uint32_t bHi = static_cast<uint32_t>(save.reversiGame.black >> 32);
  const uint32_t bLo = static_cast<uint32_t>(save.reversiGame.black & 0xFFFFFFFFULL);
  const uint32_t wHi = static_cast<uint32_t>(save.reversiGame.white >> 32);
  const uint32_t wLo = static_cast<uint32_t>(save.reversiGame.white & 0xFFFFFFFFULL);
  if (!append("REV %u %d %d %u %u %u %u %u\n",
              static_cast<unsigned>(save.reversiMode),
              save.reversiFlip ? 1 : 0,
              save.reversiGame.isOver ? 1 : 0,
              static_cast<unsigned>(save.reversiGame.turn),
              static_cast<unsigned>(bHi), static_cast<unsigned>(bLo),
              static_cast<unsigned>(wHi), static_cast<unsigned>(wLo))) {
    return 0;
  }

  // Mancala
  if (!append("MAN %u %d %d %u %d",
              static_cast<unsigned>(save.mancalaMode),
              save.mancalaFlip ? 1 : 0,
              save.mancalaGame.isOver ? 1 : 0,
              static_cast<unsigned>(save.mancalaGame.turn),
              save.mancalaGame.extraTurn ? 1 : 0)) {
    return 0;
  }
  for (int i = 0; i < 14; ++i) {
    if (!append(" %u", static_cast<unsigned>(save.mancalaGame.pits[i]))) return 0;
  }
  if (!append("\n")) return 0;

  // Morris
  if (!append("MOR %u %d %d %u %d %u %u %u %u %d\n",
              static_cast<unsigned>(save.morrisMode),
              save.morrisFlip ? 1 : 0,
              save.morrisGame.isOver ? 1 : 0,
              static_cast<unsigned>(save.morrisGame.turn),
              save.morrisGame.pendingRemoval ? 1 : 0,
              static_cast<unsigned>(save.morrisGame.placed[morris::kWhite]),
              static_cast<unsigned>(save.morrisGame.placed[morris::kBlack]),
              static_cast<unsigned>(save.morrisGame.pieces[morris::kWhite]),
              static_cast<unsigned>(save.morrisGame.pieces[morris::kBlack]),
              static_cast<int>(save.morrisGame.selected))) {
    return 0;
  }

  // Onitama
  if (!append("ONI %u %d %d %u %u %u %u %u %u %u %u",
              static_cast<unsigned>(save.onitamaMode),
              save.onitamaFlip ? 1 : 0,
              save.onitamaGame.isOver ? 1 : 0,
              static_cast<unsigned>(save.onitamaGame.winner),
              static_cast<unsigned>(save.onitamaGame.winReason),
              static_cast<unsigned>(save.onitamaGame.turn),
              static_cast<unsigned>(save.onitamaGame.tableCard),
              static_cast<unsigned>(save.onitamaGame.p1Cards[0]),
              static_cast<unsigned>(save.onitamaGame.p1Cards[1]),
              static_cast<unsigned>(save.onitamaGame.p2Cards[0]),
              static_cast<unsigned>(save.onitamaGame.p2Cards[1]))) {
    return 0;
  }
  for (int i = 0; i < onitama::kBoardCells; ++i) {
    if (!append(" %u", static_cast<unsigned>(save.onitamaGame.cells[i]))) return 0;
  }
  if (!append("\n")) return 0;

  // Liar's Dice
  const auto& ld = save.liarsDiceGame;
  if (!append("LIA %u %u %u %u %u %u %u %u %u %u %u %u",
              static_cast<unsigned>(save.liarsDiceMode),
              static_cast<unsigned>(ld.playerCount),
              static_cast<unsigned>(ld.activePlayer),
              static_cast<unsigned>(ld.phase),
              static_cast<unsigned>(ld.currentBid.count),
              static_cast<unsigned>(ld.currentBid.face),
              static_cast<unsigned>(ld.currentBid.bidder),
              static_cast<unsigned>(ld.challenger),
              static_cast<unsigned>(ld.roundLoser),
              static_cast<unsigned>(ld.totalMatchingDice),
              ld.challengeSuccessful ? 1 : 0,
              static_cast<unsigned>(ld.winner))) {
    return 0;
  }
  for (int p = 0; p < liarsdice::kMaxPlayers; ++p) {
    if (!append(" %u", static_cast<unsigned>(ld.players[p].diceCount))) return 0;
    for (int d = 0; d < liarsdice::kStartingDice; ++d) {
      if (!append(" %u", static_cast<unsigned>(ld.players[p].dice[d]))) return 0;
    }
  }
  if (!append("\n")) return 0;

  // Dobutsu Shogi
  const auto& dob = save.dobutsuGame;
  if (!append("DOB %u %d %d %u %u %d",
              static_cast<unsigned>(save.dobutsuMode),
              save.dobutsuFlip ? 1 : 0,
              dob.isOver ? 1 : 0,
              static_cast<unsigned>(dob.winner),
              static_cast<unsigned>(dob.turn),
              static_cast<int>(dob.lionTryPlayer))) {
    return 0;
  }
  for (int i = 0; i < dobutsu::kCells; ++i) {
    if (!append(" %u", static_cast<unsigned>(dob.board[i]))) return 0;
  }
  for (int p = 0; p < 2; ++p) {
    for (int r = 0; r < 3; ++r) {
      if (!append(" %u", static_cast<unsigned>(dob.reserves[p][r]))) return 0;
    }
  }
  if (!append("\n")) return 0;

  // Backgammon
  const auto& bkg = save.backgammonGame;
  if (!append("BKG %u %d %d %u %u %d %u %u %u",
              static_cast<unsigned>(save.backgammonMode),
              save.backgammonFlip ? 1 : 0,
              bkg.isOver ? 1 : 0,
              static_cast<unsigned>(bkg.winner),
              static_cast<unsigned>(bkg.turn),
              bkg.isRolled ? 1 : 0,
              static_cast<unsigned>(bkg.winMultiplier),
              static_cast<unsigned>(bkg.dice[0]),
              static_cast<unsigned>(bkg.dice[1]))) {
    return 0;
  }
  if (!append(" %u %u %u %u %u",
              static_cast<unsigned>(bkg.moveCount),
              static_cast<unsigned>(bkg.movesLeft[0]),
              static_cast<unsigned>(bkg.movesLeft[1]),
              static_cast<unsigned>(bkg.movesLeft[2]),
              static_cast<unsigned>(bkg.movesLeft[3]))) {
    return 0;
  }
  if (!append(" %u %u %u %u",
              static_cast<unsigned>(bkg.bar[0]),
              static_cast<unsigned>(bkg.bar[1]),
              static_cast<unsigned>(bkg.borneOff[0]),
              static_cast<unsigned>(bkg.borneOff[1]))) {
    return 0;
  }
  for (int i = 0; i < backgammon::kPoints; ++i) {
    if (!append(" %d", static_cast<int>(bkg.board[i]))) return 0;
  }
  if (!append("\n")) return 0;

  // Santorini
  const auto& sto = save.santoriniState;
  if (!append("STO %u %d %u %u %d %u",
              static_cast<unsigned>(sto.mode),
              sto.flip ? 1 : 0,
              static_cast<unsigned>(sto.turn),
              static_cast<unsigned>(sto.phase),
              static_cast<int>(sto.selectedWorker),
              static_cast<unsigned>(sto.winner))) {
    return 0;
  }
  for (int i = 0; i < santorini::kCells; ++i) {
    if (!append(" %u", static_cast<unsigned>(sto.board[i]))) return 0;
  }
  if (!append(" %d %d %d %d\n",
              static_cast<int>(sto.workers[0][0]),
              static_cast<int>(sto.workers[0][1]),
              static_cast<int>(sto.workers[1][0]),
              static_cast<int>(sto.workers[1][1]))) {
    return 0;
  }

  // Quoridor
  const auto& qrd = save.quoridorState;
  const uint32_t qhHi = static_cast<uint32_t>(qrd.hWalls >> 32);
  const uint32_t qhLo = static_cast<uint32_t>(qrd.hWalls & 0xFFFFFFFFULL);
  const uint32_t qvHi = static_cast<uint32_t>(qrd.vWalls >> 32);
  const uint32_t qvLo = static_cast<uint32_t>(qrd.vWalls & 0xFFFFFFFFULL);
  if (!append("QRD %u %d %u %u %d %d %u %u %u %u %u %u %d %d %u\n",
              static_cast<unsigned>(qrd.mode),
              qrd.flip ? 1 : 0,
              static_cast<unsigned>(qrd.turn),
              static_cast<unsigned>(qrd.winner),
              static_cast<int>(qrd.pawn[0]),
              static_cast<int>(qrd.pawn[1]),
              static_cast<unsigned>(qrd.wallsLeft[0]),
              static_cast<unsigned>(qrd.wallsLeft[1]),
              static_cast<unsigned>(qhHi), static_cast<unsigned>(qhLo),
              static_cast<unsigned>(qvHi), static_cast<unsigned>(qvLo),
              static_cast<int>(qrd.selectedGx), static_cast<int>(qrd.selectedGy),
              static_cast<unsigned>(qrd.curOrient))) {
    return 0;
  }

  // Dominoes
  const auto& dom = save.dominoesState;
  if (!append("DOM %u %u %u %d %u %d %d %d %d %d %u",
              static_cast<unsigned>(dom.mode),
              static_cast<unsigned>(dom.turn),
              static_cast<unsigned>(dom.winner),
              dom.roundOver ? 1 : 0,
              static_cast<unsigned>(dom.consecutivePasses),
              dom.score[0], dom.score[1],
              static_cast<int>(dom.leftEnd),
              static_cast<int>(dom.rightEnd),
              dom.privacyConcealed ? 1 : 0,
              static_cast<unsigned>(dom.trainCount))) {
    return 0;
  }
  for (int i = 0; i < dom.trainCount && i < dominoes::kTotalTiles; ++i) {
    if (!append(" %d %d %d",
                static_cast<int>(dom.train[i].tileIdx),
                static_cast<int>(dom.train[i].pips[0]),
                static_cast<int>(dom.train[i].pips[1]))) {
      return 0;
    }
  }
  if (!append(" %u", static_cast<unsigned>(dom.handCount[0]))) return 0;
  for (int i = 0; i < dom.handCount[0] && i < dominoes::kTotalTiles; ++i) {
    if (!append(" %d", static_cast<int>(dom.hand[0][i]))) return 0;
  }
  if (!append(" %u", static_cast<unsigned>(dom.handCount[1]))) return 0;
  for (int i = 0; i < dom.handCount[1] && i < dominoes::kTotalTiles; ++i) {
    if (!append(" %d", static_cast<int>(dom.hand[1][i]))) return 0;
  }
  if (!append(" %u", static_cast<unsigned>(dom.boneyardCount))) return 0;
  for (int i = 0; i < dom.boneyardCount && i < dominoes::kTotalTiles; ++i) {
    if (!append(" %d", static_cast<int>(dom.boneyard[i]))) return 0;
  }
  if (!append("\n")) return 0;

  // Sandbox
  const auto& sb = save.sandboxState;
  if (!append("SAN %u %u %u %u %d",
              static_cast<unsigned>(sb.mode),
              static_cast<unsigned>(sb.diceCount),
              static_cast<unsigned>(sb.dieType),
              static_cast<unsigned>(sb.heldMask),
              static_cast<int>(sb.lastRollSum))) {
    return 0;
  }
  for (int i = 0; i < sandbox::kMaxDice; ++i) {
    if (!append(" %u", static_cast<unsigned>(sb.diceValues[i]))) return 0;
  }
  if (!append(" %u %u %d %u",
              static_cast<unsigned>(sb.deckCount),
              static_cast<unsigned>(sb.handCount),
              sb.handHidden ? 1 : 0,
              static_cast<unsigned>(sb.discardTop))) {
    return 0;
  }
  for (int i = 0; i < sandbox::kMaxHand; ++i) {
    if (!append(" %u", static_cast<unsigned>(sb.hand[i]))) return 0;
  }
  for (int i = 0; i < sandbox::kMaxPlayers; ++i) {
    if (!append(" %d", static_cast<int>(sb.scores[i]))) return 0;
  }
  if (!append(" %u %u %u",
              static_cast<unsigned>(sb.activePlayer),
              static_cast<unsigned>(sb.spawnColor),
              static_cast<unsigned>(sb.tokenCount))) {
    return 0;
  }
  for (int i = 0; i < sb.tokenCount && i < sandbox::kMaxTokens; ++i) {
    if (!append(" %u %u", static_cast<unsigned>(sb.tokens[i].pos), static_cast<unsigned>(sb.tokens[i].color))) {
      return 0;
    }
  }
  if (!append("\n")) return 0;

  return written;
}

bool unpack(const char* text, Save& save) {
  if (text == nullptr) return false;

  int version = 0;
  unsigned screen = 0;
  int consumed = 0;
  if (std::sscanf(text, "TT %d %u\n%n", &version, &screen, &consumed) < 2 || version != kVersion) {
    return false;
  }
  text += consumed;
  save.lastScreen = static_cast<uint8_t>(screen);

  // Reversi
  unsigned revMode = 0, revTurn = 0;
  int revFlip = 0, revOver = 0;
  uint32_t bHi = 0, bLo = 0, wHi = 0, wLo = 0;
  if (std::sscanf(text, "REV %u %d %d %u %u %u %u %u\n%n",
                  &revMode, &revFlip, &revOver, &revTurn, &bHi, &bLo, &wHi, &wLo, &consumed) < 8) {
    return false;
  }
  text += consumed;
  save.reversiMode = static_cast<uint8_t>(revMode);
  save.reversiFlip = (revFlip != 0);
  save.reversiGame.isOver = (revOver != 0);
  save.reversiGame.turn = static_cast<uint8_t>(revTurn);
  save.reversiGame.black = (static_cast<uint64_t>(bHi) << 32) | bLo;
  save.reversiGame.white = (static_cast<uint64_t>(wHi) << 32) | wLo;

  // Mancala
  unsigned manMode = 0, manTurn = 0;
  int manFlip = 0, manOver = 0, manExtra = 0;
  if (std::sscanf(text, "MAN %u %d %d %u %d%n",
                  &manMode, &manFlip, &manOver, &manTurn, &manExtra, &consumed) < 5) {
    return false;
  }
  text += consumed;
  save.mancalaMode = static_cast<uint8_t>(manMode);
  save.mancalaFlip = (manFlip != 0);
  save.mancalaGame.isOver = (manOver != 0);
  save.mancalaGame.turn = static_cast<uint8_t>(manTurn);
  save.mancalaGame.extraTurn = (manExtra != 0);

  for (int i = 0; i < 14; ++i) {
    unsigned p = 0;
    if (std::sscanf(text, " %u%n", &p, &consumed) < 1) return false;
    text += consumed;
    save.mancalaGame.pits[i] = static_cast<uint8_t>(p);
  }
  if (std::sscanf(text, "\n%n", &consumed) >= 0) text += consumed;

  // Morris
  unsigned morMode = 0, morTurn = 0, placedW = 0, placedB = 0, piecesW = 0, piecesB = 0;
  int morFlip = 0, morOver = 0, morRemoval = 0, morSelected = 0;
  if (std::sscanf(text, "MOR %u %d %d %u %d %u %u %u %u %d\n%n",
                  &morMode, &morFlip, &morOver, &morTurn, &morRemoval,
                  &placedW, &placedB, &piecesW, &piecesB, &morSelected, &consumed) < 10) {
    return false;
  }
  text += consumed;
  save.morrisMode = static_cast<uint8_t>(morMode);
  save.morrisFlip = (morFlip != 0);
  save.morrisGame.isOver = (morOver != 0);
  save.morrisGame.turn = static_cast<uint8_t>(morTurn);
  save.morrisGame.pendingRemoval = (morRemoval != 0);
  save.morrisGame.placed[morris::kWhite] = static_cast<uint8_t>(placedW);
  save.morrisGame.placed[morris::kBlack] = static_cast<uint8_t>(placedB);
  save.morrisGame.pieces[morris::kWhite] = piecesW;
  save.morrisGame.pieces[morris::kBlack] = piecesB;
  save.morrisGame.selected = static_cast<int8_t>(morSelected);

  // Onitama
  unsigned oniMode = 0, oniTurn = 0, oniWinner = 0, oniWinReason = 0;
  unsigned oniTable = 0, oniP1C0 = 0, oniP1C1 = 0, oniP2C0 = 0, oniP2C1 = 0;
  int oniFlip = 0, oniOver = 0;
  if (std::sscanf(text, "ONI %u %d %d %u %u %u %u %u %u %u %u%n",
                  &oniMode, &oniFlip, &oniOver, &oniWinner, &oniWinReason,
                  &oniTurn, &oniTable, &oniP1C0, &oniP1C1, &oniP2C0, &oniP2C1, &consumed) < 11) {
    return false;
  }
  text += consumed;
  save.onitamaMode = static_cast<uint8_t>(oniMode);
  save.onitamaFlip = (oniFlip != 0);
  save.onitamaGame.isOver = (oniOver != 0);
  save.onitamaGame.winner = static_cast<uint8_t>(oniWinner);
  save.onitamaGame.winReason = static_cast<onitama::WinReason>(oniWinReason);
  save.onitamaGame.turn = static_cast<uint8_t>(oniTurn);
  save.onitamaGame.tableCard = static_cast<uint8_t>(oniTable);
  save.onitamaGame.p1Cards[0] = static_cast<uint8_t>(oniP1C0);
  save.onitamaGame.p1Cards[1] = static_cast<uint8_t>(oniP1C1);
  save.onitamaGame.p2Cards[0] = static_cast<uint8_t>(oniP2C0);
  save.onitamaGame.p2Cards[1] = static_cast<uint8_t>(oniP2C1);

  for (int i = 0; i < onitama::kBoardCells; ++i) {
    unsigned cellVal = 0;
    if (std::sscanf(text, " %u%n", &cellVal, &consumed) < 1) return false;
    text += consumed;
    save.onitamaGame.cells[i] = static_cast<uint8_t>(cellVal);
  }
  if (std::sscanf(text, "\n%n", &consumed) >= 0) text += consumed;

  // Liar's Dice
  auto& ld = save.liarsDiceGame;
  unsigned liaMode = 0, ldPlayers = 0, ldActive = 0, ldPhase = 0;
  unsigned bCount = 0, bFace = 0, bBidder = 0, chal = 0, loser = 0, matchD = 0, win = 0;
  int chalSucc = 0;
  if (std::sscanf(text, "LIA %u %u %u %u %u %u %u %u %u %u %d %u%n",
                  &liaMode, &ldPlayers, &ldActive, &ldPhase,
                  &bCount, &bFace, &bBidder, &chal, &loser, &matchD, &chalSucc, &win, &consumed) < 12) {
    return false;
  }
  text += consumed;
  save.liarsDiceMode = static_cast<uint8_t>(liaMode);
  ld.playerCount = static_cast<uint8_t>(ldPlayers);
  ld.activePlayer = static_cast<uint8_t>(ldActive);
  ld.phase = static_cast<liarsdice::Phase>(ldPhase);
  ld.currentBid.count = static_cast<uint8_t>(bCount);
  ld.currentBid.face = static_cast<uint8_t>(bFace);
  ld.currentBid.bidder = static_cast<uint8_t>(bBidder);
  ld.challenger = static_cast<uint8_t>(chal);
  ld.roundLoser = static_cast<uint8_t>(loser);
  ld.totalMatchingDice = static_cast<uint8_t>(matchD);
  ld.challengeSuccessful = (chalSucc != 0);
  ld.winner = static_cast<uint8_t>(win);

  for (int p = 0; p < liarsdice::kMaxPlayers; ++p) {
    unsigned dCount = 0;
    if (std::sscanf(text, " %u%n", &dCount, &consumed) < 1) return false;
    text += consumed;
    ld.players[p].diceCount = static_cast<uint8_t>(dCount);
    for (int d = 0; d < liarsdice::kStartingDice; ++d) {
      unsigned dVal = 0;
      if (std::sscanf(text, " %u%n", &dVal, &consumed) < 1) return false;
      text += consumed;
      ld.players[p].dice[d] = static_cast<uint8_t>(dVal);
    }
  }
  if (std::sscanf(text, "\n%n", &consumed) >= 0) text += consumed;

  // Dobutsu Shogi
  auto& dob = save.dobutsuGame;
  unsigned dobMode = 0, dobWinner = 0, dobTurn = 0;
  int dobFlip = 0, dobOver = 0, dobTry = 0;
  if (std::sscanf(text, "DOB %u %d %d %u %u %d%n",
                  &dobMode, &dobFlip, &dobOver, &dobWinner, &dobTurn, &dobTry, &consumed) < 6) {
    return false;
  }
  text += consumed;
  save.dobutsuMode = static_cast<uint8_t>(dobMode);
  save.dobutsuFlip = (dobFlip != 0);
  dob.isOver = (dobOver != 0);
  dob.winner = static_cast<dobutsu::Player>(dobWinner);
  dob.turn = static_cast<dobutsu::Player>(dobTurn);
  dob.lionTryPlayer = static_cast<int8_t>(dobTry);

  for (int i = 0; i < dobutsu::kCells; ++i) {
    unsigned cellVal = 0;
    if (std::sscanf(text, " %u%n", &cellVal, &consumed) < 1) return false;
    text += consumed;
    dob.board[i] = static_cast<dobutsu::Cell>(cellVal);
  }
  for (int p = 0; p < 2; ++p) {
    for (int r = 0; r < 3; ++r) {
      unsigned rVal = 0;
      if (std::sscanf(text, " %u%n", &rVal, &consumed) < 1) return false;
      text += consumed;
      dob.reserves[p][r] = static_cast<uint8_t>(rVal);
    }
  }
  if (std::sscanf(text, "\n%n", &consumed) >= 0) text += consumed;

  // Backgammon
  auto& bkg = save.backgammonGame;
  unsigned bkgMode = 0, bkgWinner = 0, bkgTurn = 0, bkgMult = 0, d1 = 0, d2 = 0;
  int bkgFlip = 0, bkgOver = 0, bkgRolled = 0;
  if (std::sscanf(text, "BKG %u %d %d %u %u %d %u %u %u%n",
                  &bkgMode, &bkgFlip, &bkgOver, &bkgWinner, &bkgTurn, &bkgRolled,
                  &bkgMult, &d1, &d2, &consumed) < 9) {
    return false;
  }
  text += consumed;
  save.backgammonMode = static_cast<uint8_t>(bkgMode);
  save.backgammonFlip = (bkgFlip != 0);
  bkg.isOver = (bkgOver != 0);
  bkg.winner = static_cast<backgammon::Player>(bkgWinner);
  bkg.turn = static_cast<backgammon::Player>(bkgTurn);
  bkg.isRolled = (bkgRolled != 0);
  bkg.winMultiplier = static_cast<uint8_t>(bkgMult);
  bkg.dice[0] = static_cast<uint8_t>(d1);
  bkg.dice[1] = static_cast<uint8_t>(d2);

  unsigned mCount = 0, ml0 = 0, ml1 = 0, ml2 = 0, ml3 = 0;
  if (std::sscanf(text, " %u %u %u %u %u%n", &mCount, &ml0, &ml1, &ml2, &ml3, &consumed) < 5) {
    return false;
  }
  text += consumed;
  bkg.moveCount = static_cast<uint8_t>(mCount);
  bkg.movesLeft[0] = static_cast<uint8_t>(ml0);
  bkg.movesLeft[1] = static_cast<uint8_t>(ml1);
  bkg.movesLeft[2] = static_cast<uint8_t>(ml2);
  bkg.movesLeft[3] = static_cast<uint8_t>(ml3);

  unsigned bar0 = 0, bar1 = 0, off0 = 0, off1 = 0;
  if (std::sscanf(text, " %u %u %u %u%n", &bar0, &bar1, &off0, &off1, &consumed) < 4) {
    return false;
  }
  text += consumed;
  bkg.bar[0] = static_cast<uint8_t>(bar0);
  bkg.bar[1] = static_cast<uint8_t>(bar1);
  bkg.borneOff[0] = static_cast<uint8_t>(off0);
  bkg.borneOff[1] = static_cast<uint8_t>(off1);

  for (int i = 0; i < backgammon::kPoints; ++i) {
    int bVal = 0;
    if (std::sscanf(text, " %d%n", &bVal, &consumed) < 1) return false;
    text += consumed;
    bkg.board[i] = static_cast<int8_t>(bVal);
  }
  if (std::sscanf(text, "\n%n", &consumed) >= 0) text += consumed;

  // Santorini
  unsigned stoMode = 0, stoTurn = 0, stoPhase = 0, stoWinner = 0;
  int stoFlip = 0, stoSel = 0;
  if (std::sscanf(text, "STO %u %d %u %u %d %u%n",
                  &stoMode, &stoFlip, &stoTurn, &stoPhase, &stoSel, &stoWinner, &consumed) < 6) {
    return false;
  }
  text += consumed;
  save.santoriniState.mode = static_cast<santorini::Mode>(stoMode);
  save.santoriniState.flip = (stoFlip != 0);
  save.santoriniState.turn = static_cast<santorini::Player>(stoTurn);
  save.santoriniState.phase = static_cast<santorini::Phase>(stoPhase);
  save.santoriniState.selectedWorker = static_cast<int8_t>(stoSel);
  save.santoriniState.winner = static_cast<santorini::Player>(stoWinner);

  for (int i = 0; i < santorini::kCells; ++i) {
    unsigned b = 0;
    if (std::sscanf(text, " %u%n", &b, &consumed) < 1) return false;
    text += consumed;
    save.santoriniState.board[i] = static_cast<uint8_t>(b);
  }
  int w00 = 0, w01 = 0, w10 = 0, w11 = 0;
  if (std::sscanf(text, " %d %d %d %d\n%n", &w00, &w01, &w10, &w11, &consumed) < 4) {
    return false;
  }
  text += consumed;
  save.santoriniState.workers[0][0] = static_cast<int8_t>(w00);
  save.santoriniState.workers[0][1] = static_cast<int8_t>(w01);
  save.santoriniState.workers[1][0] = static_cast<int8_t>(w10);
  save.santoriniState.workers[1][1] = static_cast<int8_t>(w11);

  // Quoridor
  unsigned qrdMode = 0, qrdTurn = 0, qrdWinner = 0, wL0 = 0, wL1 = 0, qOrient = 0;
  int qrdFlip = 0, p0 = 0, p1 = 0, selGx = 0, selGy = 0;
  uint32_t qhHi = 0, qhLo = 0, qvHi = 0, qvLo = 0;
  if (std::sscanf(text, "QRD %u %d %u %u %d %d %u %u %u %u %u %u %d %d %u\n%n",
                  &qrdMode, &qrdFlip, &qrdTurn, &qrdWinner, &p0, &p1, &wL0, &wL1,
                  &qhHi, &qhLo, &qvHi, &qvLo, &selGx, &selGy, &qOrient, &consumed) < 15) {
    return false;
  }
  text += consumed;
  save.quoridorState.mode = static_cast<quoridor::Mode>(qrdMode);
  save.quoridorState.flip = (qrdFlip != 0);
  save.quoridorState.turn = static_cast<quoridor::Player>(qrdTurn);
  save.quoridorState.winner = static_cast<quoridor::Player>(qrdWinner);
  save.quoridorState.pawn[0] = static_cast<int8_t>(p0);
  save.quoridorState.pawn[1] = static_cast<int8_t>(p1);
  save.quoridorState.wallsLeft[0] = static_cast<uint8_t>(wL0);
  save.quoridorState.wallsLeft[1] = static_cast<uint8_t>(wL1);
  save.quoridorState.hWalls = (static_cast<uint64_t>(qhHi) << 32) | qhLo;
  save.quoridorState.vWalls = (static_cast<uint64_t>(qvHi) << 32) | qvLo;
  save.quoridorState.selectedGx = static_cast<int8_t>(selGx);
  save.quoridorState.selectedGy = static_cast<int8_t>(selGy);
  save.quoridorState.curOrient = static_cast<quoridor::WallOrientation>(qOrient);
  save.quoridorState.wallPlacementMode = false;

  // Dominoes
  unsigned domMode = 0, domTurn = 0, domWinner = 0, domPass = 0, domTrainC = 0;
  int domRoundOver = 0, domScore0 = 0, domScore1 = 0, domLeft = 0, domRight = 0, domPriv = 0;
  if (std::sscanf(text, "DOM %u %u %u %d %u %d %d %d %d %d %u%n",
                  &domMode, &domTurn, &domWinner, &domRoundOver, &domPass,
                  &domScore0, &domScore1, &domLeft, &domRight, &domPriv, &domTrainC, &consumed) < 11) {
    return false;
  }
  text += consumed;
  save.dominoesState.mode = static_cast<dominoes::Mode>(domMode);
  save.dominoesState.turn = static_cast<dominoes::Player>(domTurn);
  save.dominoesState.winner = static_cast<dominoes::Player>(domWinner);
  save.dominoesState.roundOver = (domRoundOver != 0);
  save.dominoesState.consecutivePasses = static_cast<uint8_t>(domPass);
  save.dominoesState.score[0] = domScore0;
  save.dominoesState.score[1] = domScore1;
  save.dominoesState.leftEnd = static_cast<int8_t>(domLeft);
  save.dominoesState.rightEnd = static_cast<int8_t>(domRight);
  save.dominoesState.privacyConcealed = (domPriv != 0);
  save.dominoesState.trainCount = static_cast<uint8_t>(domTrainC);
  save.dominoesState.selectedHandIdx = -1;

  for (int i = 0; i < save.dominoesState.trainCount && i < dominoes::kTotalTiles; ++i) {
    int tIdx = 0, p0Pip = 0, p1Pip = 0;
    if (std::sscanf(text, " %d %d %d%n", &tIdx, &p0Pip, &p1Pip, &consumed) < 3) return false;
    text += consumed;
    save.dominoesState.train[i].tileIdx = static_cast<int8_t>(tIdx);
    save.dominoesState.train[i].pips[0] = static_cast<int8_t>(p0Pip);
    save.dominoesState.train[i].pips[1] = static_cast<int8_t>(p1Pip);
  }

  unsigned hc0 = 0;
  if (std::sscanf(text, " %u%n", &hc0, &consumed) < 1) return false;
  text += consumed;
  save.dominoesState.handCount[0] = static_cast<uint8_t>(hc0);
  for (int i = 0; i < save.dominoesState.handCount[0] && i < dominoes::kTotalTiles; ++i) {
    int tIdx = 0;
    if (std::sscanf(text, " %d%n", &tIdx, &consumed) < 1) return false;
    text += consumed;
    save.dominoesState.hand[0][i] = static_cast<int8_t>(tIdx);
  }

  unsigned hc1 = 0;
  if (std::sscanf(text, " %u%n", &hc1, &consumed) < 1) return false;
  text += consumed;
  save.dominoesState.handCount[1] = static_cast<uint8_t>(hc1);
  for (int i = 0; i < save.dominoesState.handCount[1] && i < dominoes::kTotalTiles; ++i) {
    int tIdx = 0;
    if (std::sscanf(text, " %d%n", &tIdx, &consumed) < 1) return false;
    text += consumed;
    save.dominoesState.hand[1][i] = static_cast<int8_t>(tIdx);
  }

  unsigned bc = 0;
  if (std::sscanf(text, " %u%n", &bc, &consumed) < 1) return false;
  text += consumed;
  save.dominoesState.boneyardCount = static_cast<uint8_t>(bc);
  for (int i = 0; i < save.dominoesState.boneyardCount && i < dominoes::kTotalTiles; ++i) {
    int tIdx = 0;
    if (std::sscanf(text, " %d%n", &tIdx, &consumed) < 1) return false;
    text += consumed;
    save.dominoesState.boneyard[i] = static_cast<int8_t>(tIdx);
  }
  if (std::sscanf(text, "\n%n", &consumed) >= 0) text += consumed;

  // Sandbox
  auto& sb = save.sandboxState;
  unsigned sbMode = 0, dCount = 0, dType = 0, dHeld = 0;
  int rollSum = 0;
  if (std::sscanf(text, "SAN %u %u %u %u %d%n",
                  &sbMode, &dCount, &dType, &dHeld, &rollSum, &consumed) < 5) {
    return false;
  }
  text += consumed;
  sb.mode = static_cast<sandbox::ToolMode>(sbMode);
  sb.diceCount = static_cast<uint8_t>(dCount);
  sb.dieType = static_cast<sandbox::DieType>(dType);
  sb.heldMask = static_cast<uint8_t>(dHeld);
  sb.lastRollSum = static_cast<int16_t>(rollSum);

  for (int i = 0; i < sandbox::kMaxDice; ++i) {
    unsigned dVal = 0;
    if (std::sscanf(text, " %u%n", &dVal, &consumed) < 1) return false;
    text += consumed;
    sb.diceValues[i] = static_cast<uint8_t>(dVal);
  }

  unsigned deckC = 0, handC = 0, discTop = 0;
  int handHidden = 0;
  if (std::sscanf(text, " %u %u %d %u%n", &deckC, &handC, &handHidden, &discTop, &consumed) < 4) {
    return false;
  }
  text += consumed;
  sb.deckCount = static_cast<uint8_t>(deckC);
  sb.handCount = static_cast<uint8_t>(handC);
  sb.handHidden = (handHidden != 0);
  sb.discardTop = static_cast<uint8_t>(discTop);

  for (int i = 0; i < sandbox::kMaxHand; ++i) {
    unsigned h = 0;
    if (std::sscanf(text, " %u%n", &h, &consumed) < 1) return false;
    text += consumed;
    sb.hand[i] = static_cast<uint8_t>(h);
  }

  for (int i = 0; i < sandbox::kMaxPlayers; ++i) {
    int sc = 0;
    if (std::sscanf(text, " %d%n", &sc, &consumed) < 1) return false;
    text += consumed;
    sb.scores[i] = static_cast<int16_t>(sc);
  }

  unsigned actP = 0, spColor = 0, tokC = 0;
  if (std::sscanf(text, " %u %u %u%n", &actP, &spColor, &tokC, &consumed) < 3) {
    return false;
  }
  text += consumed;
  sb.activePlayer = static_cast<uint8_t>(actP);
  sb.spawnColor = static_cast<uint8_t>(spColor);
  sb.tokenCount = static_cast<uint8_t>(tokC);
  sb.selectedToken = -1;

  for (int i = 0; i < sb.tokenCount && i < sandbox::kMaxTokens; ++i) {
    unsigned pos = 0, color = 0;
    if (std::sscanf(text, " %u %u%n", &pos, &color, &consumed) < 2) return false;
    text += consumed;
    sb.tokens[i].pos = static_cast<uint8_t>(pos);
    sb.tokens[i].color = static_cast<uint8_t>(color);
  }

  return true;
}

}  // namespace save
}  // namespace tabletop
