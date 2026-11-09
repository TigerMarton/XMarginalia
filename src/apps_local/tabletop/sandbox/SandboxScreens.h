#pragma once

#include "../../ui/ToyboxScreen.h"
#include "SandboxCore.h"

namespace sandboxui {

namespace fui = freeink::ui;

// Action IDs
constexpr int16_t ActionBackToLobby = 100;

// Tabs (200..203)
constexpr int16_t ActionTabDice = 200;
constexpr int16_t ActionTabCards = 201;
constexpr int16_t ActionTabCounters = 202;
constexpr int16_t ActionTabBoard = 203;

// Dice Actions (210..239)
constexpr int16_t ActionRollDice = 210;
constexpr int16_t ActionDieTapBase = 220;  // 220..225 for dice 0..5
constexpr int16_t ActionAddDie = 230;
constexpr int16_t ActionRemoveDie = 231;
constexpr int16_t ActionCycleDieType = 232;

// Cards Actions (240..269)
constexpr int16_t ActionDrawCard = 240;
constexpr int16_t ActionShuffleDeck = 241;
constexpr int16_t ActionResetDeck = 242;
constexpr int16_t ActionDiscardHand = 243;
constexpr int16_t ActionToggleHandHidden = 244;
constexpr int16_t ActionHandCardTapBase = 250;  // 250..254 for cards 0..4

// Counters Actions (270..299)
constexpr int16_t ActionScoreMinus = 270;  // 270..273 for players 0..3
constexpr int16_t ActionScorePlus = 280;   // 280..283 for players 0..3
constexpr int16_t ActionResetScores20 = 290;
constexpr int16_t ActionResetScores0 = 291;
constexpr int16_t ActionNextPlayer = 292;

// Board Actions (300..310)
constexpr int16_t ActionToggleSpawnColor = 300;
constexpr int16_t ActionCheckersPreset = 301;
constexpr int16_t ActionClearBoard = 302;

struct SandboxModel {
  sandbox::SandboxState state;
};

fui::Rect boardRect();
int squareAt(int x, int y);
fui::Rect dieRect(int index, int totalDice);
int dieAt(int x, int y, int totalDice);
fui::Rect handCardRect(int index);
int handCardAt(int x, int y, int handCount);
fui::Rect drawPileRect();

void buildSandbox(toybox::Screen& screen, const SandboxModel& model);

}  // namespace sandboxui
