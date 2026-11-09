#!/bin/bash
# Builds and runs the Tabletop host tests.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-tabletop-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/tabletop
SDK=../../freeink-sdk/libs/ui/FreeInkUI
ICONS=../../freeink-sdk/libs/assets/Icons

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC test_tabletop.cpp -o "$BUILD_DIR/test_tabletop"
"$BUILD_DIR/test_tabletop"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/games/reversi/ReversiCore.cpp" \
  "$SRC/games/reversi/ReversiBrain.cpp" \
  "$SRC/games/reversi/ReversiScreens.cpp" \
  test_reversi.cpp -o "$BUILD_DIR/test_reversi"
"$BUILD_DIR/test_reversi"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/games/mancala/MancalaCore.cpp" \
  "$SRC/games/mancala/MancalaBrain.cpp" \
  "$SRC/games/mancala/MancalaScreens.cpp" \
  test_mancala.cpp -o "$BUILD_DIR/test_mancala"
"$BUILD_DIR/test_mancala"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/games/morris/MorrisCore.cpp" \
  "$SRC/games/morris/MorrisBrain.cpp" \
  "$SRC/games/morris/MorrisScreens.cpp" \
  test_morris.cpp -o "$BUILD_DIR/test_morris"
"$BUILD_DIR/test_morris"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" -I"../../src/apps_local/cards" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/sandbox/SandboxCore.cpp" \
  "$SRC/sandbox/SandboxScreens.cpp" \
  "../../src/apps_local/cards/CardArt.cpp" \
  test_sandbox.cpp -o "$BUILD_DIR/test_sandbox"
"$BUILD_DIR/test_sandbox"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 \
  -I"$SRC" \
  "$SRC/link/TabletopLink.cpp" \
  test_link.cpp -o "$BUILD_DIR/test_link"
"$BUILD_DIR/test_link"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 \
  -I"$SRC" \
  "$SRC/TabletopSave.cpp" \
  "$SRC/sandbox/SandboxCore.cpp" \
  "$SRC/games/onitama/OnitamaCore.cpp" \
  "$SRC/games/liarsdice/LiarsDiceCore.cpp" \
  "$SRC/games/dobutsu/DobutsuCore.cpp" \
  "$SRC/games/backgammon/BackgammonCore.cpp" \
  "$SRC/games/santorini/SantoriniCore.cpp" \
  "$SRC/games/quoridor/QuoridorCore.cpp" \
  "$SRC/games/dominoes/DominoesCore.cpp" \
  test_save.cpp -o "$BUILD_DIR/test_save"
"$BUILD_DIR/test_save"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/games/onitama/OnitamaCore.cpp" \
  "$SRC/games/onitama/OnitamaBrain.cpp" \
  "$SRC/games/onitama/OnitamaScreens.cpp" \
  test_onitama.cpp -o "$BUILD_DIR/test_onitama"
"$BUILD_DIR/test_onitama"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/games/liarsdice/LiarsDiceCore.cpp" \
  "$SRC/games/liarsdice/LiarsDiceBrain.cpp" \
  "$SRC/games/liarsdice/LiarsDiceScreens.cpp" \
  test_liarsdice.cpp -o "$BUILD_DIR/test_liarsdice"
"$BUILD_DIR/test_liarsdice"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/games/dobutsu/DobutsuCore.cpp" \
  "$SRC/games/dobutsu/DobutsuBrain.cpp" \
  "$SRC/games/dobutsu/DobutsuScreens.cpp" \
  test_dobutsu.cpp -o "$BUILD_DIR/test_dobutsu"
"$BUILD_DIR/test_dobutsu"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/games/backgammon/BackgammonCore.cpp" \
  "$SRC/games/backgammon/BackgammonBrain.cpp" \
  "$SRC/games/backgammon/BackgammonScreens.cpp" \
  test_backgammon.cpp -o "$BUILD_DIR/test_backgammon"
"$BUILD_DIR/test_backgammon"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/games/santorini/SantoriniCore.cpp" \
  "$SRC/games/santorini/SantoriniBrain.cpp" \
  "$SRC/games/santorini/SantoriniScreens.cpp" \
  test_santorini.cpp -o "$BUILD_DIR/test_santorini"
"$BUILD_DIR/test_santorini"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/games/quoridor/QuoridorCore.cpp" \
  "$SRC/games/quoridor/QuoridorBrain.cpp" \
  "$SRC/games/quoridor/QuoridorScreens.cpp" \
  test_quoridor.cpp -o "$BUILD_DIR/test_quoridor"
"$BUILD_DIR/test_quoridor"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -O2 \
  -I"$SRC" -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  "$SRC/games/dominoes/DominoesCore.cpp" \
  "$SRC/games/dominoes/DominoesBrain.cpp" \
  "$SRC/games/dominoes/DominoesScreens.cpp" \
  test_dominoes.cpp -o "$BUILD_DIR/test_dominoes"
"$BUILD_DIR/test_dominoes"





