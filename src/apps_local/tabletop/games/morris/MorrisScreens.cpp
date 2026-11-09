#include "MorrisScreens.h"

#include <cstdio>
#include <cstring>

#include "../../../ui/ToyboxFormat.h"

namespace morrisui {

namespace {

constexpr int16_t kBoardLeft = 16;
constexpr int16_t kBoardTop = 144;
constexpr int16_t kBoardSide = 448;

const fui::Point kVertexPoints[24] = {
    // Outer (0..7)
    {40, 168}, {240, 168}, {440, 168},
    {440, 368},
    {440, 568}, {240, 568}, {40, 568},
    {40, 368},

    // Middle (8..15)
    {104, 232}, {240, 232}, {376, 232},
    {376, 368},
    {376, 504}, {240, 504}, {104, 504},
    {104, 368},

    // Inner (16..23)
    {168, 296}, {240, 296}, {312, 296},
    {312, 368},
    {312, 440}, {240, 440}, {168, 440},
    {168, 368},
};

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

}  // namespace

fui::Rect boardRect() {
  return fui::makeRect(kBoardLeft, kBoardTop, kBoardSide, kBoardSide);
}

fui::Point vertexPos(const int v, const bool flipBoard) {
  if (v >= 0 && v < 24) {
    const fui::Point pt = kVertexPoints[v];
    if (flipBoard) {
      return fui::Point{static_cast<int16_t>(480 - pt.x), static_cast<int16_t>(736 - pt.y)};
    }
    return pt;
  }
  return fui::Point{0, 0};
}

int vertexAt(const int x, const int y, const bool flipBoard) {
  constexpr int kRadiusSq = 28 * 28;  // 784
  for (int v = 0; v < 24; ++v) {
    const fui::Point pt = vertexPos(v, flipBoard);
    const int dx = x - pt.x;
    const int dy = y - pt.y;
    if (dx * dx + dy * dy <= kRadiusSq) {
      return v;
    }
  }
  return -1;
}

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  const char* modeLabel = (model.mode == PlayMode::Nearby)
                              ? "NEARBY LINK"
                              : ((model.mode == PlayMode::SoloVsAI) ? "SOLO AI" : "PASS & PLAY");
  toyboxChrome(screen, "NINE MEN'S MORRIS", modeLabel);

  // Score / Status row
  const fui::Rect scoreRow = screen.takeTop(34);

  // White status (left)
  char whiteText[24];
  const int wPlaced = model.game.placed[morris::kWhite];
  const int wBoard = morris::pieceCount(model.game.pieces[morris::kWhite]);
  if (wPlaced < morris::kMenPerPlayer) {
    std::snprintf(whiteText, sizeof(whiteText), "W: PLACE %d/9", wPlaced);
  } else {
    std::snprintf(whiteText, sizeof(whiteText), "W: MEN %d", wBoard);
  }
  fui::TextStyle leftStyle;
  leftStyle.font = toybox::kTileFont;
  leftStyle.color = fui::Color::Black;
  leftStyle.align = fui::TextAlign::Left;
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.x + 8), scoreRow.y, 110, scoreRow.height),
                       whiteText, leftStyle);

  // Black status (right)
  char blackText[24];
  const int bPlaced = model.game.placed[morris::kBlack];
  const int bBoard = morris::pieceCount(model.game.pieces[morris::kBlack]);
  if (bPlaced < morris::kMenPerPlayer) {
    std::snprintf(blackText, sizeof(blackText), "B: PLACE %d/9", bPlaced);
  } else {
    std::snprintf(blackText, sizeof(blackText), "B: MEN %d", bBoard);
  }
  fui::TextStyle rightStyle;
  rightStyle.font = toybox::kTileFont;
  rightStyle.color = fui::Color::Black;
  rightStyle.align = fui::TextAlign::Right;
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.right() - 118), scoreRow.y, 110, scoreRow.height),
                       blackText, rightStyle);

  // Center turn / status message
  char statusBuf[48] = {};
  if (model.game.isOver) {
    const morris::Outcome res = morris::outcome(model.game);
    if (res == morris::Outcome::Draw) {
      std::snprintf(statusBuf, sizeof(statusBuf), "GAME OVER: DRAW");
    } else if (model.mode == PlayMode::Nearby || model.mode == PlayMode::SoloVsAI) {
      const bool won = (res == morris::Outcome::WhiteWon && model.humanPlayer == morris::kWhite) ||
                       (res == morris::Outcome::BlackWon && model.humanPlayer == morris::kBlack);
      std::snprintf(statusBuf, sizeof(statusBuf), "%s",
                    won ? "YOU WIN!" : (model.mode == PlayMode::Nearby ? "OPPONENT WINS" : "AI WINS"));
    } else {
      std::snprintf(statusBuf, sizeof(statusBuf), "%s WINS",
                    res == morris::Outcome::WhiteWon ? "WHITE" : "BLACK");
    }
  } else if (model.game.pendingRemoval) {
    if (model.mode == PlayMode::Nearby) {
      const bool yourTurn = model.game.turn == model.humanPlayer;
      std::snprintf(statusBuf, sizeof(statusBuf), "%s", yourTurn ? "MILL! REMOVE ENEMY PIECE" : "OPPONENT'S MILL");
    } else {
      std::snprintf(statusBuf, sizeof(statusBuf), "MILL! REMOVE ENEMY");
    }
  } else if (model.isAiThinking) {
    std::snprintf(statusBuf, sizeof(statusBuf), "AI THINKING...");
  } else if (model.mode == PlayMode::Nearby) {
    const bool yourTurn = model.game.turn == model.humanPlayer;
    std::snprintf(statusBuf, sizeof(statusBuf), "%s", yourTurn ? "YOUR TURN" : "OPPONENT'S TURN");
  } else if (model.mode == PlayMode::SoloVsAI) {
    const bool yourTurn = model.game.turn == model.humanPlayer;
    std::snprintf(statusBuf, sizeof(statusBuf), "%s", yourTurn ? "YOUR TURN (WHITE)" : "AI TURN (BLACK)");
  } else {
    std::snprintf(statusBuf, sizeof(statusBuf), "%s",
                  model.game.turn == morris::kWhite ? "PASS TO WHITE" : "PASS TO BLACK");
  }

  fui::TextStyle centerStyle;
  centerStyle.font = toybox::kTileFont;
  centerStyle.color = fui::Color::Black;
  centerStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.x + 115), scoreRow.y,
                                     static_cast<int16_t>(scoreRow.width - 230), scoreRow.height),
                       statusBuf, centerStyle);

  // Hairline separator under score row
  screen.target().fill(fui::makeRect(scoreRow.x, static_cast<int16_t>(scoreRow.bottom() + 1),
                                     scoreRow.width, toybox::kHairline),
                       fui::Paint::solid(fui::Color::Black));

  // Board Drawing
  const fui::Rect board = boardRect();
  screen.target().fill(board, fui::Paint::dither(fui::Color::LightGray));
  screen.target().stroke(fui::makeRect(static_cast<int16_t>(board.x - toybox::kFrame),
                                       static_cast<int16_t>(board.y - toybox::kFrame),
                                       static_cast<int16_t>(board.width + toybox::kFrame * 2),
                                       static_cast<int16_t>(board.height + toybox::kFrame * 2)),
                         fui::Paint::solid(fui::Color::Black), toybox::kFrame);

  // 3 Concentric Squares (3px stroke)
  // Outer square
  screen.target().stroke(fui::makeRect(40, 168, 400, 400), fui::Paint::solid(fui::Color::Black), 3);
  // Middle square
  screen.target().stroke(fui::makeRect(104, 232, 272, 272), fui::Paint::solid(fui::Color::Black), 3);
  // Inner square
  screen.target().stroke(fui::makeRect(168, 296, 144, 144), fui::Paint::solid(fui::Color::Black), 3);

  // 4 Radial Cross Lines (3px stroke)
  // Top radial: (240, 168) to (240, 296)
  screen.target().fill(fui::makeRect(239, 168, 3, 296 - 168), fui::Paint::solid(fui::Color::Black));
  // Bottom radial: (240, 440) to (240, 568)
  screen.target().fill(fui::makeRect(239, 440, 3, 568 - 440), fui::Paint::solid(fui::Color::Black));
  // Left radial: (40, 368) to (168, 368)
  screen.target().fill(fui::makeRect(40, 367, 168 - 40, 3), fui::Paint::solid(fui::Color::Black));
  // Right radial: (312, 368) to (440, 368)
  screen.target().fill(fui::makeRect(312, 367, 440 - 312, 3), fui::Paint::solid(fui::Color::Black));

  // Determine legal highlights
  uint32_t highlightMask = 0;
  if (!model.game.isOver && !model.isAiThinking) {
    if (model.game.pendingRemoval) {
      highlightMask = morris::removablePieces(model.game, morris::opponent(model.game.turn));
    } else {
      const morris::Phase phase = morris::playerPhase(model.game, model.game.turn);
      if (phase == morris::Phase::Placing) {
        highlightMask = morris::legalPlacements(model.game);
      } else if (model.game.selected >= 0) {
        highlightMask = morris::legalDestinations(model.game, model.game.selected);
      }
    }
  }

  // Draw 24 Vertices
  for (int v = 0; v < 24; ++v) {
    const fui::Point pt = vertexPos(v, model.flipBoard);
    const uint8_t piece = morris::pieceAt(model.game, v);

    if (piece == morris::kNone) {
      if (highlightMask & (1U << v)) {
        // Legal placement or destination: bold circle
        toybox::disc(screen, pt.x, pt.y, 8, fui::Color::Black);
      } else {
        // Normal vacant vertex dot
        toybox::disc(screen, pt.x, pt.y, 5, fui::Color::Black);
        toybox::disc(screen, pt.x, pt.y, 3, fui::Color::White);
      }
    } else {
      // Piece present
      if (piece == morris::kWhite) {
        toybox::disc(screen, pt.x, pt.y, 18, fui::Color::Black);
        toybox::disc(screen, pt.x, pt.y, 15, fui::Color::White);
      } else {
        toybox::disc(screen, pt.x, pt.y, 18, fui::Color::Black);
      }

      // If part of mill, draw inner pip
      if (morris::isPartOfMill(model.game.pieces[piece], v)) {
        toybox::disc(screen, pt.x, pt.y, 4, piece == morris::kWhite ? fui::Color::Black : fui::Color::White);
      }

      // If selected piece in hand
      if (model.game.selected == v) {
        toybox::bracket(screen, fui::makeRect(static_cast<int16_t>(pt.x - 22),
                                              static_cast<int16_t>(pt.y - 22), 44, 44), 8, 3);
      }

      // If removable
      if (model.game.pendingRemoval && (highlightMask & (1U << v))) {
        toybox::bracket(screen, fui::makeRect(static_cast<int16_t>(pt.x - 22),
                                              static_cast<int16_t>(pt.y - 22), 44, 44), 10, 3);
      }
    }
  }

  // Bottom action buttons
  fui::ButtonProps lobbyBtn;
  lobbyBtn.label = "BACK TO LOBBY";
  lobbyBtn.action = ActionBackToLobby;
  screen.button(lobbyBtn, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  const fui::Rect actionRow = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);

  if (model.mode == PlayMode::TwoPlayerPassAndPlay) {
    const int16_t third = static_cast<int16_t>((actionRow.width - 2 * toybox::kGutter) / 3);
    fui::ButtonProps restartBtn;
    restartBtn.label = "NEW GAME";
    restartBtn.action = ActionRestart;
    restartBtn.text.font = toybox::kTileFont;
    restartBtn.text.align = fui::TextAlign::Center;
    screen.button(restartBtn, fui::makeRect(actionRow.x, actionRow.y, third, actionRow.height));

    fui::ButtonProps flipBtn;
    flipBtn.label = "FLIP";
    flipBtn.action = ActionFlipBoard;
    flipBtn.text.font = toybox::kTileFont;
    flipBtn.text.align = fui::TextAlign::Center;
    screen.button(flipBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + third + toybox::kGutter),
                                         actionRow.y, third, actionRow.height));

    fui::ButtonProps modeBtn;
    modeBtn.label = "MODE: 2P";
    modeBtn.action = ActionToggleMode;
    modeBtn.text.font = toybox::kTileFont;
    modeBtn.text.align = fui::TextAlign::Center;
    screen.button(modeBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + (third + toybox::kGutter) * 2),
                                        actionRow.y, third, actionRow.height));
  } else {
    const int16_t half = static_cast<int16_t>((actionRow.width - toybox::kGutter) / 2);
    fui::ButtonProps leftBtn;
    leftBtn.label = "NEW GAME";
    leftBtn.action = ActionRestart;
    screen.button(leftBtn, fui::makeRect(actionRow.x, actionRow.y, half, actionRow.height));

    fui::ButtonProps rightBtn;
    rightBtn.label = (model.mode == PlayMode::Nearby)
                         ? "MODE: NEARBY"
                         : ((model.mode == PlayMode::SoloVsAI) ? "MODE: SOLO" : "MODE: 2P");
    rightBtn.action = ActionToggleMode;
    screen.button(rightBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + half + toybox::kGutter),
                                          actionRow.y, half, actionRow.height));
  }

  // Hint banner
  char hintBuf[64] = {};
  if (model.game.isOver) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Game Over! Tap NEW GAME to play again.");
  } else if (model.mode == PlayMode::Nearby && model.game.turn != model.humanPlayer) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Opponent's turn (waiting for move...)");
  } else if (model.game.pendingRemoval) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Formed a Mill! Tap bracketed enemy piece to remove it.");
  } else if (model.mode == PlayMode::SoloVsAI && model.game.turn != model.humanPlayer) {
    std::snprintf(hintBuf, sizeof(hintBuf), "AI calculating strategic move...");
  } else if (model.mode == PlayMode::TwoPlayerPassAndPlay) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Pass to %s (tap FLIP to rotate table)",
                  model.game.turn == morris::kWhite ? "White" : "Black");
  } else {
    const morris::Phase phase = morris::playerPhase(model.game, model.game.turn);
    if (phase == morris::Phase::Placing) {
      std::snprintf(hintBuf, sizeof(hintBuf), "Placing phase: Tap any vacant point to place piece.");
    } else if (phase == morris::Phase::Flying) {
      if (model.game.selected >= 0) {
        std::snprintf(hintBuf, sizeof(hintBuf), "Flying: Tap any vacant point to jump.");
      } else {
        std::snprintf(hintBuf, sizeof(hintBuf), "Flying (3 left): Tap your piece to lift it.");
      }
    } else {
      if (model.game.selected >= 0) {
        std::snprintf(hintBuf, sizeof(hintBuf), "Moving: Tap an adjacent vacant point.");
      } else {
        std::snprintf(hintBuf, sizeof(hintBuf), "Moving phase: Tap your piece to select it.");
      }
    }
  }

  fui::TextStyle hintStyle;
  hintStyle.font = toybox::kTileFont;
  hintStyle.color = fui::Color::Black;
  hintStyle.align = fui::TextAlign::Center;
  const int16_t hintY = static_cast<int16_t>(board.bottom() + 10);
  const int16_t hintH = static_cast<int16_t>(actionRow.y - board.bottom() - 14);
  if (hintH > 0) {
    screen.target().text(fui::makeRect(board.x, hintY, board.width, hintH), hintBuf, hintStyle);
  }
}

}  // namespace morrisui
