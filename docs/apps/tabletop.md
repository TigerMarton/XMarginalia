# Tabletop

The rules, state models, and sandbox tools for the Tabletop suite (`src/apps_local/tabletop/`).

---

## 1. Overview

Tabletop is a suite of classic tactile board games and virtual tabletop utility tools engineered for constrained 1-bit e-ink hardware. It provides:
1. **Reversi / Othello**: 8×8 flanking discs (Bitboard engine, Minimax AI, 2P Pass-and-Play).
2. **Mancala / Kalah**: Ancient 14-pit sowing strategy (Minimax AI, extra turns, 2P Pass-and-Play).
3. **Nine Men's Morris**: Ancient 24-vertex mill game (Placing, Moving, Flying, Minimax AI, 2P Pass-and-Play).
4. **Onitama**: 5×5 martial arts tactics with dynamic card exchange (Master & Students, 16-card deck, Minimax AI, 2P Pass-and-Play).
5. **Liar's Dice / Perudo**: Bluffing & bidding dice game (Wild 1s, Challenge / Dudo, Exacto / Calza calls, Palafico round, 4-player pass-and-play or solo vs 3 bot personalities).
6. **Dobutsu Shogi / Catch the Lion**: 3×4 simplified animal Shogi (Lion, Giraffe, Elephant, Chick/Hen, piece drops, Try Rule, Minimax AI, 2P Pass-and-Play).
7. **Backgammon**: 5,000-year-old 24-point race and blot strategy (15 checkers/side, dice rolling with doubles, blot hitting & bar re-entry, bearing off, pip counts, single/gammon/backgammon multipliers, Minimax AI, 2P Pass-and-Play).
8. **Santorini**: 5×5 3D island cliff-building (2 Workers/side, Ground to Level 3, Domes, Minimax AI, 2P Pass-and-Play).
9. **Quoridor**: 9×9 maze-racing corridor tactics (10 walls/player, BFS fair-play validation, Minimax AI, 2P Pass-and-Play).
10. **Dominoes**: Double-six block dominoes (28 tiles, open ends, boneyard drawing, secret hand privacy, Minimax AI, 2P Pass-and-Play).
11. **Tabletop Sandbox**: Virtual board game toolkit (Dice cup D4–D20, 52-card standard deck with secret hand privacy, 4-player life counter, and freeform 8×8 token board).

---

## 2. Reversi / Othello

### Rules & State Representation
* **Board**: 64-bit bitboards (`uint64_t black`, `uint64_t white`).
* **Turns**: Black plays first. A move places a disc on a vacant cell that flanks one or more opposing discs along one of the 8 cardinal/diagonal rays between the placed disc and another friendly disc. All flanked opposing discs are flipped.
* **Passes**: If a player has no legal moves, their turn is automatically passed. If neither player can move, or the board is full, the game ends. The player with the most discs wins.
* **Solo AI**: Minimax search with Alpha-Beta pruning (`ReversiBrain.h`). Evaluates positional differential (corner control, edge stability, inner buffer squares) and mobility differential.

---

## 3. Mancala / Kalah

### Rules & State Representation
* **Board**: 14-element array: 6 South pits (`0..5`), South Store (`6`), 6 North pits (`7..12`), North Store (`13`).
* **Sowing**: Players choose a pit on their side with seeds > 0. Seeds are distributed one-by-one counter-clockwise, placing into the friendly store but skipping the opponent's store.
* **Extra Turn**: If the last sown seed lands in the player's own store, they immediately receive an extra turn.
* **Capture**: If the last sown seed lands in an empty pit on the player's side and the opposite pit contains seeds, all seeds in the opposite pit plus the landing seed are captured into the player's store.
* **Game Over**: When one side has no legal moves remaining, the other player sweeps all remaining seeds on their side into their store. The player with the most seeds in their store wins.
* **Solo AI**: Minimax search with Alpha-Beta pruning evaluating store margin, seed distribution, and chained extra turns.

---

## 4. Nine Men's Morris

### Rules & State Representation
* **Board**: 24 vertices across 3 concentric squares connected by 4 orthogonal mid-lines.
* **Three Game Phases**:
  1. *Placing*: Players take turns placing 9 pieces each onto any vacant vertex.
  2. *Moving*: Once all 18 pieces are placed, players slide one piece per turn along connecting lines to an adjacent vacant vertex.
  3. *Flying*: When a player is reduced to exactly 3 pieces, their pieces can fly (jump) to any vacant vertex on the board.
* **Mills**: Aligning 3 friendly pieces along any of the 16 predefined mill lines forms a Mill. This allows the player to remove one enemy piece from the board. Pieces currently part of an active mill cannot be removed unless all enemy pieces are in mills.
* **Victory**: A player wins when the opponent is reduced to fewer than 3 pieces or has no legal moves.
* **Solo AI**: Minimax search with Alpha-Beta pruning prioritizing mill formation, mill blocking, and mobility preservation.

---

## 5. Onitama

### Rules & State Representation
* **Board**: 5×5 grid. White starts at row 4, Black starts at row 0. Each player controls 1 Master (center column `x=2`) and 4 Students (`x=0, 1, 3, 4`).
* **Movement Cards**: 16 martial arts cards (Tiger, Dragon, Frog, Rabbit, Crab, Elephant, Goose, Rooster, Monkey, Mantis, Horse, Ox, Crane, Boar, Eel, Cobra), each featuring unique relative jump offset vectors.
* **Card Rotation**: 5 cards are dealt per game: 2 cards to White, 2 cards to Black, and 1 floating neutral card. Executing a move with a card swaps it with the neutral card, which will be received by the opponent after their next move.
* **Two Paths to Victory**:
  1. *Way of the Stone*: Capture the enemy Master piece.
  2. *Way of the Stream*: Move your own Master into the opponent's starting Temple Arch (`x=2, y=0` for White; `x=2, y=4` for Black).
* **Solo AI**: Minimax search with Alpha-Beta pruning (`OnitamaBrain.h`) evaluating Master preservation, opponent Master threats, distance to opponent Temple, student differential, and card mobility.

---

## 6. Liar's Dice / Perudo

### Rules & State Representation
* **Players**: 2 to 4 players (Solo vs 1–3 AI bots, or 2–4 Player Pass-and-Play).
* **Dice**: Each player begins with 5 standard D6 dice rolled inside an opaque cup.
* **Bidding**: The opening player announces a quantity and face value (e.g. "three 4s"). Subsequent bids must either increase the quantity (e.g. "four 4s", "four 2s") or increase the face value at the same quantity (e.g. "three 5s").
* **Wild 1s (Aces)**: Face 1s are wild and count toward any bid face. Transitioning from normal dice to 1s requires bidding at least half the current quantity rounded up (`(qty + 1) / 2`). Transitioning back from 1s to another face requires `2 * qty + 1`.
* **Challenges**:
  * *Liar! (Dudo)*: Player claims the previous bid is an overstatement. All cups are revealed. If the total matching dice are strictly less than bid quantity, the bidder loses 1 die. Otherwise, the challenger loses 1 die.
  * *Exacto! (Calza)*: Player claims the previous bid is exactly right. If true, the challenger regains a previously lost die (up to 5). If wrong, the challenger loses 1 die.
* **Palafico Round**: Triggered when any player is reduced to their final die (1 die remaining). In this round, 1s are NOT wild, and the bid face cannot be changed by subsequent bidders unless they also have 1 die.
* **Solo AI**: 3 bot personality profiles (Conservative, Aggressive, Balanced) utilizing exact binomial probability distributions combined with bluffing tendencies.

---

## 7. Dobutsu Shogi / Catch the Lion

### Rules & State Representation
* **Board**: 3×4 miniature Shogi grid.
* **Pieces & Movement**:
  * *Lion*: Moves 1 step in all 8 directions.
  * *Giraffe*: Moves 1 step orthogonally (4 directions).
  * *Elephant*: Moves 1 step diagonally (4 directions).
  * *Chick*: Moves 1 step straight forward. Promotes to *Hen* upon reaching the opponent's back rank.
  * *Hen*: Moves 1 step orthogonally or 1 step diagonally forward (6 directions, identical to Shogi Gold General).
* **Piece Drops (Mochi-goma)**: Captured pieces enter the player's reserve bench. On their turn, a player may either move an on-board piece or drop a captured piece onto any vacant cell (Chick always drops unpromoted).
* **Two Paths to Victory**:
  1. *Catch the Lion*: Capture the opponent's Lion.
  2. *Try Rule*: Move your Lion safely onto the opponent's back rank without being captured on the opponent's immediately following turn.
* **Solo AI**: Minimax search with Alpha-Beta pruning evaluating piece values, drop bench flexibility, Lion centralization, and Try-rule advancement.

---

## 8. Backgammon

### Rules & State Representation
* **Board**: 24 triangular points divided into four quadrants (White Home 1..6, White Outer 7..12, Black Outer 13..18, Black Home 19..24), with a central Bar and Bear-off trays.
* **Checkers**: 15 White checkers (moving 24 -> 1) and 15 Black checkers (moving 1 -> 24).
* **Turns & Rolling**: Players roll 2 dice. Rolling doubles grants 4 moves of that face value.
* **Blots & Bar**: A single checker on a point is a vulnerable *blot*. Moving an opposing checker onto a blot hits it, sending it to the central Bar. Checkers on the bar must re-enter into the opponent's home quadrant before any other move can be made.
* **Bearing Off**: Once all 15 checkers are brought into a player's home board, they may be borne off using matching dice rolls or higher rolls if no checkers remain on higher points.
* **Victory Multipliers**:
  * *Single (1×)*: Winner bears off all 15 checkers; loser has borne off at least 1.
  * *Gammon (2×)*: Loser has borne off 0 checkers.
  * *Backgammon (3×)*: Loser has borne off 0 checkers AND still has at least 1 checker on the Bar or in the winner's home board.
* **Pip Count**: Live display of the aggregate distance (pips) remaining for all checkers to bear off.
* **Solo AI**: Minimax heuristic evaluating pip count race advantage, blot exposure vulnerability penalties, home-board anchor construction, and blot-hitting opportunities.

---

## 9. Santorini

### Rules & State Representation
* **Board**: 5×5 island cliff grid (`25` spaces).
* **Workers**: Each player controls 2 workers (`W1, W2` for White; `B1, B2` for Black).
* **Two-Step Turn**: On their turn, a player chooses one of their workers and performs:
  1. *Move*: Move to an adjacent space (orthogonal or diagonal, 8 directions). A worker may move up at most 1 level, down any number of levels, or across the same level. Spaces occupied by another worker or a completed Dome are impassable.
  2. *Build*: Build one building level onto an adjacent unoccupied space (Ground `0` -> Level 1 `1` -> Level 2 `2` -> Level 3 `3` -> Dome `4`).
* **Two Paths to Victory**:
  1. *Climb to Victory*: Move one of your workers up onto Level 3.
  2. *Trap Victory*: The opponent has no legal moves (all their workers are blocked from moving or building).
* **Solo AI**: Minimax search with Alpha-Beta pruning evaluating Level 3 climb opportunities, winning threat detection, worker mobility, Dome capping of opponent threat towers, and worker proximity.

---

## 10. Quoridor

### Rules & State Representation
* **Board**: 9×9 grid of squares (`81` spaces) separated by grooves (`8×8 = 64` groove intersections).
* **Pawns & Walls**: White pawn starts at `(4, 8)` with goal row `y = 0`; Black pawn starts at `(4, 0)` with goal row `y = 8`. Each player begins with 10 walls.
* **Turns**: On their turn, a player may either:
  1. *Move Pawn*: Move 1 step orthogonally to an adjacent empty square not blocked by a wall. If adjacent to an opponent pawn, jump over them (straight jump, or diagonal jump if blocked behind).
  2. *Place Wall*: Place a 2-space wall (Horizontal or Vertical) into the grooves. Walls cannot overlap or intersect other walls.
* **Fair Play Rule**: A wall placement is strictly illegal if it completely cuts off either player from having at least one open walkable path to their respective goal rank (validated via BFS reachability search).
* **Victory**: First player whose pawn reaches any square on their target goal rank wins.
* **Solo AI**: Minimax search with Alpha-Beta pruning using bidirectional Breadth-First Search (BFS) path-length differential, wall obstruction heuristics, and jump acceleration.

---

## 11. Dominoes

### Rules & State Representation
* **Set**: Standard double-six domino set (`28` unique tiles from `[0|0]` to `[6|6]`).
* **Deal & Hands**: 7 tiles dealt to Player 1, 7 tiles to Player 2 (or AI), remaining 14 tiles form the draw Boneyard.
* **Train & Matching**: The opening tile starts the table train. Players take turns playing a tile matching either the left or right open end pip values. Doubles are played as cross-tiles.
* **Drawing & Passing**: If a player cannot match either open end, they must draw tiles from the Boneyard until a playable tile is drawn or the Boneyard is exhausted, at which point they pass.
* **Victory & Scoring**:
  * *Domino*: First player to empty their hand wins the round, scoring the total sum of all pips remaining in the opponent's hand.
  * *Blocked Game*: If neither player can play and the Boneyard is empty, the player with the lowest aggregate pip count wins the round and scores the difference.
* **Secret Hand Privacy**: Includes `[ CONCEAL ]` / `[ PEEK ]` toggling to mask player hands with authentic e-ink card back crosshatch during 2-player pass-and-play.
* **Solo AI**: Lookahead search evaluating pip reduction, end-forcing, opponent blocking heuristics, and high-value double offloading.

---

## 12. Tabletop Sandbox

The Sandbox provides freeform tabletop utility tools across four tabs:

### 1. Dice Cup (`[ DICE ]`)
* 1 to 6 dice.
* Die types: D4, D6, D8, D10, D12, and D20.
* Tap individual dice to lock/hold them across rolls.
* Tap `[ ROLL ]` to roll all unheld dice.
* Displays live total sum and roll history.

### 2. Playing Cards (`[ CARDS ]`)
* Standard 52-card deck using shared `cardart::` graphics.
* Draw pile, discard / in-play slot, and 5-card player hand.
* **Secret Hand Privacy**: Tap `[ HIDE ]` / `[ PEEK ]` to conceal cards with authentic e-ink card back patterning during pass-and-play.

### 3. Life & Point Counters (`[ LIFE ]`)
* 4-player score track.
* `-1` and `+1` quick steppers with reset to 20 (life) or 0 (points).
* Active player highlight.

### 4. Freeform Board (`[ BOARD ]`)
* 400×400px 8×8 checkered board.
* Up to 32 movable tokens (White and Black discs).
* Tap to select, tap to move / capture.
* `[ CHECKERS ]` preset button for instant 12v12 draft setup.

---

## 13. Ergonomics, Persistence & Network Link

* **Pass-and-Play 180° Table Rotation**: In 2-Player mode, Reversi, Mancala, Nine Men's Morris, Onitama, Dobutsu Shogi, Backgammon, Santorini, and Quoridor include a `[ FLIP ]` button that rotates the board 180° across the table, allowing face-to-face play without awkward upside-down reading.
* **E-Ink Paged Lobby Navigation**: To eliminate awkward swipe-scrolling on slow e-ink refresh hardware, the 12-item Lobby is paginated into two 6-item pages (`1/2` and `2/2`). Navigation is supported via touch page buttons (`[ 1 ] [ 2 ]` on the bottom bar), vertical gestures (swipe Up/Down), and hardware buttons (`PageForward`/`PageBack` or `Up`/`Down` cursor steps).
* **Refresh Economy**: Partial e-ink refreshes (`HalDisplay::FAST_REFRESH`) are used for piece placement, moves, and rolls. Full refresh flashes (`HalDisplay::FULL_REFRESH`) occur only upon new game restart or screen navigation to prevent ghosting.
* **SD Card Persistence**: State is serialized atomically to `/.crosspoint/tabletop.cfg` via `HalStorage` upon activity exit or screen changes (`TabletopSave.h`), preserving game state across all 11 modules.
* **Two-Device Wireless Link (`PLAY NEARBY`)**: All 10 competitive tabletop games support two-device wireless multiplayer via `linkplay::PlayBase` and `linkplay::LinkActivity` over ESP-NOW. Tapping the mode button cycles: `MODE: SOLO` $\to$ `MODE: 2P` $\to$ `MODE: NEARBY`. Each game has a distinct wire GameId (`0x0C01` through `0x0C0A`). For games with initial deals or randomness (Onitama cards, Liar's Dice cups, Dominoes boneyard hands), both devices derive a deterministic, symmetric initial seed from the sorted peer MAC addresses (`play.matchSeed()`), ensuring identical opening states without extra handshake round-trips. Each game state is $\le 192$ bytes. Rematches and restarts are symmetrically proposed and confirmed via the link handshake overlay.
