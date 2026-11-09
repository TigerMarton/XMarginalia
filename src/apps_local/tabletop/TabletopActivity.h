#pragma once

#include <memory>

#include "../link/LinkActivity.h"
#include "../ui/ToyboxScreen.h"
#include "TabletopFlow.h"
#include "TabletopScreens.h"
#include "games/backgammon/BackgammonBrain.h"
#include "games/backgammon/BackgammonCore.h"
#include "games/backgammon/BackgammonScreens.h"
#include "games/dobutsu/DobutsuBrain.h"
#include "games/dobutsu/DobutsuCore.h"
#include "games/dobutsu/DobutsuScreens.h"
#include "games/dominoes/DominoesBrain.h"
#include "games/dominoes/DominoesCore.h"
#include "games/dominoes/DominoesScreens.h"
#include "games/liarsdice/LiarsDiceBrain.h"
#include "games/liarsdice/LiarsDiceCore.h"
#include "games/liarsdice/LiarsDiceScreens.h"
#include "games/mancala/MancalaBrain.h"
#include "games/mancala/MancalaCore.h"
#include "games/mancala/MancalaScreens.h"
#include "games/morris/MorrisBrain.h"
#include "games/morris/MorrisCore.h"
#include "games/morris/MorrisScreens.h"
#include "games/onitama/OnitamaBrain.h"
#include "games/onitama/OnitamaCore.h"
#include "games/onitama/OnitamaScreens.h"
#include "games/quoridor/QuoridorBrain.h"
#include "games/quoridor/QuoridorCore.h"
#include "games/quoridor/QuoridorScreens.h"
#include "games/reversi/ReversiBrain.h"
#include "games/reversi/ReversiCore.h"
#include "games/reversi/ReversiScreens.h"
#include "games/santorini/SantoriniBrain.h"
#include "games/santorini/SantoriniCore.h"
#include "games/santorini/SantoriniScreens.h"
#include "sandbox/SandboxCore.h"
#include "sandbox/SandboxScreens.h"

class TabletopPlay final : public linkplay::PlayBase {
 public:
  using PlayBase::PlayBase;
  template <typename State>
  bool playState(const State& state) {
    static_assert(std::is_trivially_copyable<State>::value, "State must be trivially copyable");
    static_assert(sizeof(State) <= linkplay::kMaxPayloadBytes, "State must fit 192 bytes");
    return playRaw(reinterpret_cast<const uint8_t*>(&state), static_cast<uint8_t>(sizeof(State)));
  }
  template <typename State>
  bool takeOpponentState(State& out) {
    static_assert(std::is_trivially_copyable<State>::value, "State must be trivially copyable");
    static_assert(sizeof(State) <= linkplay::kMaxPayloadBytes, "State must fit 192 bytes");
    State scratch;
    uint8_t length = 0;
    if (!takeRaw(reinterpret_cast<uint8_t*>(&scratch), static_cast<uint8_t>(sizeof(State)), length)) return false;
    if (length != sizeof(State)) return false;
    out = scratch;
    return true;
  }
};

class TabletopActivity final : public linkplay::LinkActivity {
 public:
  TabletopActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : linkplay::LinkActivity("Tabletop", renderer, mappedInput) {}
  ~TabletopActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;

  const char* linkGameTitle() const override;
  const char* linkHeadline() const override;
  void onMatchStart(bool goesFirst) override;
  bool takeOpponentState() override;
  void onRematch() override;
  void onLinkEnded() override;
  bool matchGameOver() const override;
  void onMatchEnded() override;
  void gameLoop() override;
  void gameRender() override;

 protected:
  linkplay::PlayBase& linkState() override { return play; }
  const linkplay::PlayBase& linkState() const override { return play; }
  uint32_t surfaceMeaning() const override;

 private:
  TabletopPlay play;
  void loadSave();
  void writeSave();
  void goTo(tabletop::Screen next);
  void startReversi();
  void stepReversiAi();
  void startMancala();
  void stepMancalaAi();
  void startMorris();
  void stepMorrisAi();
  void startOnitama();
  void stepOnitamaAi();
  void startLiarsDice();
  void stepLiarsDiceAi();
  void startDobutsu();
  void stepDobutsuAi();
  void startBackgammon();
  void stepBackgammonAi();
  void startSantorini();
  void stepSantoriniAi();
  void startQuoridor();
  void stepQuoridorAi();
  void startDominoes();
  void stepDominoesAi();
  void startSandbox();

  tabletop::Screen screen = tabletop::Screen::Lobby;
  int lobbySelected = 0;
  int lobbyPage = 0;

  // Reversi state
  reversi::Game reversiGame{};
  reversiui::PlayMode reversiMode = reversiui::PlayMode::SoloVsAI;
  uint8_t reversiHumanSeat = reversi::kBlack;
  bool reversiAiPending = false;
  bool reversiFlip = false;

  // Mancala state
  mancala::Game mancalaGame{};
  mancalaui::PlayMode mancalaMode = mancalaui::PlayMode::SoloVsAI;
  uint8_t mancalaHumanPlayer = mancala::kPlayer1;
  bool mancalaAiPending = false;
  bool mancalaFlip = false;

  // Nine Men's Morris state
  morris::Game morrisGame{};
  morrisui::PlayMode morrisMode = morrisui::PlayMode::SoloVsAI;
  uint8_t morrisHumanPlayer = morris::kWhite;
  bool morrisAiPending = false;
  bool morrisFlip = false;

  // Onitama state
  onitama::Game onitamaGame{};
  onitamaui::PlayMode onitamaMode = onitamaui::PlayMode::SoloVsAI;
  uint8_t onitamaHumanPlayer = onitama::kPlayer1;
  int8_t onitamaSelectedCell = -1;
  int8_t onitamaSelectedCard = -1;
  bool onitamaAiPending = false;
  bool onitamaFlip = false;

  // Liar's Dice state
  liarsdice::Game liarsDiceGame{};
  liarsdiceui::PlayMode liarsDiceMode = liarsdiceui::PlayMode::SoloVsAI;
  uint8_t liarsDiceHumanPlayer = 0;
  uint8_t liarsDiceSelectedCount = 1;
  uint8_t liarsDiceSelectedFace = 2;
  bool liarsDiceAiPending = false;
  bool liarsDiceShieldDice = false;
  bool liarsDiceFlip = false;
  uint32_t liarsDiceRound = 0;

  // Dobutsu Shogi state
  dobutsu::Game dobutsuGame{};
  dobutsuui::PlayMode dobutsuMode = dobutsuui::PlayMode::SoloVsAI;
  dobutsu::Player dobutsuHumanPlayer = dobutsu::Player::P1;
  int8_t dobutsuSelectedSq = -1;
  bool dobutsuAiPending = false;
  bool dobutsuFlip = false;

  // Backgammon state
  backgammon::Game backgammonGame{};
  backgammonui::PlayMode backgammonMode = backgammonui::PlayMode::SoloVsAI;
  backgammon::Player backgammonHumanPlayer = backgammon::Player::White;
  int8_t backgammonSelectedPoint = -99;
  bool backgammonAiPending = false;
  bool backgammonFlip = false;

  // Santorini state
  santorini::State santoriniState{};
  uint8_t santoriniHumanPlayer = 0;
  bool santoriniAiPending = false;
  bool santoriniFlip = false;

  // Quoridor state
  quoridor::State quoridorState{};
  uint8_t quoridorHumanPlayer = 0;
  bool quoridorAiPending = false;
  bool quoridorFlip = false;

  // Dominoes state
  dominoes::State dominoesState{};
  dominoes::Player dominoesHumanPlayer = dominoes::Player::P1;
  bool dominoesAiPending = false;

  // Sandbox state
  sandbox::SandboxState sandboxState{};

  bool wantsFullRefresh = false;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
