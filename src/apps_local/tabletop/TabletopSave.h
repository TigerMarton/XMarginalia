#pragma once

#include <cstdint>
#include "TabletopFlow.h"
#include "games/backgammon/BackgammonCore.h"
#include "games/dobutsu/DobutsuCore.h"
#include "games/dominoes/DominoesCore.h"
#include "games/liarsdice/LiarsDiceCore.h"
#include "games/mancala/MancalaCore.h"
#include "games/morris/MorrisCore.h"
#include "games/onitama/OnitamaCore.h"
#include "games/quoridor/QuoridorCore.h"
#include "games/reversi/ReversiCore.h"
#include "games/santorini/SantoriniCore.h"
#include "sandbox/SandboxCore.h"

namespace tabletop {
namespace save {

constexpr int kVersion = 1;
constexpr int kMaxLine = 4096;

struct Save {
  uint8_t lastScreen = 0;  // Lobby

  // Reversi
  uint8_t reversiMode = 0;
  bool reversiFlip = false;
  reversi::Game reversiGame{};

  // Mancala
  uint8_t mancalaMode = 0;
  bool mancalaFlip = false;
  mancala::Game mancalaGame{};

  // Morris
  uint8_t morrisMode = 0;
  bool morrisFlip = false;
  morris::Game morrisGame{};

  // Onitama
  uint8_t onitamaMode = 0;
  bool onitamaFlip = false;
  onitama::Game onitamaGame{};

  // Liar's Dice
  uint8_t liarsDiceMode = 0;
  liarsdice::Game liarsDiceGame{};

  // Dobutsu Shogi
  uint8_t dobutsuMode = 0;
  bool dobutsuFlip = false;
  dobutsu::Game dobutsuGame{};

  // Backgammon
  uint8_t backgammonMode = 0;
  bool backgammonFlip = false;
  backgammon::Game backgammonGame{};

  // Santorini
  santorini::State santoriniState{};

  // Quoridor
  quoridor::State quoridorState{};

  // Dominoes
  dominoes::State dominoesState{};

  // Sandbox
  sandbox::SandboxState sandboxState{};
};

int pack(const Save& save, char* out, int capacity);
bool unpack(const char* text, Save& save);

}  // namespace save
}  // namespace tabletop
