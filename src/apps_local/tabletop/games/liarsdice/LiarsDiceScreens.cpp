#include "LiarsDiceScreens.h"

#include <cstdio>
#include <cstring>

namespace liarsdiceui {

namespace {

void drawDiePips(toybox::Screen& screen, const fui::Rect& box, const int face) {
  constexpr int16_t kPip = 6;
  const int16_t left = static_cast<int16_t>(box.x + 8);
  const int16_t mid = static_cast<int16_t>(box.x + box.width / 2 - kPip / 2);
  const int16_t right = static_cast<int16_t>(box.x + box.width - 8 - kPip);
  const int16_t top = static_cast<int16_t>(box.y + 8);
  const int16_t centre = static_cast<int16_t>(box.y + box.height / 2 - kPip / 2);
  const int16_t bottom = static_cast<int16_t>(box.y + box.height - 8 - kPip);

  static const uint16_t kPips[7] = {
      0,
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

void drawDie(toybox::Screen& screen, const fui::Rect& box, const uint8_t face, const bool highlight) {
  screen.target().fill(box, fui::Paint::solid(fui::Color::White));
  const int16_t strokeW = highlight ? 4 : 2;
  screen.target().stroke(box, fui::Paint::solid(fui::Color::Black), strokeW);
  drawDiePips(screen, box, face);

  if (highlight) {
    // Top-left asterisk or dot mark for matched die
    screen.target().fill(fui::makeRect(static_cast<int16_t>(box.x + 3), static_cast<int16_t>(box.y + 3), 4, 4),
                         fui::Paint::solid(fui::Color::Black));
  }
}

}  // namespace

void buildBoard(toybox::Screen& screen, const ScreenModel& model) {
  const auto& game = model.game;

  // Header band
  fui::HeaderProps header;
  header.title = "LIAR'S DICE";
  header.rightLabel = (model.mode == PlayMode::Nearby)
                          ? "NEARBY LINK"
                          : ((model.mode == PlayMode::SoloVsAI) ? "PERUDO BLUFFING" : "PASS & PLAY");
  header.titleText.font = toybox::kDisplayFont;
  header.titleText.color = fui::Color::White;
  header.titleText.align = fui::TextAlign::Left;
  header.subtitleText.font = toybox::kTileFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});

  // Top Area: Opponent / Bot (North)
  const uint8_t southIdx = (model.humanPlayer > 0) ? 1 : 0;
  const uint8_t northIdx = 1 - southIdx;

  const fui::Rect northRect = screen.takeTop(100);
  screen.target().stroke(northRect, fui::Paint::solid(fui::Color::Black), 1);

  char northTitle[48];
  if (model.mode == PlayMode::SoloVsAI) {
    std::snprintf(northTitle, sizeof(northTitle), "NORTH: AI (%d DICE LEFT)", game.players[northIdx].diceCount);
  } else if (model.mode == PlayMode::Nearby) {
    std::snprintf(northTitle, sizeof(northTitle), "NORTH: OPPONENT (%d DICE LEFT)", game.players[northIdx].diceCount);
  } else {
    std::snprintf(northTitle, sizeof(northTitle), "NORTH: %s (%d DICE LEFT)",
                  northIdx == 0 ? "PLAYER 1" : "PLAYER 2", game.players[northIdx].diceCount);
  }
  fui::TextStyle northStyle;
  northStyle.font = toybox::kTileFont;
  northStyle.color = fui::Color::Black;
  northStyle.align = fui::TextAlign::Left;
  screen.target().text(fui::makeRect(static_cast<int16_t>(northRect.x + 10), static_cast<int16_t>(northRect.y + 8),
                                     static_cast<int16_t>(northRect.width - 20), 20),
                       northTitle, northStyle);

  if (game.phase == liarsdice::Phase::Showdown || game.phase == liarsdice::Phase::GameOver) {
    // Show revealed opponent dice
    constexpr int16_t kDieSize = 44;
    constexpr int16_t kDieGap = 10;
    const uint8_t count = game.players[northIdx].diceCount;
    if (count > 0) {
      const int16_t totalW = static_cast<int16_t>(count * kDieSize + (count - 1) * kDieGap);
      const int16_t startX = static_cast<int16_t>(northRect.x + (northRect.width - totalW) / 2);
      const int16_t startY = static_cast<int16_t>(northRect.y + 40);
      for (uint8_t d = 0; d < count; ++d) {
        const uint8_t val = game.players[northIdx].dice[d];
        const bool matched = (val == game.currentBid.face) || (game.currentBid.face != 1 && val == 1);
        drawDie(screen, fui::makeRect(static_cast<int16_t>(startX + d * (kDieSize + kDieGap)), startY, kDieSize, kDieSize),
                val, matched);
      }
    }
  } else {
    // Concealed cup
    fui::TextStyle cupStyle;
    cupStyle.font = toybox::kUiFont;
    cupStyle.color = fui::Color::Black;
    cupStyle.align = fui::TextAlign::Center;
    screen.target().text(fui::makeRect(northRect.x, static_cast<int16_t>(northRect.y + 46), northRect.width, 32),
                         "[ CONCEALED IN CUP ]", cupStyle);
  }

  screen.takeTop(10);  // spacing

  // Middle Area: Current Bid & Showdown Banner
  const fui::Rect midRect = screen.takeTop(150);
  screen.target().stroke(midRect, fui::Paint::solid(fui::Color::Black), 2);

  fui::TextStyle bidHeaderStyle;
  bidHeaderStyle.font = toybox::kTileFont;
  bidHeaderStyle.color = fui::Color::Black;
  bidHeaderStyle.align = fui::TextAlign::Center;

  if (game.phase == liarsdice::Phase::Showdown || game.phase == liarsdice::Phase::GameOver) {
    // Showdown results
    screen.target().fill(fui::makeRect(midRect.x, midRect.y, midRect.width, 28), fui::Paint::solid(fui::Color::Black));
    fui::TextStyle whiteHeader;
    whiteHeader.font = toybox::kTileFont;
    whiteHeader.color = fui::Color::White;
    whiteHeader.align = fui::TextAlign::Center;
    screen.target().text(fui::makeRect(midRect.x, static_cast<int16_t>(midRect.y + 4), midRect.width, 24),
                         "*** SHOWDOWN! ***", whiteHeader);

    char resLine1[96];
    const char* chalName = (game.challenger == southIdx) ? "SOUTH (YOU)" : (model.mode == PlayMode::SoloVsAI ? "NORTH (AI)" : "NORTH (OPPONENT)");
    std::snprintf(resLine1, sizeof(resLine1), "CHALLENGE BY %s: 'LIAR!'", chalName);
    screen.target().text(fui::makeRect(midRect.x, static_cast<int16_t>(midRect.y + 36), midRect.width, 24), resLine1,
                         bidHeaderStyle);

    char resLine2[96];
    std::snprintf(resLine2, sizeof(resLine2), "BID: %d OF [F%d]  |  ACTUAL: %d FOUND", game.currentBid.count,
                  game.currentBid.face, game.totalMatchingDice);
    screen.target().text(fui::makeRect(midRect.x, static_cast<int16_t>(midRect.y + 66), midRect.width, 24), resLine2,
                         bidHeaderStyle);

    char resLine3[96];
    if (game.phase == liarsdice::Phase::GameOver) {
      const char* winName = (game.winner == southIdx) ? "YOU WIN!" : "OPPONENT WINS!";
      std::snprintf(resLine3, sizeof(resLine3), "GAME OVER! %s", winName);
    } else {
      const char* loseName = (game.roundLoser == southIdx) ? "YOU WERE" : "OPPONENT WAS";
      std::snprintf(resLine3, sizeof(resLine3), "%s %s -> LOSES 1 DIE", loseName,
                    game.challengeSuccessful ? "BLUFFING" : "CORRECT");
    }
    fui::TextStyle boldRes;
    boldRes.font = toybox::kDisplayFont;
    boldRes.color = fui::Color::Black;
    boldRes.align = fui::TextAlign::Center;
    screen.target().text(fui::makeRect(midRect.x, static_cast<int16_t>(midRect.y + 98), midRect.width, 36), resLine3,
                         boldRes);
  } else {
    // Active bidding banner
    screen.target().text(fui::makeRect(midRect.x, static_cast<int16_t>(midRect.y + 10), midRect.width, 24),
                         "CURRENT TABLE BID:", bidHeaderStyle);

    if (game.currentBid.count == 0) {
      fui::TextStyle bigStyle;
      bigStyle.font = toybox::kDisplayFont;
      bigStyle.color = fui::Color::Black;
      bigStyle.align = fui::TextAlign::Center;
      screen.target().text(fui::makeRect(midRect.x, static_cast<int16_t>(midRect.y + 40), midRect.width, 36),
                           "NO BID YET", bigStyle);

      screen.target().text(fui::makeRect(midRect.x, static_cast<int16_t>(midRect.y + 102), midRect.width, 24),
                           "Open the bidding round below!", bidHeaderStyle);
    } else {
      // Big bid display: "4 x [ DIE ]"
      char bidStr[32];
      std::snprintf(bidStr, sizeof(bidStr), "%d  x", game.currentBid.count);
      fui::TextStyle bigStyle;
      bigStyle.font = toybox::kDisplayFont;
      bigStyle.color = fui::Color::Black;
      bigStyle.align = fui::TextAlign::Center;
      screen.target().text(fui::makeRect(static_cast<int16_t>(midRect.x + 80), static_cast<int16_t>(midRect.y + 42), 120, 44),
                           bidStr, bigStyle);

      // Die graphic for bid face
      const fui::Rect bidDieBox = fui::makeRect(static_cast<int16_t>(midRect.x + 220), static_cast<int16_t>(midRect.y + 40), 48, 48);
      drawDie(screen, bidDieBox, game.currentBid.face, true);

      char byStr[48];
      const char* bidderName = (game.currentBid.bidder == southIdx)
                                   ? "SOUTH (YOU)"
                                   : (model.mode == PlayMode::SoloVsAI ? "NORTH (AI)" : "NORTH (OPPONENT)");
      std::snprintf(byStr, sizeof(byStr), "BID BY: %s", bidderName);
      screen.target().text(fui::makeRect(midRect.x, static_cast<int16_t>(midRect.y + 106), midRect.width, 24), byStr,
                           bidHeaderStyle);
    }
  }

  screen.takeTop(10);  // spacing

  // South Area: Human player / P1
  const fui::Rect southRect = screen.takeTop(110);
  screen.target().stroke(southRect, fui::Paint::solid(fui::Color::Black), 1);

  char southTitle[48];
  if (model.mode == PlayMode::SoloVsAI || model.mode == PlayMode::Nearby) {
    std::snprintf(southTitle, sizeof(southTitle), "SOUTH: YOU (%d DICE LEFT)", game.players[southIdx].diceCount);
  } else {
    std::snprintf(southTitle, sizeof(southTitle), "SOUTH: %s (%d DICE LEFT)",
                  southIdx == 0 ? "PLAYER 1" : "PLAYER 2", game.players[southIdx].diceCount);
  }
  screen.target().text(fui::makeRect(static_cast<int16_t>(southRect.x + 10), static_cast<int16_t>(southRect.y + 8),
                                     static_cast<int16_t>(southRect.width - 20), 20),
                       southTitle, northStyle);

  if (model.shieldDice) {
    fui::TextStyle shieldStyle;
    shieldStyle.font = toybox::kUiFont;
    shieldStyle.color = fui::Color::Black;
    shieldStyle.align = fui::TextAlign::Center;
    screen.target().text(fui::makeRect(southRect.x, static_cast<int16_t>(southRect.y + 50), southRect.width, 32),
                         "[ DICE SHIELDED - TAP PEEK ]", shieldStyle);
  } else {
    // Show player's dice
    constexpr int16_t kDieSize = 48;
    constexpr int16_t kDieGap = 12;
    const uint8_t count = game.players[southIdx].diceCount;
    if (count > 0) {
      const int16_t totalW = static_cast<int16_t>(count * kDieSize + (count - 1) * kDieGap);
      const int16_t startX = static_cast<int16_t>(southRect.x + (southRect.width - totalW) / 2);
      const int16_t startY = static_cast<int16_t>(southRect.y + 44);
      for (uint8_t d = 0; d < count; ++d) {
        const uint8_t val = game.players[southIdx].dice[d];
        const bool matched = (game.phase == liarsdice::Phase::Showdown) &&
                             ((val == game.currentBid.face) || (game.currentBid.face != 1 && val == 1));
        drawDie(screen, fui::makeRect(static_cast<int16_t>(startX + d * (kDieSize + kDieGap)), startY, kDieSize, kDieSize),
                val, matched);
      }
    }
  }

  // Bottom Navigation Buttons
  fui::ButtonProps lobbyBtn;
  lobbyBtn.label = "BACK TO LOBBY";
  lobbyBtn.action = ActionBackToLobby;
  screen.button(lobbyBtn, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  const fui::Rect actionRow = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);

  if (game.phase == liarsdice::Phase::Showdown) {
    fui::ButtonProps nextRoundBtn;
    if (model.mode == PlayMode::Nearby && !model.isYourTurn) {
      nextRoundBtn.label = "WAITING FOR OPPONENT...";
      nextRoundBtn.action = 0;
    } else {
      nextRoundBtn.label = "NEXT ROUND";
      nextRoundBtn.action = ActionNextRound;
    }
    screen.button(nextRoundBtn, actionRow);
    return;
  }

  if (game.phase == liarsdice::Phase::GameOver) {
    fui::ButtonProps playAgainBtn;
    playAgainBtn.label = "PLAY AGAIN";
    playAgainBtn.action = ActionRestart;
    screen.button(playAgainBtn, actionRow);
    return;
  }

  // Active Bidding Controls
  const int16_t half = static_cast<int16_t>((actionRow.width - toybox::kGutter) / 2);
  fui::ButtonProps restartBtn;
  restartBtn.label = "NEW GAME";
  restartBtn.action = ActionRestart;
  screen.button(restartBtn, fui::makeRect(actionRow.x, actionRow.y, half, actionRow.height));

  fui::ButtonProps modeBtn;
  modeBtn.label = (model.mode == PlayMode::Nearby) ? "MODE: NEARBY" : ((model.mode == PlayMode::SoloVsAI) ? "MODE: SOLO" : "MODE: 2P");
  modeBtn.action = ActionToggleMode;
  screen.button(modeBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + half + toybox::kGutter), actionRow.y, half, actionRow.height));

  // If it's human's turn in Bidding phase:
  const bool isHumanTurn = (model.mode == PlayMode::Nearby)
                               ? (game.activePlayer == model.humanPlayer)
                               : ((game.activePlayer == 0) && !model.isAiThinking);

  if (isHumanTurn) {
    // Action Buttons: MAKE BID vs LIAR!
    const fui::Rect bidActionRow = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);
    const int16_t halfAction = static_cast<int16_t>((bidActionRow.width - toybox::kGutter) / 2);

    const liarsdice::Bid proposedBid{model.selectedCount, model.selectedFace, 0};
    const bool valid = liarsdice::isValidBid(game, proposedBid);

    fui::ButtonProps makeBidBtn;
    makeBidBtn.label = valid ? "MAKE BID" : "(INVALID BID)";
    makeBidBtn.action = ActionMakeBid;
    screen.button(makeBidBtn, fui::makeRect(bidActionRow.x, bidActionRow.y, halfAction, bidActionRow.height));

    fui::ButtonProps challengeBtn;
    challengeBtn.label = (game.currentBid.count > 0) ? "LIAR! (CALL)" : "LIAR! (N/A)";
    challengeBtn.action = ActionChallenge;
    screen.button(challengeBtn, fui::makeRect(static_cast<int16_t>(bidActionRow.x + halfAction + toybox::kGutter),
                                              bidActionRow.y, halfAction, bidActionRow.height));

    // Face Selector Row: 6 face buttons
    const fui::Rect faceRow = screen.takeBottom(36, toybox::kGutter);
    const int16_t faceBtnW = static_cast<int16_t>((faceRow.width - 5 * 6) / 6);
    for (uint8_t f = 1; f <= 6; ++f) {
      const int16_t fx = static_cast<int16_t>(faceRow.x + (f - 1) * (faceBtnW + 6));
      const fui::Rect fBox = fui::makeRect(fx, faceRow.y, faceBtnW, faceRow.height);

      fui::ButtonProps fBtn;
      char fLabel[8];
      std::snprintf(fLabel, sizeof(fLabel), "%s%d", (f == 1 ? "*" : ""), f);
      fBtn.label = fLabel;
      fBtn.action = static_cast<fui::ActionId>(ActionFaceBase + (f - 1));
      fBtn.text.font = toybox::kTileFont;
      fBtn.text.align = fui::TextAlign::Center;
      screen.button(fBtn, fBox);

      if (model.selectedFace == f) {
        screen.target().stroke(fBox, fui::Paint::solid(fui::Color::Black), 3);
      }
    }

    // Count Stepper Row: [-] [ COUNT: N ] [+]
    const fui::Rect countRow = screen.takeBottom(36, toybox::kGutter);
    fui::ButtonProps minusBtn;
    minusBtn.label = "[-]";
    minusBtn.action = ActionCountMinus;
    screen.button(minusBtn, fui::makeRect(countRow.x, countRow.y, 60, countRow.height));

    fui::ButtonProps plusBtn;
    plusBtn.label = "[+]";
    plusBtn.action = ActionCountPlus;
    screen.button(plusBtn, fui::makeRect(static_cast<int16_t>(countRow.right() - 60), countRow.y, 60, countRow.height));

    char countStr[32];
    std::snprintf(countStr, sizeof(countStr), "QUANTITY: %d DICE", model.selectedCount);
    fui::TextStyle countStyle;
    countStyle.font = toybox::kTileFont;
    countStyle.color = fui::Color::Black;
    countStyle.align = fui::TextAlign::Center;
    screen.target().text(fui::makeRect(static_cast<int16_t>(countRow.x + 64), static_cast<int16_t>(countRow.y + 6),
                                       static_cast<int16_t>(countRow.width - 128), 24),
                         countStr, countStyle);
  } else {
    // Waiting message
    fui::TextStyle waitStyle;
    waitStyle.font = toybox::kTileFont;
    waitStyle.color = fui::Color::Black;
    waitStyle.align = fui::TextAlign::Center;
    screen.target().text(screen.takeBottom(36, toybox::kGutter),
                         model.isAiThinking ? "AI calculating probabilities..." : "Opponent turn...", waitStyle);
  }
}

}  // namespace liarsdiceui
