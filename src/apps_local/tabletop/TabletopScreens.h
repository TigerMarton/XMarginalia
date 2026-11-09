#pragma once

#include "../ui/ToyboxScreen.h"
#include "TabletopFlow.h"

namespace tabletopui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionLobbyRow = 1,
  ActionBackToLobby = 2,
  ActionGoToPage = 3,
};

constexpr int kRowsPerPage = 8;
constexpr int kPageCount = (static_cast<int>(tabletop::LobbyRow::Count) + kRowsPerPage - 1) / kRowsPerPage;

struct LobbyModel {
  int page = 0;
  int selected = 0;
};

void buildLobby(toybox::Screen& screen, const LobbyModel& model);
void buildAbout(toybox::Screen& screen);
void buildPlaceholder(toybox::Screen& screen, const char* title, const char* message);

}  // namespace tabletopui
