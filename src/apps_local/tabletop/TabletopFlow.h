#pragma once

#include <cstdint>

namespace tabletop {

enum class Screen : uint8_t {
  Lobby = 0,
  Reversi,
  Mancala,
  Morris,
  Santorini,
  Quoridor,
  Onitama,
  Dobutsu,
  Dominoes,
  LiarsDice,
  Backgammon,
  Sandbox,
  About,
};

enum class LobbyRow : int {
  Reversi = 0,
  Mancala,
  Morris,
  Santorini,
  Quoridor,
  Onitama,
  Dobutsu,
  Dominoes,
  LiarsDice,
  Backgammon,
  Sandbox,
  About,
  Count,
};

constexpr Screen back(const Screen screen) {
  switch (screen) {
    case Screen::Lobby:
      return Screen::Lobby;
    case Screen::Reversi:
    case Screen::Mancala:
    case Screen::Morris:
    case Screen::Santorini:
    case Screen::Quoridor:
    case Screen::Onitama:
    case Screen::Dobutsu:
    case Screen::Dominoes:
    case Screen::LiarsDice:
    case Screen::Backgammon:
    case Screen::Sandbox:
    case Screen::About:
      return Screen::Lobby;
  }
  return Screen::Lobby;
}

constexpr bool leavesApp(const Screen screen) { return screen == Screen::Lobby; }

}  // namespace tabletop
