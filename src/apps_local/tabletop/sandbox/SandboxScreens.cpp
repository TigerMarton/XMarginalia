#include "SandboxScreens.h"

#include <cstdio>
#include <cstring>

#include "../../ui/ToyboxFormat.h"
#include "../../cards/CardArt.h"

namespace sandboxui {

namespace {

constexpr int16_t kBoardLeft = 40;
constexpr int16_t kBoardTop = 150;
constexpr int16_t kBoardSide = 400;
constexpr int16_t kSquareSide = 50;

void toyboxChrome(toybox::Screen& screen, const char* title, const char* rightLabel = nullptr) {
  fui::HeaderProps header;
  header.title = title;
  header.rightLabel = rightLabel;
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kUiFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

void drawPips(toybox::Screen& screen, const fui::Rect& box, const int face) {
  constexpr int16_t kPip = 10;
  const int16_t left = static_cast<int16_t>(box.x + 14);
  const int16_t mid = static_cast<int16_t>(box.x + box.width / 2 - kPip / 2);
  const int16_t right = static_cast<int16_t>(box.x + box.width - 14 - kPip);
  const int16_t top = static_cast<int16_t>(box.y + 14);
  const int16_t centre = static_cast<int16_t>(box.y + box.height / 2 - kPip / 2);
  const int16_t bottom = static_cast<int16_t>(box.y + box.height - 14 - kPip);

  static const uint16_t kPips[7] = {
      0,            // unused
      0b000010000,  // 1
      0b100000001,  // 2
      0b100010001,  // 3
      0b101000101,  // 4
      0b101010101,  // 5
      0b101101101,  // 6
  };
  const int16_t xs[3] = {left, mid, right};
  const int16_t ys[3] = {top, centre, bottom};
  const uint16_t bits = kPips[(face >= 1 && face <= 6) ? face : 1];
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      if ((bits & (1u << (8 - (row * 3 + col)))) != 0) {
        screen.target().fill(fui::makeRect(xs[col], ys[row], kPip, kPip), fui::Paint::solid(fui::Color::Black));
      }
    }
  }
}

void drawDieFace(toybox::Screen& screen, const fui::Rect& box, const sandbox::DieType type, const int val,
                 const bool held) {
  screen.target().fill(box, fui::Paint::solid(fui::Color::White));
  screen.target().stroke(box, fui::Paint::solid(fui::Color::Black), toybox::kRule);

  if (type == sandbox::DieType::D6) {
    drawPips(screen, box, val);
  } else {
    // Numeric face for D4/D8/D10/D12/D20
    char numBuf[16];
    std::snprintf(numBuf, sizeof(numBuf), "%d", val);
    fui::TextStyle numStyle;
    numStyle.font = toybox::kDisplayFont;
    numStyle.color = fui::Color::Black;
    numStyle.align = fui::TextAlign::Center;
    const int16_t textY = static_cast<int16_t>(box.y + (box.height - 36) / 2);
    screen.target().text(fui::makeRect(box.x, textY, box.width, 36), numBuf, numStyle);
  }

  if (held) {
    constexpr int16_t kGap = 4;
    const int16_t inset = static_cast<int16_t>(toybox::kFrame + kGap);
    toybox::bracket(screen,
                    fui::makeRect(static_cast<int16_t>(box.x - inset), static_cast<int16_t>(box.y - inset),
                                  static_cast<int16_t>(box.width + inset * 2),
                                  static_cast<int16_t>(box.height + inset * 2)),
                    16, toybox::kFrame);
  }
}

}  // namespace

fui::Rect boardRect() {
  return fui::makeRect(kBoardLeft, kBoardTop, kBoardSide, kBoardSide);
}

int squareAt(const int x, const int y) {
  if (x < kBoardLeft || x >= kBoardLeft + kBoardSide || y < kBoardTop || y >= kBoardTop + kBoardSide) {
    return -1;
  }
  const int col = (x - kBoardLeft) / kSquareSide;
  const int row = (y - kBoardTop) / kSquareSide;
  if (col < 0 || col >= 8 || row < 0 || row >= 8) return -1;
  return row * 8 + col;
}

fui::Rect dieRect(const int index, const int totalDice) {
  constexpr int16_t kDieSize = 84;
  constexpr int16_t kDieGap = 16;
  constexpr int16_t kAreaTop = 250;

  if (totalDice <= 3) {
    const int16_t totalW = static_cast<int16_t>(totalDice * kDieSize + (totalDice - 1) * kDieGap);
    const int16_t startX = static_cast<int16_t>((480 - totalW) / 2);
    return fui::makeRect(static_cast<int16_t>(startX + index * (kDieSize + kDieGap)), kAreaTop + 40, kDieSize,
                         kDieSize);
  }

  // 4, 5, or 6 dice in 2 rows
  const int row = index / 3;
  const int col = index % 3;
  const int countInRow = (row == 0) ? (totalDice > 3 ? 3 : totalDice) : (totalDice - 3);
  const int16_t totalW = static_cast<int16_t>(countInRow * kDieSize + (countInRow - 1) * kDieGap);
  const int16_t startX = static_cast<int16_t>((480 - totalW) / 2);
  const int16_t startY = static_cast<int16_t>(kAreaTop + row * (kDieSize + kDieGap + 10));
  return fui::makeRect(static_cast<int16_t>(startX + col * (kDieSize + kDieGap)), startY, kDieSize, kDieSize);
}

int dieAt(const int x, const int y, const int totalDice) {
  const int16_t sx = static_cast<int16_t>(x);
  const int16_t sy = static_cast<int16_t>(y);
  for (int i = 0; i < totalDice && i < sandbox::kMaxDice; ++i) {
    if (dieRect(i, totalDice).contains(sx, sy)) return i;
  }
  return -1;
}

fui::Rect drawPileRect() {
  return fui::makeRect(80, 160, cardart::kCardW, cardart::kCardH);
}

fui::Rect handCardRect(const int index) {
  constexpr int16_t kCardW = 80;
  constexpr int16_t kCardH = 114;
  constexpr int16_t kGap = 12;
  constexpr int16_t kTotalW = 5 * kCardW + 4 * kGap;
  constexpr int16_t kStartX = (480 - kTotalW) / 2;
  constexpr int16_t kTop = 380;
  return fui::makeRect(static_cast<int16_t>(kStartX + index * (kCardW + kGap)), kTop, kCardW, kCardH);
}

int handCardAt(const int x, const int y, const int handCount) {
  const int16_t sx = static_cast<int16_t>(x);
  const int16_t sy = static_cast<int16_t>(y);
  for (int i = 0; i < handCount && i < sandbox::kMaxHand; ++i) {
    if (handCardRect(i).contains(sx, sy)) return i;
  }
  return -1;
}

void buildSandbox(toybox::Screen& screen, const SandboxModel& model) {
  const char* modeLabel = "DICE";
  if (model.state.mode == sandbox::ToolMode::Cards) modeLabel = "CARDS";
  if (model.state.mode == sandbox::ToolMode::Counters) modeLabel = "LIFE";
  if (model.state.mode == sandbox::ToolMode::Board) modeLabel = "BOARD";

  toyboxChrome(screen, "TABLETOP SANDBOX", modeLabel);

  // Top Tab Navigation Bar
  const fui::Rect tabRow = screen.takeTop(36);
  constexpr int16_t kTabGap = 6;
  const int16_t tabW = static_cast<int16_t>((tabRow.width - 3 * kTabGap) / 4);

  const char* const kTabs[4] = {"DICE", "CARDS", "LIFE", "BOARD"};
  const int16_t kTabActions[4] = {ActionTabDice, ActionTabCards, ActionTabCounters, ActionTabBoard};

  for (int i = 0; i < 4; ++i) {
    const fui::Rect box =
        fui::makeRect(static_cast<int16_t>(tabRow.x + i * (tabW + kTabGap)), tabRow.y, tabW, tabRow.height);
    const bool active = (static_cast<int>(model.state.mode) == i);

    fui::ButtonProps btn;
    btn.label = kTabs[i];
    btn.action = kTabActions[i];
    btn.text.font = toybox::kTileFont;
    btn.text.align = fui::TextAlign::Center;
    if (active) {
      screen.target().fill(box, fui::Paint::solid(fui::Color::Black));
      fui::TextStyle activeStyle;
      activeStyle.font = toybox::kTileFont;
      activeStyle.color = fui::Color::White;
      activeStyle.align = fui::TextAlign::Center;
      screen.target().text(box, kTabs[i], activeStyle);
    } else {
      screen.button(btn, box);
    }
  }

  // Hairline under tab bar
  screen.target().fill(
      fui::makeRect(tabRow.x, static_cast<int16_t>(tabRow.bottom() + 4), tabRow.width, toybox::kHairline),
      fui::Paint::solid(fui::Color::Black));

  // Common Bottom Back Button
  fui::ButtonProps backBtn;
  backBtn.label = "BACK TO LOBBY";
  backBtn.action = ActionBackToLobby;
  screen.button(backBtn, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  // -------------------------------------------------------------------------
  // TAB 0: DICE
  // -------------------------------------------------------------------------
  if (model.state.mode == sandbox::ToolMode::Dice) {
    fui::ButtonProps rollBtn;
    rollBtn.label = "ROLL ALL DICE";
    rollBtn.action = ActionRollDice;
    screen.button(rollBtn, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

    const fui::Rect setupRow = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);
    const int16_t thirdW = static_cast<int16_t>((setupRow.width - 2 * toybox::kGutter) / 3);

    fui::ButtonProps minusBtn;
    minusBtn.label = "- DIE";
    minusBtn.action = ActionRemoveDie;
    minusBtn.text.align = fui::TextAlign::Center;
    minusBtn.text.font = toybox::kTileFont;
    screen.button(minusBtn, fui::makeRect(setupRow.x, setupRow.y, thirdW, setupRow.height));

    char typeBuf[24];
    std::snprintf(typeBuf, sizeof(typeBuf), "DIE: D%d", static_cast<int>(model.state.dieType));
    fui::ButtonProps typeBtn;
    typeBtn.label = typeBuf;
    typeBtn.action = ActionCycleDieType;
    typeBtn.text.align = fui::TextAlign::Center;
    typeBtn.text.font = toybox::kTileFont;
    screen.button(typeBtn,
                  fui::makeRect(static_cast<int16_t>(setupRow.x + thirdW + toybox::kGutter), setupRow.y, thirdW,
                                setupRow.height));

    fui::ButtonProps plusBtn;
    plusBtn.label = "+ DIE";
    plusBtn.action = ActionAddDie;
    plusBtn.text.align = fui::TextAlign::Center;
    plusBtn.text.font = toybox::kTileFont;
    screen.button(plusBtn,
                  fui::makeRect(static_cast<int16_t>(setupRow.x + (thirdW + toybox::kGutter) * 2), setupRow.y, thirdW,
                                setupRow.height));

    // Summary banner
    char sumBuf[32];
    std::snprintf(sumBuf, sizeof(sumBuf), "TOTAL: %d", model.state.lastRollSum);
    fui::TextStyle sumStyle;
    sumStyle.font = toybox::kDisplayFont;
    sumStyle.color = fui::Color::Black;
    sumStyle.align = fui::TextAlign::Center;
    const int16_t totalY = static_cast<int16_t>(tabRow.bottom() + 14);
    screen.target().text(fui::makeRect(tabRow.x, totalY, tabRow.width, 44), sumBuf, sumStyle);

    fui::TextStyle subStyle;
    subStyle.font = toybox::kTileFont;
    subStyle.color = fui::Color::Black;
    subStyle.align = fui::TextAlign::Center;
    screen.target().text(fui::makeRect(tabRow.x, static_cast<int16_t>(totalY + 58), tabRow.width, 24),
                         "Tap any die to hold / lock it", subStyle);

    // Draw the active dice
    for (int i = 0; i < model.state.diceCount && i < sandbox::kMaxDice; ++i) {
      const fui::Rect box = dieRect(i, model.state.diceCount);
      const bool held = (model.state.heldMask & (1U << i)) != 0;
      drawDieFace(screen, box, model.state.dieType, model.state.diceValues[i], held);
    }

    // Roll History
    char histBuf[48];
    std::snprintf(histBuf, sizeof(histBuf), "HISTORY: %d, %d, %d", model.state.rollHistory[0],
                  model.state.rollHistory[1], model.state.rollHistory[2]);
    screen.target().text(fui::makeRect(tabRow.x, static_cast<int16_t>(setupRow.y - 28), tabRow.width, 20), histBuf,
                         subStyle);
    return;
  }

  // -------------------------------------------------------------------------
  // TAB 1: CARDS
  // -------------------------------------------------------------------------
  if (model.state.mode == sandbox::ToolMode::Cards) {
    const fui::Rect actionRow = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);
    constexpr int16_t kCardBtnCount = 5;
    const int16_t quintW = static_cast<int16_t>((actionRow.width - (kCardBtnCount - 1) * toybox::kGutter) / kCardBtnCount);

    fui::ButtonProps drawBtn;
    drawBtn.label = "DRAW";
    drawBtn.action = ActionDrawCard;
    drawBtn.text.font = toybox::kTileFont;
    drawBtn.text.align = fui::TextAlign::Center;
    screen.button(drawBtn, fui::makeRect(actionRow.x, actionRow.y, quintW, actionRow.height));

    fui::ButtonProps hideBtn;
    hideBtn.label = model.state.handHidden ? "PEEK" : "HIDE";
    hideBtn.action = ActionToggleHandHidden;
    hideBtn.text.font = toybox::kTileFont;
    hideBtn.text.align = fui::TextAlign::Center;
    screen.button(hideBtn,
                  fui::makeRect(static_cast<int16_t>(actionRow.x + quintW + toybox::kGutter), actionRow.y, quintW,
                                actionRow.height));

    fui::ButtonProps discBtn;
    discBtn.label = "DISCARD";
    discBtn.action = ActionDiscardHand;
    discBtn.text.font = toybox::kTileFont;
    discBtn.text.align = fui::TextAlign::Center;
    screen.button(discBtn,
                  fui::makeRect(static_cast<int16_t>(actionRow.x + (quintW + toybox::kGutter) * 2), actionRow.y,
                                quintW, actionRow.height));

    fui::ButtonProps shufBtn;
    shufBtn.label = "SHUFFLE";
    shufBtn.action = ActionShuffleDeck;
    shufBtn.text.font = toybox::kTileFont;
    shufBtn.text.align = fui::TextAlign::Center;
    screen.button(shufBtn,
                  fui::makeRect(static_cast<int16_t>(actionRow.x + (quintW + toybox::kGutter) * 3), actionRow.y,
                                quintW, actionRow.height));

    fui::ButtonProps resetBtn;
    resetBtn.label = "NEW 52";
    resetBtn.action = ActionResetDeck;
    resetBtn.text.font = toybox::kTileFont;
    resetBtn.text.align = fui::TextAlign::Center;
    screen.button(resetBtn,
                  fui::makeRect(static_cast<int16_t>(actionRow.x + (quintW + toybox::kGutter) * 4), actionRow.y,
                                quintW, actionRow.height));

    // Draw Pile (Left) & Discard Pile (Right)
    const fui::Rect dPile = drawPileRect();
    const fui::Rect discardPileRect = fui::makeRect(308, 160, cardart::kCardW, cardart::kCardH);

    if (model.state.deckCount > 0) {
      cardart::drawCardBack(screen, dPile, cardart::kCardH);
    } else {
      cardart::drawCardSlot(screen, dPile);
    }

    if (model.state.discardTop != cards::kNoCard) {
      cardart::drawCardFace(screen, discardPileRect, model.state.discardTop, cardart::kCardH);
    } else {
      cardart::drawCardSlot(screen, discardPileRect);
    }

    // Pile labels
    char deckLabel[24];
    std::snprintf(deckLabel, sizeof(deckLabel), "DECK: %d CARDS", model.state.deckCount);
    fui::TextStyle pileStyle;
    pileStyle.font = toybox::kTileFont;
    pileStyle.color = fui::Color::Black;
    pileStyle.align = fui::TextAlign::Center;
    screen.target().text(fui::makeRect(40, static_cast<int16_t>(dPile.bottom() + 6), 172, 20), deckLabel,
                         pileStyle);
    screen.target().text(fui::makeRect(268, static_cast<int16_t>(discardPileRect.bottom() + 6), 172, 20),
                         "IN PLAY / DISCARD", pileStyle);

    // Separator line
    const int16_t divY = static_cast<int16_t>(dPile.bottom() + 36);
    screen.target().fill(fui::makeRect(tabRow.x, divY, tabRow.width, toybox::kHairline),
                         fui::Paint::solid(fui::Color::Black));

    // Hand section label
    char handLabel[64];
    if (model.state.handHidden) {
      std::snprintf(handLabel, sizeof(handLabel), "YOUR HAND (%d/5) [CONCEALED] - TAP PEEK", model.state.handCount);
    } else {
      std::snprintf(handLabel, sizeof(handLabel), "YOUR HAND (%d/5) - TAP CARD TO PLAY", model.state.handCount);
    }
    screen.target().text(fui::makeRect(tabRow.x, static_cast<int16_t>(divY + 12), tabRow.width, 24), handLabel,
                         pileStyle);

    // Hand cards
    for (int i = 0; i < sandbox::kMaxHand; ++i) {
      const fui::Rect cardBox = handCardRect(i);
      if (i < model.state.handCount) {
        if (model.state.handHidden) {
          cardart::drawCardBack(screen, cardBox, cardBox.height);
        } else {
          cardart::drawCardFace(screen, cardBox, model.state.hand[i], cardBox.height);
        }
      } else {
        cardart::drawCardSlot(screen, cardBox);
      }
    }
    return;
  }

  // -------------------------------------------------------------------------
  // TAB 2: COUNTERS
  // -------------------------------------------------------------------------
  if (model.state.mode == sandbox::ToolMode::Counters) {
    const fui::Rect actionRow = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);
    const int16_t thirdW = static_cast<int16_t>((actionRow.width - 2 * toybox::kGutter) / 3);

    fui::ButtonProps nextBtn;
    nextBtn.label = "NEXT TURN";
    nextBtn.action = ActionNextPlayer;
    nextBtn.text.font = toybox::kTileFont;
    nextBtn.text.align = fui::TextAlign::Center;
    screen.button(nextBtn, fui::makeRect(actionRow.x, actionRow.y, thirdW, actionRow.height));

    fui::ButtonProps r20Btn;
    r20Btn.label = "RESET: 20";
    r20Btn.action = ActionResetScores20;
    r20Btn.text.font = toybox::kTileFont;
    r20Btn.text.align = fui::TextAlign::Center;
    screen.button(r20Btn,
                  fui::makeRect(static_cast<int16_t>(actionRow.x + thirdW + toybox::kGutter), actionRow.y, thirdW,
                                actionRow.height));

    fui::ButtonProps r0Btn;
    r0Btn.label = "RESET: 0";
    r0Btn.action = ActionResetScores0;
    r0Btn.text.font = toybox::kTileFont;
    r0Btn.text.align = fui::TextAlign::Center;
    screen.button(r0Btn,
                  fui::makeRect(static_cast<int16_t>(actionRow.x + (thirdW + toybox::kGutter) * 2), actionRow.y,
                                thirdW, actionRow.height));

    // 4 Player Rows
    constexpr int16_t kRowH = 92;
    constexpr int16_t kRowGap = 12;
    const int16_t startY = static_cast<int16_t>(tabRow.bottom() + 14);

    for (int p = 0; p < sandbox::kMaxPlayers; ++p) {
      const int16_t y = static_cast<int16_t>(startY + p * (kRowH + kRowGap));
      const fui::Rect rowBox = fui::makeRect(tabRow.x, y, tabRow.width, kRowH);
      const bool isTurn = (model.state.activePlayer == p);

      screen.target().fill(rowBox, fui::Paint::solid(fui::Color::White));
      screen.target().stroke(rowBox, fui::Paint::solid(fui::Color::Black), isTurn ? 3 : 1);

      // Left Player Badge
      char pName[24];
      std::snprintf(pName, sizeof(pName), "%s P%d", isTurn ? "[TURN]" : " ", p + 1);
      fui::TextStyle nameStyle;
      nameStyle.font = toybox::kTileFont;
      nameStyle.color = fui::Color::Black;
      nameStyle.align = fui::TextAlign::Left;
      screen.target().text(fui::makeRect(static_cast<int16_t>(rowBox.x + 12), static_cast<int16_t>(rowBox.y + 12),
                                         100, 24),
                           pName, nameStyle);

      // Center Big Score
      char scoreBuf[16];
      std::snprintf(scoreBuf, sizeof(scoreBuf), "%d", model.state.scores[p]);
      fui::TextStyle scoreStyle;
      scoreStyle.font = toybox::kDisplayFont;
      scoreStyle.color = fui::Color::Black;
      scoreStyle.align = fui::TextAlign::Center;
      screen.target().text(fui::makeRect(static_cast<int16_t>(rowBox.x + 105), static_cast<int16_t>(rowBox.y + 24),
                                         130, 44),
                           scoreBuf, scoreStyle);

      // Right 2 Stepper Buttons (- and +)
      constexpr int16_t kBtnW = 64;
      constexpr int16_t kBtnH = 48;
      constexpr int16_t kBtnGap = 12;
      const int16_t btnBaseX = static_cast<int16_t>(rowBox.right() - (2 * kBtnW + kBtnGap + 16));
      const int16_t btnY = static_cast<int16_t>(rowBox.y + (rowBox.height - kBtnH) / 2);

      fui::ButtonProps minusBtn;
      minusBtn.label = "-1";
      minusBtn.action = static_cast<int16_t>(ActionScoreMinus + p);
      minusBtn.text.font = toybox::kTileFont;
      minusBtn.text.align = fui::TextAlign::Center;
      screen.button(minusBtn, fui::makeRect(btnBaseX, btnY, kBtnW, kBtnH));

      fui::ButtonProps plusBtn;
      plusBtn.label = "+1";
      plusBtn.action = static_cast<int16_t>(ActionScorePlus + p);
      plusBtn.text.font = toybox::kTileFont;
      plusBtn.text.align = fui::TextAlign::Center;
      screen.button(plusBtn, fui::makeRect(static_cast<int16_t>(btnBaseX + kBtnW + kBtnGap), btnY, kBtnW, kBtnH));
    }
    return;
  }

  // -------------------------------------------------------------------------
  // TAB 3: BOARD
  // -------------------------------------------------------------------------
  if (model.state.mode == sandbox::ToolMode::Board) {
    const fui::Rect actionRow = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);
    const int16_t thirdW = static_cast<int16_t>((actionRow.width - 2 * toybox::kGutter) / 3);

    char colorBuf[24];
    std::snprintf(colorBuf, sizeof(colorBuf), "COLOR: %s", model.state.spawnColor == 0 ? "WHITE" : "BLACK");
    fui::ButtonProps colorBtn;
    colorBtn.label = colorBuf;
    colorBtn.action = ActionToggleSpawnColor;
    colorBtn.text.font = toybox::kTileFont;
    colorBtn.text.align = fui::TextAlign::Center;
    screen.button(colorBtn, fui::makeRect(actionRow.x, actionRow.y, thirdW, actionRow.height));

    fui::ButtonProps chkBtn;
    chkBtn.label = "CHECKERS";
    chkBtn.action = ActionCheckersPreset;
    chkBtn.text.font = toybox::kTileFont;
    chkBtn.text.align = fui::TextAlign::Center;
    screen.button(chkBtn,
                  fui::makeRect(static_cast<int16_t>(actionRow.x + thirdW + toybox::kGutter), actionRow.y, thirdW,
                                actionRow.height));

    fui::ButtonProps clrBtn;
    clrBtn.label = "CLEAR";
    clrBtn.action = ActionClearBoard;
    clrBtn.text.font = toybox::kTileFont;
    clrBtn.text.align = fui::TextAlign::Center;
    screen.button(clrBtn,
                  fui::makeRect(static_cast<int16_t>(actionRow.x + (thirdW + toybox::kGutter) * 2), actionRow.y,
                                thirdW, actionRow.height));

    // Board Frame & Checkered Pattern
    const fui::Rect board = boardRect();
    screen.target().stroke(fui::makeRect(static_cast<int16_t>(board.x - 2), static_cast<int16_t>(board.y - 2),
                                         static_cast<int16_t>(board.width + 4),
                                         static_cast<int16_t>(board.height + 4)),
                           fui::Paint::solid(fui::Color::Black), 2);

    for (int r = 0; r < 8; ++r) {
      for (int c = 0; c < 8; ++c) {
        const fui::Rect sq = fui::makeRect(static_cast<int16_t>(board.x + c * kSquareSide),
                                           static_cast<int16_t>(board.y + r * kSquareSide), kSquareSide, kSquareSide);
        if ((r + c) % 2 == 1) {
          screen.target().fill(sq, fui::Paint::dither(fui::Color::LightGray));
        } else {
          screen.target().fill(sq, fui::Paint::solid(fui::Color::White));
        }

        const int pos = r * 8 + c;
        const int tokIdx = sandbox::findTokenAt(model.state, static_cast<uint8_t>(pos));
        if (tokIdx != -1) {
          const uint8_t color = model.state.tokens[tokIdx].color;
          const int16_t cx = static_cast<int16_t>(sq.x + kSquareSide / 2);
          const int16_t cy = static_cast<int16_t>(sq.y + kSquareSide / 2);

          if (color == 0) {
            // White piece
            toybox::disc(screen, cx, cy, 18, fui::Color::Black);
            toybox::disc(screen, cx, cy, 15, fui::Color::White);
          } else {
            // Black piece
            toybox::disc(screen, cx, cy, 18, fui::Color::Black);
          }

          if (model.state.selectedToken == tokIdx) {
            toybox::bracket(screen,
                            fui::makeRect(static_cast<int16_t>(cx - 22), static_cast<int16_t>(cy - 22), 44, 44), 8,
                            3);
          }
        }
      }
    }

    // Hint banner
    fui::TextStyle hintStyle;
    hintStyle.font = toybox::kTileFont;
    hintStyle.color = fui::Color::Black;
    hintStyle.align = fui::TextAlign::Center;
    const char* hint = "Tap square to place piece. Tap piece to move.";
    if (model.state.selectedToken != -1) {
      hint = "Piece selected! Tap square to move or capture.";
    }
    screen.target().text(fui::makeRect(tabRow.x, static_cast<int16_t>(board.bottom() + 10), tabRow.width, 24),
                         hint, hintStyle);
    return;
  }
}

}  // namespace sandboxui
