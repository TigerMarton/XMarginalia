#include "TabletopActivity.h"

#include <Memory.h>

#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "TabletopSave.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
#include <HalStorage.h>
#include <Logging.h>

constexpr const char* kSavePath = "/.crosspoint/tabletop.cfg";
constexpr const char* kSaveTempPath = "/.crosspoint/tabletop.tmp";
#endif

namespace tt = tabletop;

std::unique_ptr<Activity> TabletopActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TabletopActivity>(renderer, mappedInput);
}

void TabletopActivity::onEnter() {
  LinkActivity::onEnter();
  toybox::ensureFonts(renderer);
  screen = tt::Screen::Lobby;
  lobbySelected = 0;
  lobbyPage = 0;
  startReversi();
  startMancala();
  startMorris();
  startOnitama();
  startLiarsDice();
  startDobutsu();
  startBackgammon();
  startSantorini();
  startQuoridor();
  startDominoes();
  startSandbox();
  loadSave();
  wantsFullRefresh = true;
  requestUpdate();
}

void TabletopActivity::onExit() {
  leaveLink();
  writeSave();
  LinkActivity::onExit();
}

const char* TabletopActivity::linkGameTitle() const {
  switch (screen) {
    case tt::Screen::Reversi: return "REVERSI";
    case tt::Screen::Mancala: return "MANCALA";
    case tt::Screen::Morris: return "NINE MEN'S MORRIS";
    case tt::Screen::Onitama: return "ONITAMA";
    case tt::Screen::LiarsDice: return "LIAR'S DICE";
    case tt::Screen::Dobutsu: return "DOBUTSU SHOGI";
    case tt::Screen::Backgammon: return "BACKGAMMON";
    case tt::Screen::Santorini: return "SANTORINI";
    case tt::Screen::Quoridor: return "QUORIDOR";
    case tt::Screen::Dominoes: return "DOMINOES";
    default: return "TABLETOP";
  }
}

const char* TabletopActivity::linkHeadline() const {
  if (linkPhase() == linkplay::PlayBase::Phase::Searching) {
    return "LOOKING FOR A PLAYER";
  }
  if (matchGameOver()) {
    return "GAME OVER";
  }
  if (linkPhase() == linkplay::PlayBase::Phase::YourTurn) {
    return "YOUR TURN";
  }
  return "OPPONENT'S TURN";
}

void TabletopActivity::onMatchStart(const bool goesFirst) {
  const uint32_t seed = play.matchSeed();
  switch (screen) {
    case tt::Screen::Reversi:
      reversiGame = reversi::Game{};
      reversi::start(reversiGame);
      reversiMode = reversiui::PlayMode::Nearby;
      reversiHumanSeat = goesFirst ? reversi::kBlack : reversi::kWhite;
      reversiFlip = !goesFirst;
      break;
    case tt::Screen::Mancala:
      mancalaGame = mancala::Game{};
      mancala::start(mancalaGame);
      mancalaMode = mancalaui::PlayMode::Nearby;
      mancalaHumanPlayer = goesFirst ? mancala::kPlayer1 : mancala::kPlayer2;
      mancalaFlip = !goesFirst;
      break;
    case tt::Screen::Morris:
      morrisGame = morris::Game{};
      morris::start(morrisGame);
      morrisMode = morrisui::PlayMode::Nearby;
      morrisHumanPlayer = goesFirst ? morris::kWhite : morris::kBlack;
      morrisFlip = !goesFirst;
      break;
    case tt::Screen::Onitama:
      onitamaGame = onitama::initGame(seed);
      onitamaMode = onitamaui::PlayMode::Nearby;
      onitamaHumanPlayer = goesFirst ? onitama::kPlayer1 : onitama::kPlayer2;
      onitamaSelectedCell = -1;
      onitamaSelectedCard = -1;
      onitamaAiPending = false;
      onitamaFlip = !goesFirst;
      break;
    case tt::Screen::LiarsDice:
      liarsdice::initGame(liarsDiceGame, 2, seed);
      liarsDiceMode = liarsdiceui::PlayMode::Nearby;
      liarsDiceHumanPlayer = goesFirst ? 0 : 1;
      liarsDiceSelectedCount = 1;
      liarsDiceSelectedFace = 2;
      liarsDiceAiPending = false;
      liarsDiceShieldDice = false;
      liarsDiceFlip = !goesFirst;
      liarsDiceRound = 0;
      break;
    case tt::Screen::Dobutsu:
      dobutsu::initGame(dobutsuGame);
      dobutsuMode = dobutsuui::PlayMode::Nearby;
      dobutsuHumanPlayer = goesFirst ? dobutsu::Player::P1 : dobutsu::Player::P2;
      dobutsuSelectedSq = -1;
      dobutsuAiPending = false;
      dobutsuFlip = !goesFirst;
      break;
    case tt::Screen::Backgammon:
      backgammon::initGame(backgammonGame);
      backgammonMode = backgammonui::PlayMode::Nearby;
      backgammonHumanPlayer = goesFirst ? backgammon::Player::White : backgammon::Player::Black;
      backgammonSelectedPoint = -99;
      backgammonAiPending = false;
      backgammonFlip = !goesFirst;
      break;
    case tt::Screen::Santorini:
      santorini::initGame(santoriniState);
      santoriniState.mode = santorini::Mode::Nearby;
      santoriniHumanPlayer = goesFirst ? 0 : 1;
      santoriniFlip = !goesFirst;
      santoriniState.flip = santoriniFlip;
      santoriniAiPending = false;
      break;
    case tt::Screen::Quoridor:
      quoridor::initGame(quoridorState);
      quoridorState.mode = quoridor::Mode::Nearby;
      quoridorHumanPlayer = goesFirst ? 0 : 1;
      quoridorFlip = !goesFirst;
      quoridorState.flip = quoridorFlip;
      quoridorAiPending = false;
      break;
    case tt::Screen::Dominoes:
      dominoes::initGame(dominoesState, seed);
      dominoesState.mode = dominoes::Mode::Nearby;
      dominoesHumanPlayer = goesFirst ? dominoes::Player::P1 : dominoes::Player::P2;
      dominoesState.turn = dominoesHumanPlayer;
      dominoesAiPending = false;
      break;
    default:
      break;
  }
  requestUpdate();
}

bool TabletopActivity::takeOpponentState() {
  bool changed = false;
  switch (screen) {
    case tt::Screen::Reversi:
      changed = play.takeOpponentState(reversiGame);
      break;
    case tt::Screen::Mancala:
      changed = play.takeOpponentState(mancalaGame);
      break;
    case tt::Screen::Morris:
      changed = play.takeOpponentState(morrisGame);
      break;
    case tt::Screen::Onitama:
      changed = play.takeOpponentState(onitamaGame);
      break;
    case tt::Screen::LiarsDice:
      changed = play.takeOpponentState(liarsDiceGame);
      break;
    case tt::Screen::Dobutsu:
      changed = play.takeOpponentState(dobutsuGame);
      break;
    case tt::Screen::Backgammon:
      changed = play.takeOpponentState(backgammonGame);
      break;
    case tt::Screen::Santorini:
      changed = play.takeOpponentState(santoriniState);
      if (changed) santoriniState.flip = santoriniFlip;
      break;
    case tt::Screen::Quoridor:
      changed = play.takeOpponentState(quoridorState);
      if (changed) quoridorState.flip = quoridorFlip;
      break;
    case tt::Screen::Dominoes:
      changed = play.takeOpponentState(dominoesState);
      break;
    default:
      break;
  }
  if (changed) {
    requestUpdate();
  }
  return changed;
}

void TabletopActivity::onRematch() {
  onMatchStart(linkState().goesFirst());
}

void TabletopActivity::onLinkEnded() {
  switch (screen) {
    case tt::Screen::Reversi: reversiMode = reversiui::PlayMode::SoloVsAI; break;
    case tt::Screen::Mancala: mancalaMode = mancalaui::PlayMode::SoloVsAI; break;
    case tt::Screen::Morris: morrisMode = morrisui::PlayMode::SoloVsAI; break;
    case tt::Screen::Onitama: onitamaMode = onitamaui::PlayMode::SoloVsAI; break;
    case tt::Screen::LiarsDice: liarsDiceMode = liarsdiceui::PlayMode::SoloVsAI; break;
    case tt::Screen::Dobutsu: dobutsuMode = dobutsuui::PlayMode::SoloVsAI; break;
    case tt::Screen::Backgammon: backgammonMode = backgammonui::PlayMode::SoloVsAI; break;
    case tt::Screen::Santorini: santoriniState.mode = santorini::Mode::Solo; break;
    case tt::Screen::Quoridor: quoridorState.mode = quoridor::Mode::Solo; break;
    case tt::Screen::Dominoes: dominoesState.mode = dominoes::Mode::Solo; break;
    default: break;
  }
  requestUpdate();
}

bool TabletopActivity::matchGameOver() const {
  switch (screen) {
    case tt::Screen::Reversi: return reversiGame.isOver;
    case tt::Screen::Mancala: return mancalaGame.isOver;
    case tt::Screen::Morris: return morrisGame.isOver;
    case tt::Screen::Onitama: return onitamaGame.isOver;
    case tt::Screen::LiarsDice: return liarsDiceGame.phase == liarsdice::Phase::GameOver;
    case tt::Screen::Dobutsu: return dobutsuGame.isOver;
    case tt::Screen::Backgammon: return backgammonGame.isOver;
    case tt::Screen::Santorini: return santoriniState.winner != santorini::Player::None;
    case tt::Screen::Quoridor: return quoridorState.winner != quoridor::Player::None;
    case tt::Screen::Dominoes: return dominoesState.winner != dominoes::Player::None || dominoesState.roundOver;
    default: return false;
  }
}

void TabletopActivity::onMatchEnded() {
  requestUpdate();
}

void TabletopActivity::loadSave() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!Storage.exists(kSavePath)) return;
  char buffer[tabletop::save::kMaxLine] = {};
  if (Storage.readFileToBuffer(kSavePath, buffer, sizeof(buffer)) == 0) return;

  tabletop::save::Save save;
  if (!tabletop::save::unpack(buffer, save)) {
    LOG_ERR("TABLETOP", "Save file parse failed");
    return;
  }
  reversiMode = static_cast<reversiui::PlayMode>(save.reversiMode);
  if (reversiMode == reversiui::PlayMode::Nearby) reversiMode = reversiui::PlayMode::SoloVsAI;
  reversiFlip = save.reversiFlip;
  reversiGame = save.reversiGame;

  mancalaMode = static_cast<mancalaui::PlayMode>(save.mancalaMode);
  if (mancalaMode == mancalaui::PlayMode::Nearby) mancalaMode = mancalaui::PlayMode::SoloVsAI;
  mancalaFlip = save.mancalaFlip;
  mancalaGame = save.mancalaGame;

  morrisMode = static_cast<morrisui::PlayMode>(save.morrisMode);
  if (morrisMode == morrisui::PlayMode::Nearby) morrisMode = morrisui::PlayMode::SoloVsAI;
  morrisFlip = save.morrisFlip;
  morrisGame = save.morrisGame;

  onitamaMode = static_cast<onitamaui::PlayMode>(save.onitamaMode);
  if (onitamaMode == onitamaui::PlayMode::Nearby) onitamaMode = onitamaui::PlayMode::SoloVsAI;
  onitamaFlip = save.onitamaFlip;
  onitamaGame = save.onitamaGame;

  liarsDiceMode = static_cast<liarsdiceui::PlayMode>(save.liarsDiceMode);
  if (liarsDiceMode == liarsdiceui::PlayMode::Nearby) liarsDiceMode = liarsdiceui::PlayMode::SoloVsAI;
  liarsDiceGame = save.liarsDiceGame;

  dobutsuMode = static_cast<dobutsuui::PlayMode>(save.dobutsuMode);
  if (dobutsuMode == dobutsuui::PlayMode::Nearby) dobutsuMode = dobutsuui::PlayMode::SoloVsAI;
  dobutsuFlip = save.dobutsuFlip;
  dobutsuGame = save.dobutsuGame;

  backgammonMode = static_cast<backgammonui::PlayMode>(save.backgammonMode);
  if (backgammonMode == backgammonui::PlayMode::Nearby) backgammonMode = backgammonui::PlayMode::SoloVsAI;
  backgammonFlip = save.backgammonFlip;
  backgammonGame = save.backgammonGame;

  santoriniState = save.santoriniState;
  if (santoriniState.mode == santorini::Mode::Nearby) santoriniState.mode = santorini::Mode::Solo;
  quoridorState = save.quoridorState;
  if (quoridorState.mode == quoridor::Mode::Nearby) quoridorState.mode = quoridor::Mode::Solo;
  dominoesState = save.dominoesState;
  if (dominoesState.mode == dominoes::Mode::Nearby) dominoesState.mode = dominoes::Mode::Solo;

  sandboxState = save.sandboxState;
#endif
}

void TabletopActivity::writeSave() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  tabletop::save::Save save;
  save.lastScreen = static_cast<uint8_t>(screen);
  save.reversiMode = static_cast<uint8_t>(reversiMode == reversiui::PlayMode::Nearby ? reversiui::PlayMode::SoloVsAI : reversiMode);
  save.reversiFlip = reversiFlip;
  save.reversiGame = reversiGame;

  save.mancalaMode = static_cast<uint8_t>(mancalaMode == mancalaui::PlayMode::Nearby ? mancalaui::PlayMode::SoloVsAI : mancalaMode);
  save.mancalaFlip = mancalaFlip;
  save.mancalaGame = mancalaGame;

  save.morrisMode = static_cast<uint8_t>(morrisMode == morrisui::PlayMode::Nearby ? morrisui::PlayMode::SoloVsAI : morrisMode);
  save.morrisFlip = morrisFlip;
  save.morrisGame = morrisGame;

  save.onitamaMode = static_cast<uint8_t>(onitamaMode == onitamaui::PlayMode::Nearby ? onitamaui::PlayMode::SoloVsAI : onitamaMode);
  save.onitamaFlip = onitamaFlip;
  save.onitamaGame = onitamaGame;

  save.liarsDiceMode = static_cast<uint8_t>(liarsDiceMode == liarsdiceui::PlayMode::Nearby ? liarsdiceui::PlayMode::SoloVsAI : liarsDiceMode);
  save.liarsDiceGame = liarsDiceGame;

  save.dobutsuMode = static_cast<uint8_t>(dobutsuMode == dobutsuui::PlayMode::Nearby ? dobutsuui::PlayMode::SoloVsAI : dobutsuMode);
  save.dobutsuFlip = dobutsuFlip;
  save.dobutsuGame = dobutsuGame;

  save.backgammonMode = static_cast<uint8_t>(backgammonMode == backgammonui::PlayMode::Nearby ? backgammonui::PlayMode::SoloVsAI : backgammonMode);
  save.backgammonFlip = backgammonFlip;
  save.backgammonGame = backgammonGame;

  save.santoriniState = santoriniState;
  if (save.santoriniState.mode == santorini::Mode::Nearby) save.santoriniState.mode = santorini::Mode::Solo;
  save.quoridorState = quoridorState;
  if (save.quoridorState.mode == quoridor::Mode::Nearby) save.quoridorState.mode = quoridor::Mode::Solo;
  save.dominoesState = dominoesState;
  if (save.dominoesState.mode == dominoes::Mode::Nearby) save.dominoesState.mode = dominoes::Mode::Solo;

  save.sandboxState = sandboxState;

  char line[tabletop::save::kMaxLine];
  const int bytes = tabletop::save::pack(save, line, sizeof(line));
  if (bytes <= 0) {
    LOG_ERR("TABLETOP", "Save line did not fit %d bytes", static_cast<int>(sizeof(line)));
    return;
  }

  Storage.writeFile(kSaveTempPath, String(line));
  Storage.remove(kSavePath);
  Storage.rename(kSaveTempPath, kSavePath);
#endif
}

void TabletopActivity::startReversi() {
  reversi::start(reversiGame);
  reversiHumanSeat = reversi::kBlack;
  reversiAiPending = false;
}

void TabletopActivity::stepReversiAi() {
  if (reversiGame.isOver) return;
  const reversi::BestMove move = reversi::findBestMove(reversiGame, reversiGame.turn, reversi::Difficulty::Standard);
  if (move.col >= 0) {
    reversi::playMove(reversiGame, move.col, move.row);
  } else {
    reversi::passTurn(reversiGame);
  }
  reversiAiPending = false;
  requestUpdate();
}

void TabletopActivity::startMancala() {
  mancala::start(mancalaGame);
  mancalaHumanPlayer = mancala::kPlayer1;
  mancalaAiPending = false;
}

void TabletopActivity::stepMancalaAi() {
  if (mancalaGame.isOver) return;
  const mancala::BestMove move = mancala::findBestMove(mancalaGame, mancalaGame.turn, mancala::Difficulty::Standard);
  if (move.pit >= 0) {
    mancala::playMove(mancalaGame, move.pit);
  }
  mancalaAiPending = false;
  requestUpdate();
}

void TabletopActivity::startMorris() {
  morris::start(morrisGame);
  morrisHumanPlayer = morris::kWhite;
  morrisAiPending = false;
}

void TabletopActivity::stepMorrisAi() {
  if (morrisGame.isOver) return;
  const morris::BestMove move = morris::findBestMove(morrisGame, morrisGame.turn, morris::Difficulty::Standard);
  if (move.kind == morris::BestMove::Place) {
    morris::placePiece(morrisGame, move.to);
  } else if (move.kind == morris::BestMove::Slide) {
    morris::movePiece(morrisGame, move.from, move.to);
  } else if (move.kind == morris::BestMove::Remove) {
    morris::removePiece(morrisGame, move.to);
  }
  morrisAiPending = false;
  requestUpdate();
}

void TabletopActivity::startOnitama() {
  onitamaGame = onitama::initGame();
  onitamaHumanPlayer = onitama::kPlayer1;
  onitamaSelectedCell = -1;
  onitamaSelectedCard = -1;
  onitamaAiPending = false;
}

void TabletopActivity::stepOnitamaAi() {
  if (onitamaGame.isOver) return;
  const onitama::Move move = onitama::chooseMove(onitamaGame, onitama::Difficulty::Standard);
  onitama::applyMove(onitamaGame, move);
  onitamaAiPending = false;
  requestUpdate();
}

void TabletopActivity::startLiarsDice() {
  liarsdice::initGame(liarsDiceGame, 2, static_cast<uint32_t>(millis()));
  liarsDiceHumanPlayer = 0;
  liarsDiceSelectedCount = 1;
  liarsDiceSelectedFace = 2;
  liarsDiceAiPending = false;
  liarsDiceShieldDice = false;
}

void TabletopActivity::stepLiarsDiceAi() {
  if (liarsDiceGame.phase != liarsdice::Phase::Bidding) return;
  if (liarsDiceGame.activePlayer == 0 && liarsDiceMode == liarsdiceui::PlayMode::SoloVsAI) return;

  const auto decision = liarsdice::decideBotMove(liarsDiceGame, liarsDiceGame.activePlayer, static_cast<uint32_t>(millis()));
  if (decision.action == liarsdice::BotActionType::Challenge) {
    liarsdice::challenge(liarsDiceGame);
  } else {
    liarsdice::makeBid(liarsDiceGame, decision.bid);
    if (liarsDiceMode == liarsdiceui::PlayMode::SoloVsAI && liarsDiceGame.activePlayer != 0) {
      liarsDiceAiPending = true;
      requestUpdate();
      return;
    }
  }
  liarsDiceAiPending = false;
  requestUpdate();
}

void TabletopActivity::startDobutsu() {
  dobutsu::initGame(dobutsuGame);
  dobutsuHumanPlayer = dobutsu::Player::P1;
  dobutsuSelectedSq = -1;
  dobutsuAiPending = false;
}

void TabletopActivity::stepDobutsuAi() {
  if (dobutsuGame.isOver) return;
  const dobutsu::Move move = dobutsu::chooseBotMove(dobutsuGame, dobutsu::Difficulty::Standard, static_cast<uint32_t>(millis()));
  dobutsu::applyMove(dobutsuGame, move);
  dobutsuAiPending = false;
  requestUpdate();
}

void TabletopActivity::startBackgammon() {
  backgammon::initGame(backgammonGame);
  backgammonHumanPlayer = backgammon::Player::White;
  backgammonSelectedPoint = -99;
  backgammonAiPending = false;
}

void TabletopActivity::stepBackgammonAi() {
  if (backgammonGame.isOver) return;

  if (!backgammonGame.isRolled) {
    backgammon::rollDice(backgammonGame, static_cast<uint32_t>(millis()));
    backgammon::Move moves[64];
    if (backgammon::generateLegalMoves(backgammonGame, moves, 64) == 0) {
      backgammon::passTurn(backgammonGame);
    }
  } else {
    const backgammon::Move move = backgammon::chooseBotMove(backgammonGame, backgammon::Difficulty::Standard, static_cast<uint32_t>(millis()));
    backgammon::applyMove(backgammonGame, move);
  }

  if (backgammonMode == backgammonui::PlayMode::SoloVsAI && !backgammonGame.isOver && backgammonGame.turn != backgammonHumanPlayer) {
    backgammonAiPending = true;
  } else {
    backgammonAiPending = false;
  }
  requestUpdate();
}

void TabletopActivity::startSantorini() {
  santorini::initGame(santoriniState);
  santoriniHumanPlayer = 0;
  santoriniAiPending = false;
}

void TabletopActivity::stepSantoriniAi() {
  if (santoriniState.winner != santorini::Player::None || santoriniState.phase == santorini::Phase::GameOver) {
    santoriniAiPending = false;
    return;
  }
  santorini::Move move{};
  if (santorini::SantoriniBrain::findBestMove(santoriniState, move, 3)) {
    santorini::executeFullMove(santoriniState, move);
  }
  santoriniAiPending = false;
  requestUpdate();
}

void TabletopActivity::startQuoridor() {
  quoridor::initGame(quoridorState);
  quoridorHumanPlayer = 0;
  quoridorAiPending = false;
}

void TabletopActivity::stepQuoridorAi() {
  if (quoridorState.winner != quoridor::Player::None) {
    quoridorAiPending = false;
    return;
  }
  quoridor::Move move{};
  if (quoridor::QuoridorBrain::findBestMove(quoridorState, move, 2)) {
    quoridor::executeMove(quoridorState, move);
  }
  quoridorAiPending = false;
  requestUpdate();
}

void TabletopActivity::startDominoes() {
  dominoes::initGame(dominoesState, static_cast<uint32_t>(millis()));
  dominoesAiPending = false;
}

void TabletopActivity::stepDominoesAi() {
  if (dominoesState.winner != dominoes::Player::None || dominoesState.roundOver) {
    dominoesAiPending = false;
    return;
  }
  dominoes::Move move{};
  if (dominoes::DominoesBrain::findBestMove(dominoesState, move)) {
    dominoes::playTile(dominoesState, move.handIdx, move.side);
    dominoesAiPending = false;
  } else if (dominoes::DominoesBrain::shouldDraw(dominoesState)) {
    dominoes::drawFromBoneyard(dominoesState);
    if (dominoesState.mode == dominoes::Mode::Solo && dominoesState.turn == dominoes::Player::P2 &&
        dominoesState.winner == dominoes::Player::None && !dominoesState.roundOver) {
      dominoesAiPending = true;
      requestUpdate();
      return;
    }
    dominoesAiPending = false;
  } else {
    dominoes::passTurn(dominoesState);
    dominoesAiPending = false;
  }
  requestUpdate();
}

void TabletopActivity::startSandbox() {
  sandboxState = sandbox::makeDefault();
}

uint32_t TabletopActivity::surfaceMeaning() const {
  uint32_t meaning = static_cast<uint32_t>(screen);
  if (screen == tt::Screen::Reversi) {
    meaning = (meaning << 8) | static_cast<uint32_t>(reversiGame.turn);
    if (reversiAiPending) meaning |= 0x10000;
    if (reversiFlip) meaning |= 0x40000;
  } else if (screen == tt::Screen::Mancala) {
    meaning = (meaning << 8) | static_cast<uint32_t>(mancalaGame.turn);
    if (mancalaAiPending) meaning |= 0x10000;
    if (mancalaFlip) meaning |= 0x40000;
  } else if (screen == tt::Screen::Morris) {
    meaning = (meaning << 8) | static_cast<uint32_t>(morrisGame.turn);
    if (morrisAiPending) meaning |= 0x10000;
    if (morrisGame.pendingRemoval) meaning |= 0x20000;
    if (morrisFlip) meaning |= 0x40000;
  } else if (screen == tt::Screen::Onitama) {
    meaning = (meaning << 8) | static_cast<uint32_t>(onitamaGame.turn);
    if (onitamaAiPending) meaning |= 0x10000;
    if (onitamaFlip) meaning |= 0x40000;
    if (onitamaSelectedCard >= 0) meaning |= (static_cast<uint32_t>(onitamaSelectedCard + 1) << 12);
    if (onitamaSelectedCell >= 0) meaning |= (static_cast<uint32_t>(onitamaSelectedCell + 1) << 16);
  } else if (screen == tt::Screen::LiarsDice) {
    meaning = (meaning << 8) | static_cast<uint32_t>(liarsDiceGame.phase);
    meaning = (meaning << 8) | static_cast<uint32_t>(liarsDiceGame.currentBid.count);
    meaning = (meaning << 8) | static_cast<uint32_t>(liarsDiceGame.currentBid.face);
    if (liarsDiceAiPending) meaning |= 0x10000;
    if (liarsDiceShieldDice) meaning |= 0x20000;
  } else if (screen == tt::Screen::Dobutsu) {
    meaning = (meaning << 8) | static_cast<uint32_t>(dobutsuGame.turn);
    if (dobutsuAiPending) meaning |= 0x10000;
    if (dobutsuFlip) meaning |= 0x40000;
    if (dobutsuSelectedSq >= 0) meaning |= (static_cast<uint32_t>(dobutsuSelectedSq + 1) << 12);
  } else if (screen == tt::Screen::Backgammon) {
    meaning = (meaning << 8) | static_cast<uint32_t>(backgammonGame.turn);
    meaning = (meaning << 8) | static_cast<uint32_t>(backgammonGame.isRolled ? 1 : 0);
    meaning = (meaning << 8) | static_cast<uint32_t>(backgammonGame.dice[0] * 10 + backgammonGame.dice[1]);
    if (backgammonAiPending) meaning |= 0x10000;
    if (backgammonFlip) meaning |= 0x40000;
    if (backgammonSelectedPoint != -99) meaning |= (static_cast<uint32_t>(backgammonSelectedPoint + 100) << 12);
  } else if (screen == tt::Screen::Santorini) {
    meaning = (meaning << 8) | static_cast<uint32_t>(santoriniState.turn);
    meaning = (meaning << 8) | static_cast<uint32_t>(santoriniState.phase);
    meaning = (meaning << 8) | static_cast<uint32_t>(santoriniState.selectedWorker + 1);
    if (santoriniAiPending) meaning |= 0x10000;
    if (santoriniState.flip) meaning |= 0x40000;
  } else if (screen == tt::Screen::Quoridor) {
    meaning = (meaning << 8) | static_cast<uint32_t>(quoridorState.turn);
    meaning = (meaning << 8) | static_cast<uint32_t>(quoridorState.pawn[0]);
    meaning = (meaning << 8) | static_cast<uint32_t>(quoridorState.pawn[1]);
    meaning = (meaning << 8) | static_cast<uint32_t>(quoridorState.selectedGx + 1);
    if (quoridorAiPending) meaning |= 0x10000;
    if (quoridorState.flip) meaning |= 0x40000;
  } else if (screen == tt::Screen::Dominoes) {
    meaning = (meaning << 8) | static_cast<uint32_t>(dominoesState.turn);
    meaning = (meaning << 8) | static_cast<uint32_t>(dominoesState.trainCount);
    meaning = (meaning << 8) | static_cast<uint32_t>(dominoesState.selectedHandIdx + 1);
    if (dominoesAiPending) meaning |= 0x10000;
    if (dominoesState.privacyConcealed) meaning |= 0x20000;
  } else if (screen == tt::Screen::Sandbox) {
    meaning = (meaning << 8) | static_cast<uint32_t>(sandboxState.mode);
    meaning = (meaning << 8) | static_cast<uint32_t>(sandboxState.lastRollSum & 0xFF);
    if (sandboxState.handHidden) meaning |= 0x10000;
  }
  return meaning;
}

void TabletopActivity::goTo(const tt::Screen next) {
  if (screen == next) return;
  if (screen != tt::Screen::Lobby && screen != tt::Screen::About) {
    writeSave();
  }
  if (next == tt::Screen::Lobby) {
    leaveLink();
  }
  screen = next;
  if (next == tt::Screen::Lobby) {
    lobbyPage = lobbySelected / tabletopui::kRowsPerPage;
    if (lobbyPage < 0) lobbyPage = 0;
    if (lobbyPage >= tabletopui::kPageCount) lobbyPage = tabletopui::kPageCount - 1;
  }
  requestUpdate();
}

void TabletopActivity::gameLoop() {
  namespace fui = freeink::ui;

  if (screen == tt::Screen::Reversi) {
    if (reversiMode == reversiui::PlayMode::SoloVsAI && !reversiGame.isOver &&
        reversiGame.turn != reversiHumanSeat) {
      if (reversiAiPending) {
        stepReversiAi();
        return;
      } else {
        reversiAiPending = true;
        requestUpdate();
        return;
      }
    }
  } else if (screen == tt::Screen::Mancala) {
    if (mancalaMode == mancalaui::PlayMode::SoloVsAI && !mancalaGame.isOver &&
        mancalaGame.turn != mancalaHumanPlayer) {
      if (mancalaAiPending) {
        stepMancalaAi();
        return;
      } else {
        mancalaAiPending = true;
        requestUpdate();
        return;
      }
    }
  } else if (screen == tt::Screen::Morris) {
    if (morrisMode == morrisui::PlayMode::SoloVsAI && !morrisGame.isOver &&
        morrisGame.turn != morrisHumanPlayer) {
      if (morrisAiPending) {
        stepMorrisAi();
        return;
      } else {
        morrisAiPending = true;
        requestUpdate();
        return;
      }
    }
  } else if (screen == tt::Screen::Onitama) {
    if (onitamaMode == onitamaui::PlayMode::SoloVsAI && !onitamaGame.isOver &&
        onitamaGame.turn != onitamaHumanPlayer) {
      if (onitamaAiPending) {
        stepOnitamaAi();
        return;
      } else {
        onitamaAiPending = true;
        requestUpdate();
        return;
      }
    }
  } else if (screen == tt::Screen::LiarsDice) {
    if (liarsDiceMode == liarsdiceui::PlayMode::SoloVsAI &&
        liarsDiceGame.phase == liarsdice::Phase::Bidding &&
        liarsDiceGame.activePlayer != liarsDiceHumanPlayer) {
      if (liarsDiceAiPending) {
        stepLiarsDiceAi();
        return;
      } else {
        liarsDiceAiPending = true;
        requestUpdate();
        return;
      }
    }
  } else if (screen == tt::Screen::Dobutsu) {
    if (dobutsuMode == dobutsuui::PlayMode::SoloVsAI &&
        !dobutsuGame.isOver &&
        dobutsuGame.turn != dobutsuHumanPlayer) {
      if (dobutsuAiPending) {
        stepDobutsuAi();
        return;
      } else {
        dobutsuAiPending = true;
        requestUpdate();
        return;
      }
    }
  } else if (screen == tt::Screen::Backgammon) {
    if (backgammonMode == backgammonui::PlayMode::SoloVsAI &&
        !backgammonGame.isOver &&
        backgammonGame.turn != backgammonHumanPlayer) {
      if (backgammonAiPending) {
        stepBackgammonAi();
        return;
      } else {
        backgammonAiPending = true;
        requestUpdate();
        return;
      }
    }
  } else if (screen == tt::Screen::Santorini) {
    if (santoriniState.mode == santorini::Mode::Solo &&
        santoriniState.winner == santorini::Player::None &&
        santoriniState.phase != santorini::Phase::GameOver &&
        santoriniState.turn == santorini::Player::Black) {
      if (santoriniAiPending) {
        stepSantoriniAi();
        return;
      } else {
        santoriniAiPending = true;
        requestUpdate();
        return;
      }
    }
  } else if (screen == tt::Screen::Quoridor) {
    if (quoridorState.mode == quoridor::Mode::Solo &&
        quoridorState.winner == quoridor::Player::None &&
        quoridorState.turn == quoridor::Player::Black) {
      if (quoridorAiPending) {
        stepQuoridorAi();
        return;
      } else {
        quoridorAiPending = true;
        requestUpdate();
        return;
      }
    }
  } else if (screen == tt::Screen::Dominoes) {
    if (dominoesState.mode == dominoes::Mode::Solo &&
        dominoesState.winner == dominoes::Player::None &&
        !dominoesState.roundOver &&
        dominoesState.turn == dominoes::Player::P2) {
      if (dominoesAiPending) {
        stepDominoesAi();
        return;
      } else {
        dominoesAiPending = true;
        requestUpdate();
        return;
      }
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (tt::leavesApp(screen)) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    goTo(tt::back(screen));
    return;
  }

  if (screen == tt::Screen::Lobby) {
    const bool keyNext = mappedInput.wasReleased(MappedInputManager::Button::Down) ||
                         mappedInput.wasReleased(MappedInputManager::Button::Right) ||
                         mappedInput.wasReleased(MappedInputManager::Button::PageForward);
    const bool keyPrev = mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                         mappedInput.wasReleased(MappedInputManager::Button::Left) ||
                         mappedInput.wasReleased(MappedInputManager::Button::PageBack);
    const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
    const bool next = keyNext || swipe == MappedInputManager::SwipeDir::Up;
    const bool prev = keyPrev || swipe == MappedInputManager::SwipeDir::Down;

    if (next && lobbyPage + 1 < tabletopui::kPageCount) {
      ++lobbyPage;
      lobbySelected = lobbyPage * tabletopui::kRowsPerPage;
      requestUpdate();
      return;
    }
    if (prev && lobbyPage > 0) {
      --lobbyPage;
      lobbySelected = lobbyPage * tabletopui::kRowsPerPage;
      requestUpdate();
      return;
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      const auto row = static_cast<tt::LobbyRow>(lobbySelected);
      switch (row) {
        case tt::LobbyRow::Reversi: goTo(tt::Screen::Reversi); return;
        case tt::LobbyRow::Mancala: goTo(tt::Screen::Mancala); return;
        case tt::LobbyRow::Morris: goTo(tt::Screen::Morris); return;
        case tt::LobbyRow::Onitama: startOnitama(); goTo(tt::Screen::Onitama); return;
        case tt::LobbyRow::LiarsDice: startLiarsDice(); goTo(tt::Screen::LiarsDice); return;
        case tt::LobbyRow::Dobutsu: startDobutsu(); goTo(tt::Screen::Dobutsu); return;
        case tt::LobbyRow::Backgammon: startBackgammon(); goTo(tt::Screen::Backgammon); return;
        case tt::LobbyRow::Santorini: startSantorini(); goTo(tt::Screen::Santorini); return;
        case tt::LobbyRow::Quoridor: startQuoridor(); goTo(tt::Screen::Quoridor); return;
        case tt::LobbyRow::Dominoes: startDominoes(); goTo(tt::Screen::Dominoes); return;
        case tt::LobbyRow::Sandbox: goTo(tt::Screen::Sandbox); return;
        case tt::LobbyRow::About: goTo(tt::Screen::About); return;
        case tt::LobbyRow::Count: break;
      }
    }
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;

  // Board hit-testing
  if (screen == tt::Screen::Reversi) {
    if (surfaceRevealed()) {
      int col = -1;
      int row = -1;
      if (reversiui::cellAt(tapX, tapY, col, row, reversiFlip) >= 0) {
        const bool canMove = (reversiMode == reversiui::PlayMode::TwoPlayerPassAndPlay) ||
                             (reversiMode == reversiui::PlayMode::Nearby && reversiGame.turn == reversiHumanSeat &&
                              linkPhase() == linkplay::PlayBase::Phase::YourTurn) ||
                             (reversiMode == reversiui::PlayMode::SoloVsAI && reversiGame.turn == reversiHumanSeat);
        if (canMove && !reversiAiPending && !reversiGame.isOver) {
          if (reversi::playMove(reversiGame, col, row)) {
            if (reversiMode == reversiui::PlayMode::Nearby) {
              play.playState(reversiGame);
            }
            requestUpdate();
            return;
          }
        }
      }
    }
  } else if (screen == tt::Screen::Mancala) {
    if (surfaceRevealed()) {
      const int pit = mancalaui::pitAt(tapX, tapY, mancalaFlip);
      if (pit >= 0) {
        const bool canMove = (mancalaMode == mancalaui::PlayMode::TwoPlayerPassAndPlay) ||
                             (mancalaMode == mancalaui::PlayMode::Nearby && mancalaGame.turn == mancalaHumanPlayer &&
                              linkPhase() == linkplay::PlayBase::Phase::YourTurn) ||
                             (mancalaMode == mancalaui::PlayMode::SoloVsAI && mancalaGame.turn == mancalaHumanPlayer);
        if (canMove && !mancalaAiPending && !mancalaGame.isOver) {
          if (mancala::playMove(mancalaGame, pit)) {
            if (mancalaMode == mancalaui::PlayMode::Nearby && (!mancalaGame.extraTurn || mancalaGame.isOver)) {
              play.playState(mancalaGame);
            }
            requestUpdate();
            return;
          }
        }
      }
    }
  } else if (screen == tt::Screen::Morris) {
    if (surfaceRevealed()) {
      const int v = morrisui::vertexAt(tapX, tapY, morrisFlip);
      if (v >= 0) {
        const bool canMove = (morrisMode == morrisui::PlayMode::TwoPlayerPassAndPlay) ||
                             (morrisMode == morrisui::PlayMode::Nearby && morrisGame.turn == morrisHumanPlayer &&
                              linkPhase() == linkplay::PlayBase::Phase::YourTurn) ||
                             (morrisMode == morrisui::PlayMode::SoloVsAI && morrisGame.turn == morrisHumanPlayer);
        if (canMove && !morrisAiPending && !morrisGame.isOver) {
          if (morrisGame.pendingRemoval) {
            if (morris::removePiece(morrisGame, v)) {
              if (morrisMode == morrisui::PlayMode::Nearby) {
                play.playState(morrisGame);
              }
              requestUpdate();
              return;
            }
          } else {
            const morris::Phase phase = morris::playerPhase(morrisGame, morrisGame.turn);
            if (phase == morris::Phase::Placing) {
              if (morris::placePiece(morrisGame, v)) {
                if (morrisMode == morrisui::PlayMode::Nearby && !morrisGame.pendingRemoval) {
                  play.playState(morrisGame);
                }
                requestUpdate();
                return;
              }
            } else {
              // Moving or Flying
              if (morris::pieceAt(morrisGame, v) == morrisGame.turn) {
                morrisGame.selected = (morrisGame.selected == v) ? -1 : static_cast<int8_t>(v);
                requestUpdate();
                return;
              } else if (morrisGame.selected >= 0) {
                if (morris::movePiece(morrisGame, morrisGame.selected, v)) {
                  if (morrisMode == morrisui::PlayMode::Nearby && !morrisGame.pendingRemoval) {
                    play.playState(morrisGame);
                  }
                  requestUpdate();
                  return;
                }
              }
            }
          }
        }
      }
    }
  } else if (screen == tt::Screen::Onitama) {
    if (surfaceRevealed()) {
      const bool canAct = (onitamaMode == onitamaui::PlayMode::TwoPlayerPassAndPlay) ||
                          (onitamaMode == onitamaui::PlayMode::Nearby && onitamaGame.turn == onitamaHumanPlayer &&
                           linkPhase() == linkplay::PlayBase::Phase::YourTurn) ||
                          (onitamaMode == onitamaui::PlayMode::SoloVsAI && onitamaGame.turn == onitamaHumanPlayer);
      if (canAct && !onitamaAiPending && !onitamaGame.isOver) {
        // First check card selection
        const int c = onitamaui::cardAt(tapX, tapY);
        if (c >= 0) {
          onitamaSelectedCard = static_cast<int8_t>(c);
          requestUpdate();
          return;
        }

        // Then check board cell selection
        const int cell = onitamaui::cellAt(tapX, tapY, onitamaFlip);
        if (cell >= 0) {
          if (onitamaSelectedCell >= 0 && onitamaSelectedCard >= 0) {
            onitama::Move move = {static_cast<uint8_t>(onitamaSelectedCell),
                                  static_cast<uint8_t>(cell),
                                  static_cast<uint8_t>(onitamaSelectedCard)};
            if (onitama::applyMove(onitamaGame, move)) {
              onitamaSelectedCell = -1;
              onitamaSelectedCard = -1;
              if (onitamaMode == onitamaui::PlayMode::Nearby) {
                play.playState(onitamaGame);
              }
              requestUpdate();
              return;
            }
          }

          const uint8_t p = onitamaGame.cells[cell];
          const bool isMine = (onitamaGame.turn == onitama::kPlayer1) ? onitama::isP1(p) : onitama::isP2(p);
          if (isMine) {
            onitamaSelectedCell = (onitamaSelectedCell == cell) ? -1 : static_cast<int8_t>(cell);
            if (onitamaSelectedCard < 0) onitamaSelectedCard = 0;
            requestUpdate();
            return;
          }
        }
      }
    }
  } else if (screen == tt::Screen::Dobutsu) {
    if (surfaceRevealed()) {
      const bool canAct = (dobutsuMode == dobutsuui::PlayMode::TwoPlayerPassAndPlay) ||
                          (dobutsuMode == dobutsuui::PlayMode::Nearby && dobutsuGame.turn == dobutsuHumanPlayer &&
                           linkPhase() == linkplay::PlayBase::Phase::YourTurn) ||
                          (dobutsuMode == dobutsuui::PlayMode::SoloVsAI && dobutsuGame.turn == dobutsuHumanPlayer);
      if (canAct && !dobutsuAiPending && !dobutsuGame.isOver) {
        const int cell = dobutsuui::cellAt(tapX, tapY, dobutsuFlip);
        if (cell >= 0) {
          const int8_t targetSq = static_cast<int8_t>(cell);
          if (dobutsuSelectedSq != -1) {
            const dobutsu::Move move{dobutsuSelectedSq, targetSq, false};
            if (dobutsu::isLegalMove(dobutsuGame, move)) {
              dobutsu::applyMove(dobutsuGame, move);
              dobutsuSelectedSq = -1;
              if (dobutsuMode == dobutsuui::PlayMode::Nearby) {
                play.playState(dobutsuGame);
              } else if (dobutsuMode == dobutsuui::PlayMode::SoloVsAI && !dobutsuGame.isOver &&
                  dobutsuGame.turn != dobutsuHumanPlayer) {
                dobutsuAiPending = true;
              }
              requestUpdate();
              return;
            }
          }

          const dobutsu::Cell cellPiece = dobutsuGame.board[targetSq];
          if (cellPiece != 0 && dobutsu::pieceOwner(cellPiece) == dobutsuGame.turn) {
            dobutsuSelectedSq = (dobutsuSelectedSq == targetSq) ? -1 : targetSq;
            requestUpdate();
            return;
          }

          dobutsuSelectedSq = -1;
          requestUpdate();
          return;
        }
      }
    }
  } else if (screen == tt::Screen::Backgammon) {
    if (surfaceRevealed()) {
      const bool canAct = (backgammonMode == backgammonui::PlayMode::TwoPlayerPassAndPlay) ||
                          (backgammonMode == backgammonui::PlayMode::Nearby && backgammonGame.turn == backgammonHumanPlayer &&
                           linkPhase() == linkplay::PlayBase::Phase::YourTurn) ||
                          (backgammonMode == backgammonui::PlayMode::SoloVsAI && backgammonGame.turn == backgammonHumanPlayer);
      if (canAct && !backgammonAiPending && !backgammonGame.isOver && backgammonGame.isRolled) {
        const int8_t pt = backgammonui::pointAt(tapX, tapY, backgammonFlip);
        if (pt != -99) {
          if (backgammonSelectedPoint != -99) {
            backgammon::Move legalMoves[64];
            const size_t nMoves = backgammon::generateLegalMoves(backgammonGame, legalMoves, 64);
            for (size_t mi = 0; mi < nMoves; ++mi) {
              if (legalMoves[mi].from == backgammonSelectedPoint && legalMoves[mi].to == pt) {
                backgammon::applyMove(backgammonGame, legalMoves[mi]);
                backgammonSelectedPoint = -99;
                if (backgammonMode == backgammonui::PlayMode::Nearby) {
                  if (backgammonGame.turn != backgammonHumanPlayer || backgammonGame.isOver) {
                    play.playState(backgammonGame);
                  }
                } else if (backgammonMode == backgammonui::PlayMode::SoloVsAI && !backgammonGame.isOver &&
                    backgammonGame.turn != backgammonHumanPlayer) {
                  backgammonAiPending = true;
                }
                requestUpdate();
                return;
              }
            }
          }

          const int pIdx = static_cast<int>(backgammonGame.turn);
          if (pt == backgammon::kFromBar) {
            if (backgammonGame.bar[pIdx] > 0) {
              backgammonSelectedPoint = (backgammonSelectedPoint == backgammon::kFromBar) ? -99 : backgammon::kFromBar;
              requestUpdate();
              return;
            }
          } else if (pt >= 0 && pt < backgammon::kPoints) {
            const bool friendly = (backgammonGame.turn == backgammon::Player::White)
                                      ? (backgammonGame.board[pt] > 0)
                                      : (backgammonGame.board[pt] < 0);
            if (friendly && backgammonGame.bar[pIdx] == 0) {
              backgammonSelectedPoint = (backgammonSelectedPoint == pt) ? -99 : pt;
              requestUpdate();
              return;
            }
          }

          backgammonSelectedPoint = -99;
          requestUpdate();
          return;
        }
      }
    }
  } else if (screen == tt::Screen::Santorini) {
    if (surfaceRevealed()) {
      const bool canAct = (santoriniState.mode == santorini::Mode::TwoPlayer) ||
                          (santoriniState.mode == santorini::Mode::Nearby &&
                           santoriniState.turn == static_cast<santorini::Player>(santoriniHumanPlayer) &&
                           linkYourTurn()) ||
                          (santoriniState.mode == santorini::Mode::Solo &&
                           santoriniState.turn == santorini::Player::White);
      if (canAct && !santoriniAiPending && santoriniState.winner == santorini::Player::None) {
        const int8_t cell = santoriniui::cellAt(static_cast<int16_t>(tapX), static_cast<int16_t>(tapY), santoriniState.flip);
        if (cell >= 0) {
          if (santoriniState.phase == santorini::Phase::SelectWorker) {
            santorini::Player owner = santorini::Player::None;
            int w = santorini::workerAt(santoriniState, cell, owner);
            if (owner == santoriniState.turn && w != -1) {
              if (santorini::selectWorker(santoriniState, w)) {
                requestUpdate();
                return;
              }
            }
          } else if (santoriniState.phase == santorini::Phase::SelectMove) {
            santorini::Player owner = santorini::Player::None;
            int w = santorini::workerAt(santoriniState, cell, owner);
            if (owner == santoriniState.turn && w != -1) {
              if (santorini::selectWorker(santoriniState, w)) {
                requestUpdate();
                return;
              }
            } else if (santorini::isLegalMove(santoriniState, santoriniState.selectedWorker, cell)) {
              if (santorini::executeMove(santoriniState, cell)) {
                if (santoriniState.winner != santorini::Player::None && santoriniState.mode == santorini::Mode::Nearby) {
                  play.playState(santoriniState);
                }
                requestUpdate();
                return;
              }
            }
          } else if (santoriniState.phase == santorini::Phase::SelectBuild) {
            if (santorini::isLegalBuild(santoriniState, cell)) {
              if (santorini::executeBuild(santoriniState, cell)) {
                if (santoriniState.mode == santorini::Mode::Nearby) {
                  play.playState(santoriniState);
                } else if (santoriniState.mode == santorini::Mode::Solo && santoriniState.turn == santorini::Player::Black &&
                    santoriniState.winner == santorini::Player::None) {
                  santoriniAiPending = true;
                }
                requestUpdate();
                return;
              }
            }
          }
        }
      }
    }
  } else if (screen == tt::Screen::Quoridor) {
    if (surfaceRevealed()) {
      const bool canAct = (quoridorState.mode == quoridor::Mode::TwoPlayer) ||
                          (quoridorState.mode == quoridor::Mode::Nearby &&
                           quoridorState.turn == static_cast<quoridor::Player>(quoridorHumanPlayer) &&
                           linkYourTurn()) ||
                          (quoridorState.mode == quoridor::Mode::Solo &&
                           quoridorState.turn == quoridor::Player::White);
      if (canAct && !quoridorAiPending && quoridorState.winner == quoridor::Player::None) {
        int8_t gx = -1, gy = -1;
        if (quoridorui::grooveAt(static_cast<int16_t>(tapX), static_cast<int16_t>(tapY), gx, gy, quoridorState.flip)) {
          if (quoridorState.wallsLeft[static_cast<int>(quoridorState.turn)] > 0) {
            if (quoridor::canPlaceWall(quoridorState, gx, gy, quoridorState.curOrient)) {
              quoridorState.selectedGx = gx;
              quoridorState.selectedGy = gy;
              requestUpdate();
              return;
            }
          }
        }
        const int8_t cell = quoridorui::cellAt(static_cast<int16_t>(tapX), static_cast<int16_t>(tapY), quoridorState.flip);
        if (cell >= 0) {
          if (quoridor::isLegalPawnMove(quoridorState, cell)) {
            quoridor::executePawnMove(quoridorState, cell);
            quoridorState.selectedGx = -1;
            quoridorState.selectedGy = -1;
            if (quoridorState.mode == quoridor::Mode::Nearby) {
              play.playState(quoridorState);
            } else if (quoridorState.mode == quoridor::Mode::Solo && quoridorState.turn == quoridor::Player::Black &&
                quoridorState.winner == quoridor::Player::None) {
              quoridorAiPending = true;
            }
            requestUpdate();
            return;
          }
        }
      }
    }
  } else if (screen == tt::Screen::Dominoes) {
    if (surfaceRevealed()) {
      const bool canAct = (dominoesState.mode == dominoes::Mode::TwoPlayer) ||
                          (dominoesState.mode == dominoes::Mode::Nearby &&
                           dominoesState.turn == dominoesHumanPlayer && linkYourTurn()) ||
                          (dominoesState.mode == dominoes::Mode::Solo &&
                           dominoesState.turn == dominoes::Player::P1);
      if (canAct && !dominoesAiPending && dominoesState.winner == dominoes::Player::None && !dominoesState.roundOver) {
        const int pIdx = (dominoesState.mode == dominoes::Mode::Nearby) ? static_cast<int>(dominoesHumanPlayer) : 0;
        const int8_t hIdx = dominoesui::handIdxAt(static_cast<int16_t>(tapX), static_cast<int16_t>(tapY), dominoesState.handCount[pIdx]);
        if (hIdx >= 0) {
          dominoesState.selectedHandIdx = (dominoesState.selectedHandIdx == hIdx) ? -1 : hIdx;
          requestUpdate();
          return;
        }
      }
    }
  } else if (screen == tt::Screen::Sandbox) {
    if (surfaceRevealed()) {
      if (sandboxState.mode == sandbox::ToolMode::Dice) {
        const int d = sandboxui::dieAt(tapX, tapY, sandboxState.diceCount);
        if (d >= 0) {
          sandbox::toggleHoldDie(sandboxState, d);
          requestUpdate();
          return;
        }
      } else if (sandboxState.mode == sandbox::ToolMode::Cards) {
        if (sandboxui::drawPileRect().contains(static_cast<int16_t>(tapX), static_cast<int16_t>(tapY))) {
          if (sandbox::drawCard(sandboxState)) {
            requestUpdate();
            return;
          }
        }
        const int c = sandboxui::handCardAt(tapX, tapY, sandboxState.handCount);
        if (c >= 0) {
          if (sandboxState.handHidden) {
            sandboxState.handHidden = false;
          } else {
            sandbox::discardCard(sandboxState, c);
          }
          requestUpdate();
          return;
        }
      } else if (sandboxState.mode == sandbox::ToolMode::Board) {
        const int sq = sandboxui::squareAt(tapX, tapY);
        if (sq >= 0) {
          if (sandboxState.selectedToken != -1) {
            sandbox::moveToken(sandboxState, sandboxState.selectedToken, static_cast<uint8_t>(sq));
            requestUpdate();
            return;
          } else {
            const int tok = sandbox::findTokenAt(sandboxState, static_cast<uint8_t>(sq));
            if (tok != -1) {
              sandboxState.selectedToken = static_cast<int8_t>(tok);
              requestUpdate();
              return;
            } else {
              sandbox::spawnToken(sandboxState, static_cast<uint8_t>(sq), sandboxState.spawnColor);
              requestUpdate();
              return;
            }
          }
        }
      }
    }
  }

  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case tabletopui::ActionGoToPage: {
      if (event.value >= 0 && event.value < tabletopui::kPageCount && event.value != lobbyPage) {
        lobbyPage = event.value;
        lobbySelected = lobbyPage * tabletopui::kRowsPerPage;
        requestUpdate();
      }
      return;
    }
    case tabletopui::ActionLobbyRow: {
      const auto row = static_cast<tt::LobbyRow>(event.value);
      lobbySelected = static_cast<int>(row);
      lobbyPage = lobbySelected / tabletopui::kRowsPerPage;
      switch (row) {
        case tt::LobbyRow::Reversi:
          goTo(tt::Screen::Reversi);
          return;
        case tt::LobbyRow::Mancala:
          goTo(tt::Screen::Mancala);
          return;
        case tt::LobbyRow::Morris:
          goTo(tt::Screen::Morris);
          return;
        case tt::LobbyRow::Onitama:
          startOnitama();
          goTo(tt::Screen::Onitama);
          return;
        case tt::LobbyRow::LiarsDice:
          startLiarsDice();
          goTo(tt::Screen::LiarsDice);
          return;
        case tt::LobbyRow::Dobutsu:
          startDobutsu();
          goTo(tt::Screen::Dobutsu);
          return;
        case tt::LobbyRow::Backgammon:
          startBackgammon();
          goTo(tt::Screen::Backgammon);
          return;
        case tt::LobbyRow::Santorini:
          startSantorini();
          goTo(tt::Screen::Santorini);
          return;
        case tt::LobbyRow::Quoridor:
          startQuoridor();
          goTo(tt::Screen::Quoridor);
          return;
        case tt::LobbyRow::Dominoes:
          startDominoes();
          goTo(tt::Screen::Dominoes);
          return;
        case tt::LobbyRow::Sandbox:
          goTo(tt::Screen::Sandbox);
          return;
        case tt::LobbyRow::About:
          goTo(tt::Screen::About);
          return;
        case tt::LobbyRow::Count:
          return;
      }
      return;
    }
    case tabletopui::ActionBackToLobby:
    case reversiui::ActionBackToLobby:
    case mancalaui::ActionBackToLobby:
    case morrisui::ActionBackToLobby:
    case onitamaui::ActionBackToLobby:
    case liarsdiceui::ActionBackToLobby:
    case dobutsuui::ActionBackToLobby:
    case santoriniui::ActionBackToLobby:
    case quoridorui::ActionBackToLobby:
    case dominoesui::ActionBackToLobby:
    case sandboxui::ActionBackToLobby:
      goTo(tt::Screen::Lobby);
      return;
    case reversiui::ActionRestart:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startReversi();
      requestUpdate();
      return;
    case reversiui::ActionPass:
      if (!reversiGame.isOver) {
        reversi::passTurn(reversiGame);
        if (reversiMode == reversiui::PlayMode::Nearby) {
          play.playState(reversiGame);
        }
        requestUpdate();
      }
      return;
    case reversiui::ActionToggleMode:
      if (reversiMode == reversiui::PlayMode::SoloVsAI) {
        reversiMode = reversiui::PlayMode::TwoPlayerPassAndPlay;
      } else if (reversiMode == reversiui::PlayMode::TwoPlayerPassAndPlay) {
        reversiMode = reversiui::PlayMode::Nearby;
        enterLink(linkplay::GameId::TabletopReversi);
      } else {
        leaveLink();
        reversiMode = reversiui::PlayMode::SoloVsAI;
      }
      requestUpdate();
      return;
    case reversiui::ActionFlipBoard:
      reversiFlip = !reversiFlip;
      requestUpdate();
      return;
    case mancalaui::ActionRestart:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startMancala();
      requestUpdate();
      return;
    case mancalaui::ActionToggleMode:
      if (mancalaMode == mancalaui::PlayMode::SoloVsAI) {
        mancalaMode = mancalaui::PlayMode::TwoPlayerPassAndPlay;
      } else if (mancalaMode == mancalaui::PlayMode::TwoPlayerPassAndPlay) {
        mancalaMode = mancalaui::PlayMode::Nearby;
        enterLink(linkplay::GameId::TabletopMancala);
      } else {
        leaveLink();
        mancalaMode = mancalaui::PlayMode::SoloVsAI;
      }
      requestUpdate();
      return;
    case mancalaui::ActionFlipBoard:
      mancalaFlip = !mancalaFlip;
      requestUpdate();
      return;
    case morrisui::ActionRestart:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startMorris();
      requestUpdate();
      return;
    case morrisui::ActionToggleMode:
      if (morrisMode == morrisui::PlayMode::SoloVsAI) {
        morrisMode = morrisui::PlayMode::TwoPlayerPassAndPlay;
      } else if (morrisMode == morrisui::PlayMode::TwoPlayerPassAndPlay) {
        morrisMode = morrisui::PlayMode::Nearby;
        enterLink(linkplay::GameId::TabletopMorris);
      } else {
        leaveLink();
        morrisMode = morrisui::PlayMode::SoloVsAI;
      }
      requestUpdate();
      return;
    case morrisui::ActionFlipBoard:
      morrisFlip = !morrisFlip;
      requestUpdate();
      return;
    case onitamaui::ActionRestart:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startOnitama();
      requestUpdate();
      return;
    case onitamaui::ActionToggleMode:
      if (onitamaMode == onitamaui::PlayMode::SoloVsAI) {
        onitamaMode = onitamaui::PlayMode::TwoPlayerPassAndPlay;
      } else if (onitamaMode == onitamaui::PlayMode::TwoPlayerPassAndPlay) {
        onitamaMode = onitamaui::PlayMode::Nearby;
        enterLink(linkplay::GameId::TabletopOnitama);
      } else {
        leaveLink();
        onitamaMode = onitamaui::PlayMode::SoloVsAI;
      }
      requestUpdate();
      return;
    case onitamaui::ActionFlipBoard:
      onitamaFlip = !onitamaFlip;
      requestUpdate();
      return;
    case liarsdiceui::ActionRestart:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startLiarsDice();
      requestUpdate();
      return;
    case liarsdiceui::ActionToggleMode:
      if (liarsDiceMode == liarsdiceui::PlayMode::SoloVsAI) {
        liarsDiceMode = liarsdiceui::PlayMode::TwoPlayerPassAndPlay;
      } else if (liarsDiceMode == liarsdiceui::PlayMode::TwoPlayerPassAndPlay) {
        liarsDiceMode = liarsdiceui::PlayMode::Nearby;
        enterLink(linkplay::GameId::TabletopLiarsDice);
      } else {
        leaveLink();
        liarsDiceMode = liarsdiceui::PlayMode::SoloVsAI;
      }
      requestUpdate();
      return;
    case liarsdiceui::ActionFlipBoard:
      liarsDiceFlip = !liarsDiceFlip;
      requestUpdate();
      return;
    case liarsdiceui::ActionTogglePeek:
      liarsDiceShieldDice = !liarsDiceShieldDice;
      requestUpdate();
      return;
    case liarsdiceui::ActionCountMinus:
      if (liarsDiceSelectedCount > 1) {
        liarsDiceSelectedCount--;
        requestUpdate();
      }
      return;
    case liarsdiceui::ActionCountPlus: {
      const uint8_t total = liarsdice::countTotalActiveDice(liarsDiceGame);
      if (liarsDiceSelectedCount < total) {
        liarsDiceSelectedCount++;
        requestUpdate();
      }
      return;
    }
    case liarsdiceui::ActionFaceBase:
    case liarsdiceui::ActionFaceBase + 1:
    case liarsdiceui::ActionFaceBase + 2:
    case liarsdiceui::ActionFaceBase + 3:
    case liarsdiceui::ActionFaceBase + 4:
    case liarsdiceui::ActionFaceBase + 5:
      liarsDiceSelectedFace = static_cast<uint8_t>(event.action - liarsdiceui::ActionFaceBase + 1);
      requestUpdate();
      return;
    case liarsdiceui::ActionMakeBid: {
      const bool canBid = (liarsDiceMode == liarsdiceui::PlayMode::TwoPlayerPassAndPlay) ||
                          (liarsDiceMode == liarsdiceui::PlayMode::Nearby &&
                           liarsDiceGame.activePlayer == liarsDiceHumanPlayer && linkYourTurn()) ||
                          (liarsDiceMode == liarsdiceui::PlayMode::SoloVsAI &&
                           liarsDiceGame.activePlayer == liarsDiceHumanPlayer);
      if (!canBid) return;
      const liarsdice::Bid proposedBid{liarsDiceSelectedCount, liarsDiceSelectedFace, liarsDiceGame.activePlayer};
      if (liarsdice::makeBid(liarsDiceGame, proposedBid)) {
        if (liarsDiceSelectedFace < 6) {
          liarsDiceSelectedFace++;
        } else {
          liarsDiceSelectedCount++;
          liarsDiceSelectedFace = 2;
        }
        if (liarsDiceMode == liarsdiceui::PlayMode::Nearby) {
          play.playState(liarsDiceGame);
        } else if (liarsDiceMode == liarsdiceui::PlayMode::SoloVsAI && liarsDiceGame.activePlayer != 0) {
          liarsDiceAiPending = true;
        }
        requestUpdate();
      }
      return;
    }
    case liarsdiceui::ActionChallenge: {
      const bool canChallenge = (liarsDiceMode == liarsdiceui::PlayMode::TwoPlayerPassAndPlay) ||
                                (liarsDiceMode == liarsdiceui::PlayMode::Nearby &&
                                 liarsDiceGame.activePlayer == liarsDiceHumanPlayer && linkYourTurn()) ||
                                (liarsDiceMode == liarsdiceui::PlayMode::SoloVsAI &&
                                 liarsDiceGame.activePlayer == liarsDiceHumanPlayer);
      if (!canChallenge) return;
      if (liarsdice::challenge(liarsDiceGame)) {
        if (liarsDiceMode == liarsdiceui::PlayMode::Nearby) {
          play.playState(liarsDiceGame);
        }
        requestUpdate();
      }
      return;
    }
    case liarsdiceui::ActionNextRound: {
      if (liarsDiceMode == liarsdiceui::PlayMode::Nearby && !linkYourTurn()) return;
      liarsDiceRound++;
      const uint32_t roundSeed = (liarsDiceMode == liarsdiceui::PlayMode::Nearby)
                                     ? play.matchSeed(liarsDiceRound)
                                     : static_cast<uint32_t>(millis());
      liarsdice::startNewRound(liarsDiceGame, roundSeed);
      liarsDiceSelectedCount = 1;
      liarsDiceSelectedFace = 2;
      if (liarsDiceMode == liarsdiceui::PlayMode::Nearby) {
        liarsDiceGame.activePlayer = 1 - liarsDiceHumanPlayer;
        play.playState(liarsDiceGame);
      } else if (liarsDiceMode == liarsdiceui::PlayMode::SoloVsAI && liarsDiceGame.activePlayer != 0) {
        liarsDiceAiPending = true;
      }
      requestUpdate();
      return;
    }
    case dobutsuui::ActionRestart:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startDobutsu();
      requestUpdate();
      return;
    case dobutsuui::ActionToggleMode:
      if (dobutsuMode == dobutsuui::PlayMode::SoloVsAI) {
        dobutsuMode = dobutsuui::PlayMode::TwoPlayerPassAndPlay;
      } else if (dobutsuMode == dobutsuui::PlayMode::TwoPlayerPassAndPlay) {
        dobutsuMode = dobutsuui::PlayMode::Nearby;
        enterLink(linkplay::GameId::TabletopDobutsu);
      } else {
        leaveLink();
        dobutsuMode = dobutsuui::PlayMode::SoloVsAI;
      }
      requestUpdate();
      return;
    case dobutsuui::ActionFlipBoard:
      dobutsuFlip = !dobutsuFlip;
      requestUpdate();
      return;
    case dobutsuui::ActionReserveGiraffe:
      dobutsuSelectedSq = (dobutsuSelectedSq == dobutsu::kDropGiraffe) ? -1 : dobutsu::kDropGiraffe;
      requestUpdate();
      return;
    case dobutsuui::ActionReserveElephant:
      dobutsuSelectedSq = (dobutsuSelectedSq == dobutsu::kDropElephant) ? -1 : dobutsu::kDropElephant;
      requestUpdate();
      return;
    case dobutsuui::ActionReserveChick:
      dobutsuSelectedSq = (dobutsuSelectedSq == dobutsu::kDropChick) ? -1 : dobutsu::kDropChick;
      requestUpdate();
      return;
    case backgammonui::ActionRollDice: {
      const bool canRoll = (backgammonMode == backgammonui::PlayMode::TwoPlayerPassAndPlay) ||
                           (backgammonMode == backgammonui::PlayMode::Nearby &&
                            backgammonGame.turn == backgammonHumanPlayer && linkYourTurn()) ||
                           (backgammonMode == backgammonui::PlayMode::SoloVsAI &&
                            backgammonGame.turn == backgammonHumanPlayer);
      if (!canRoll) return;
      if (!backgammonGame.isRolled) {
        backgammon::rollDice(backgammonGame, static_cast<uint32_t>(millis()));
        backgammon::Move moves[64];
        if (backgammon::generateLegalMoves(backgammonGame, moves, 64) == 0) {
          backgammon::passTurn(backgammonGame);
          if (backgammonMode == backgammonui::PlayMode::Nearby) {
            play.playState(backgammonGame);
          } else if (backgammonMode == backgammonui::PlayMode::SoloVsAI && backgammonGame.turn != backgammonHumanPlayer) {
            backgammonAiPending = true;
          }
        }
      }
      requestUpdate();
      return;
    }
    case backgammonui::ActionEndTurn: {
      const bool canEnd = (backgammonMode == backgammonui::PlayMode::TwoPlayerPassAndPlay) ||
                          (backgammonMode == backgammonui::PlayMode::Nearby &&
                           backgammonGame.turn == backgammonHumanPlayer && linkYourTurn()) ||
                          (backgammonMode == backgammonui::PlayMode::SoloVsAI &&
                           backgammonGame.turn == backgammonHumanPlayer);
      if (!canEnd) return;
      backgammon::passTurn(backgammonGame);
      backgammonSelectedPoint = -99;
      if (backgammonMode == backgammonui::PlayMode::Nearby) {
        play.playState(backgammonGame);
      } else if (backgammonMode == backgammonui::PlayMode::SoloVsAI && !backgammonGame.isOver &&
                 backgammonGame.turn != backgammonHumanPlayer) {
        backgammonAiPending = true;
      }
      requestUpdate();
      return;
    }
    case backgammonui::ActionBearOff: {
      const bool canBear = (backgammonMode == backgammonui::PlayMode::TwoPlayerPassAndPlay) ||
                           (backgammonMode == backgammonui::PlayMode::Nearby &&
                            backgammonGame.turn == backgammonHumanPlayer && linkYourTurn()) ||
                           (backgammonMode == backgammonui::PlayMode::SoloVsAI &&
                            backgammonGame.turn == backgammonHumanPlayer);
      if (!canBear) return;
      if (backgammonSelectedPoint != -99) {
        backgammon::Move legalMoves[64];
        const size_t nMoves = backgammon::generateLegalMoves(backgammonGame, legalMoves, 64);
        for (size_t mi = 0; mi < nMoves; ++mi) {
          if (legalMoves[mi].from == backgammonSelectedPoint && legalMoves[mi].to == backgammon::kToOff) {
            backgammon::applyMove(backgammonGame, legalMoves[mi]);
            backgammonSelectedPoint = -99;
            if (backgammonMode == backgammonui::PlayMode::Nearby) {
              if (backgammonGame.turn != backgammonHumanPlayer || backgammonGame.isOver) {
                play.playState(backgammonGame);
              }
            } else if (backgammonMode == backgammonui::PlayMode::SoloVsAI && !backgammonGame.isOver &&
                       backgammonGame.turn != backgammonHumanPlayer) {
              backgammonAiPending = true;
            }
            requestUpdate();
            return;
          }
        }
      }
      requestUpdate();
      return;
    }
    case backgammonui::ActionRestart:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startBackgammon();
      requestUpdate();
      return;
    case backgammonui::ActionToggleMode:
      if (backgammonMode == backgammonui::PlayMode::SoloVsAI) {
        backgammonMode = backgammonui::PlayMode::TwoPlayerPassAndPlay;
      } else if (backgammonMode == backgammonui::PlayMode::TwoPlayerPassAndPlay) {
        backgammonMode = backgammonui::PlayMode::Nearby;
        enterLink(linkplay::GameId::TabletopBackgammon);
      } else {
        leaveLink();
        backgammonMode = backgammonui::PlayMode::SoloVsAI;
      }
      requestUpdate();
      return;
    case backgammonui::ActionFlipBoard:
      backgammonFlip = !backgammonFlip;
      requestUpdate();
      return;
    case backgammonui::ActionBackToLobby:
      goTo(tt::Screen::Lobby);
      return;
    case santoriniui::ActionRestart:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startSantorini();
      requestUpdate();
      return;
    case santoriniui::ActionToggleMode:
      if (santoriniState.mode == santorini::Mode::Solo) {
        santoriniState.mode = santorini::Mode::TwoPlayer;
      } else if (santoriniState.mode == santorini::Mode::TwoPlayer) {
        santoriniState.mode = santorini::Mode::Nearby;
        enterLink(linkplay::GameId::TabletopSantorini);
      } else {
        leaveLink();
        santoriniState.mode = santorini::Mode::Solo;
      }
      requestUpdate();
      return;
    case santoriniui::ActionFlipBoard:
      santoriniState.flip = !santoriniState.flip;
      requestUpdate();
      return;
    case quoridorui::ActionRestart:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startQuoridor();
      requestUpdate();
      return;
    case quoridorui::ActionToggleMode:
      if (quoridorState.mode == quoridor::Mode::Solo) {
        quoridorState.mode = quoridor::Mode::TwoPlayer;
      } else if (quoridorState.mode == quoridor::Mode::TwoPlayer) {
        quoridorState.mode = quoridor::Mode::Nearby;
        enterLink(linkplay::GameId::TabletopQuoridor);
      } else {
        leaveLink();
        quoridorState.mode = quoridor::Mode::Solo;
      }
      requestUpdate();
      return;
    case quoridorui::ActionFlipBoard:
      quoridorState.flip = !quoridorState.flip;
      requestUpdate();
      return;
    case quoridorui::ActionToggleOrient:
      quoridorState.curOrient = (quoridorState.curOrient == quoridor::WallOrientation::Horizontal)
                                    ? quoridor::WallOrientation::Vertical
                                    : quoridor::WallOrientation::Horizontal;
      if (quoridorState.selectedGx != -1 &&
          !quoridor::canPlaceWall(quoridorState, quoridorState.selectedGx, quoridorState.selectedGy, quoridorState.curOrient)) {
        quoridorState.selectedGx = -1;
        quoridorState.selectedGy = -1;
      }
      requestUpdate();
      return;
    case quoridorui::ActionConfirmWall:
      if (quoridorState.selectedGx != -1 && quoridorState.selectedGy != -1) {
        const bool canAct = (quoridorState.mode == quoridor::Mode::TwoPlayer) ||
                            (quoridorState.mode == quoridor::Mode::Nearby &&
                             quoridorState.turn == static_cast<quoridor::Player>(quoridorHumanPlayer) &&
                             linkYourTurn()) ||
                            (quoridorState.mode == quoridor::Mode::Solo &&
                             quoridorState.turn == quoridor::Player::White);
        if (!canAct) return;
        if (quoridor::executeWallPlacement(quoridorState, quoridorState.selectedGx, quoridorState.selectedGy, quoridorState.curOrient)) {
          quoridorState.selectedGx = -1;
          quoridorState.selectedGy = -1;
          if (quoridorState.mode == quoridor::Mode::Nearby) {
            play.playState(quoridorState);
          } else if (quoridorState.mode == quoridor::Mode::Solo && quoridorState.turn == quoridor::Player::Black &&
              quoridorState.winner == quoridor::Player::None) {
            quoridorAiPending = true;
          }
          requestUpdate();
          return;
        }
      }
      return;
    case dominoesui::ActionRestart:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startDominoes();
      requestUpdate();
      return;
    case dominoesui::ActionToggleMode:
      if (dominoesState.mode == dominoes::Mode::Solo) {
        dominoesState.mode = dominoes::Mode::TwoPlayer;
      } else if (dominoesState.mode == dominoes::Mode::TwoPlayer) {
        dominoesState.mode = dominoes::Mode::Nearby;
        enterLink(linkplay::GameId::TabletopDominoes);
      } else {
        leaveLink();
        dominoesState.mode = dominoes::Mode::Solo;
      }
      requestUpdate();
      return;
    case dominoesui::ActionTogglePrivacy:
      dominoesState.privacyConcealed = !dominoesState.privacyConcealed;
      requestUpdate();
      return;
    case dominoesui::ActionPlayLeft:
      if (dominoesState.selectedHandIdx >= 0) {
        const bool canAct = (dominoesState.mode == dominoes::Mode::TwoPlayer) ||
                            (dominoesState.mode == dominoes::Mode::Nearby &&
                             dominoesState.turn == dominoesHumanPlayer && linkYourTurn()) ||
                            (dominoesState.mode == dominoes::Mode::Solo &&
                             dominoesState.turn == dominoes::Player::P1);
        if (!canAct) return;
        if (dominoes::playTile(dominoesState, dominoesState.selectedHandIdx, dominoes::PlaySide::Left)) {
          dominoesState.selectedHandIdx = -1;
          if (dominoesState.mode == dominoes::Mode::Nearby) {
            play.playState(dominoesState);
          } else if (dominoesState.mode == dominoes::Mode::Solo && dominoesState.turn == dominoes::Player::P2 &&
              dominoesState.winner == dominoes::Player::None && !dominoesState.roundOver) {
            dominoesAiPending = true;
          }
          requestUpdate();
          return;
        }
      }
      return;
    case dominoesui::ActionPlayRight:
      if (dominoesState.selectedHandIdx >= 0) {
        const bool canAct = (dominoesState.mode == dominoes::Mode::TwoPlayer) ||
                            (dominoesState.mode == dominoes::Mode::Nearby &&
                             dominoesState.turn == dominoesHumanPlayer && linkYourTurn()) ||
                            (dominoesState.mode == dominoes::Mode::Solo &&
                             dominoesState.turn == dominoes::Player::P1);
        if (!canAct) return;
        if (dominoes::playTile(dominoesState, dominoesState.selectedHandIdx, dominoes::PlaySide::Right)) {
          dominoesState.selectedHandIdx = -1;
          if (dominoesState.mode == dominoes::Mode::Nearby) {
            play.playState(dominoesState);
          } else if (dominoesState.mode == dominoes::Mode::Solo && dominoesState.turn == dominoes::Player::P2 &&
              dominoesState.winner == dominoes::Player::None && !dominoesState.roundOver) {
            dominoesAiPending = true;
          }
          requestUpdate();
          return;
        }
      }
      return;
    case dominoesui::ActionDrawTile: {
      const bool canAct = (dominoesState.mode == dominoes::Mode::TwoPlayer) ||
                          (dominoesState.mode == dominoes::Mode::Nearby &&
                           dominoesState.turn == dominoesHumanPlayer && linkYourTurn()) ||
                          (dominoesState.mode == dominoes::Mode::Solo &&
                           dominoesState.turn == dominoes::Player::P1);
      if (!canAct) return;
      dominoes::drawFromBoneyard(dominoesState);
      requestUpdate();
      return;
    }
    case dominoesui::ActionPassTurn: {
      const bool canAct = (dominoesState.mode == dominoes::Mode::TwoPlayer) ||
                          (dominoesState.mode == dominoes::Mode::Nearby &&
                           dominoesState.turn == dominoesHumanPlayer && linkYourTurn()) ||
                          (dominoesState.mode == dominoes::Mode::Solo &&
                           dominoesState.turn == dominoes::Player::P1);
      if (!canAct) return;
      dominoes::passTurn(dominoesState);
      if (dominoesState.mode == dominoes::Mode::Nearby) {
        play.playState(dominoesState);
      } else if (dominoesState.mode == dominoes::Mode::Solo && dominoesState.turn == dominoes::Player::P2 &&
          dominoesState.winner == dominoes::Player::None && !dominoesState.roundOver) {
        dominoesAiPending = true;
      }
      requestUpdate();
      return;
    }
    case sandboxui::ActionTabDice:
      sandboxState.mode = sandbox::ToolMode::Dice;
      requestUpdate();
      return;
    case sandboxui::ActionTabCards:
      sandboxState.mode = sandbox::ToolMode::Cards;
      requestUpdate();
      return;
    case sandboxui::ActionTabCounters:
      sandboxState.mode = sandbox::ToolMode::Counters;
      requestUpdate();
      return;
    case sandboxui::ActionTabBoard:
      sandboxState.mode = sandbox::ToolMode::Board;
      requestUpdate();
      return;
    case sandboxui::ActionRollDice:
      sandbox::rollDice(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionAddDie:
      sandbox::addDie(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionRemoveDie:
      sandbox::removeDie(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionCycleDieType:
      sandbox::cycleDieType(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionDrawCard:
      sandbox::drawCard(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionShuffleDeck:
      sandbox::shuffleDeck(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionResetDeck:
      sandbox::initDeck(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionDiscardHand:
      sandbox::discardHand(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionToggleHandHidden:
      sandbox::toggleHandHidden(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionNextPlayer:
      sandbox::nextPlayer(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionResetScores20:
      sandbox::resetScores(sandboxState, 20);
      requestUpdate();
      return;
    case sandboxui::ActionResetScores0:
      sandbox::resetScores(sandboxState, 0);
      requestUpdate();
      return;
    case sandboxui::ActionToggleSpawnColor:
      sandbox::toggleSpawnColor(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionCheckersPreset:
      sandbox::setupCheckersPreset(sandboxState);
      requestUpdate();
      return;
    case sandboxui::ActionClearBoard:
      sandbox::clearTokens(sandboxState);
      requestUpdate();
      return;
    default:
      if (event.action >= sandboxui::ActionDieTapBase && event.action < sandboxui::ActionDieTapBase + 6) {
        sandbox::toggleHoldDie(sandboxState, event.action - sandboxui::ActionDieTapBase);
        requestUpdate();
        return;
      }
      if (event.action >= sandboxui::ActionHandCardTapBase && event.action < sandboxui::ActionHandCardTapBase + 5) {
        sandbox::discardCard(sandboxState, event.action - sandboxui::ActionHandCardTapBase);
        requestUpdate();
        return;
      }
      if (event.action >= sandboxui::ActionScoreMinus && event.action < sandboxui::ActionScoreMinus + 4) {
        sandbox::changeScore(sandboxState, event.action - sandboxui::ActionScoreMinus, -1);
        requestUpdate();
        return;
      }
      if (event.action >= sandboxui::ActionScorePlus && event.action < sandboxui::ActionScorePlus + 4) {
        sandbox::changeScore(sandboxState, event.action - sandboxui::ActionScorePlus, 1);
        requestUpdate();
        return;
      }
      break;
  }
}

void TabletopActivity::gameRender() {
  namespace fui = freeink::ui;

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  switch (screen) {
    case tt::Screen::Lobby: {
      tabletopui::LobbyModel model;
      model.page = lobbyPage;
      model.selected = lobbySelected;
      tabletopui::buildLobby(surface, model);
      break;
    }
    case tt::Screen::Reversi: {
      reversiui::BoardModel model;
      model.game = reversiGame;
      model.humanSeat = reversiHumanSeat;
      model.mode = reversiMode;
      model.isAiThinking = reversiAiPending;
      model.flipBoard = reversiFlip;
      model.legalMoves = reversi::legalMoves(reversiGame, reversiGame.turn);
      reversiui::buildBoard(surface, model);
      break;
    }
    case tt::Screen::Mancala: {
      mancalaui::BoardModel model;
      model.game = mancalaGame;
      model.humanPlayer = mancalaHumanPlayer;
      model.mode = mancalaMode;
      model.isAiThinking = mancalaAiPending;
      model.flipBoard = mancalaFlip;
      mancalaui::buildBoard(surface, model);
      break;
    }
    case tt::Screen::Morris: {
      morrisui::BoardModel model;
      model.game = morrisGame;
      model.humanPlayer = morrisHumanPlayer;
      model.mode = morrisMode;
      model.isAiThinking = morrisAiPending;
      model.flipBoard = morrisFlip;
      morrisui::buildBoard(surface, model);
      break;
    }
    case tt::Screen::Onitama: {
      onitamaui::ScreenModel model;
      model.game = onitamaGame;
      model.humanPlayer = onitamaHumanPlayer;
      model.mode = onitamaMode;
      model.selectedCell = onitamaSelectedCell;
      model.selectedCard = onitamaSelectedCard;
      model.isAiThinking = onitamaAiPending;
      model.flipBoard = onitamaFlip;
      onitamaui::buildBoard(surface, model);
      break;
    }
    case tt::Screen::LiarsDice: {
      liarsdiceui::ScreenModel model;
      model.game = liarsDiceGame;
      model.humanPlayer = liarsDiceHumanPlayer;
      model.mode = liarsDiceMode;
      model.selectedCount = liarsDiceSelectedCount;
      model.selectedFace = liarsDiceSelectedFace;
      model.isAiThinking = liarsDiceAiPending;
      model.shieldDice = liarsDiceShieldDice;
      model.flipBoard = liarsDiceFlip;
      model.isYourTurn = (liarsDiceMode == liarsdiceui::PlayMode::Nearby) ? linkYourTurn() : true;
      liarsdiceui::buildBoard(surface, model);
      break;
    }
    case tt::Screen::Dobutsu: {
      dobutsuui::ScreenModel model;
      model.game = dobutsuGame;
      model.humanPlayer = dobutsuHumanPlayer;
      model.mode = dobutsuMode;
      model.selectedSq = dobutsuSelectedSq;
      model.isAiThinking = dobutsuAiPending;
      model.flipBoard = dobutsuFlip;
      dobutsuui::buildBoard(surface, model);
      break;
    }
    case tt::Screen::Backgammon: {
      backgammonui::ScreenModel model;
      model.game = backgammonGame;
      model.humanPlayer = backgammonHumanPlayer;
      model.mode = backgammonMode;
      model.selectedPoint = backgammonSelectedPoint;
      model.isAiThinking = backgammonAiPending;
      model.flipBoard = backgammonFlip;
      backgammonui::buildBoard(surface, model);
      break;
    }
    case tt::Screen::Santorini:
      santoriniui::render(surface, santoriniState, santoriniHumanPlayer);
      break;
    case tt::Screen::Quoridor:
      quoridorui::render(surface, quoridorState, quoridorHumanPlayer);
      break;
    case tt::Screen::Dominoes:
      dominoesui::render(surface, dominoesState, static_cast<uint8_t>(dominoesHumanPlayer));
      break;
    case tt::Screen::Sandbox: {
      sandboxui::SandboxModel model;
      model.state = sandboxState;
      sandboxui::buildSandbox(surface, model);
      break;
    }
    case tt::Screen::About:
      tabletopui::buildAbout(surface);
      break;
  }

  interactionsReady = true;
  renderer.displayBuffer(wantsFullRefresh ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  wantsFullRefresh = false;
}
