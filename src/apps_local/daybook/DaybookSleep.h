#pragma once

#include <string>
#include <vector>

class GfxRenderer;

namespace daybook {

// Draws the agenda as a white panel with a black border on top of whatever is in the
// frame buffer: today's events that are not over, today's open to-dos, a count of the
// overdue ones and, if Settings asks for it, tomorrow in a smaller font. Reads the
// Daybook files from the SD card. Returns false, drawing nothing, when the clock is not set.
bool drawAgenda(GfxRenderer& renderer);

// Whether Daybook > Settings has "Agenda on sleep screen" on.
bool agendaEnabled();
// "Today + tomorrow" (the default) rather than "Today only".
bool agendaTomorrow();
// The panel sits at the bottom of the screen instead of the top.
bool agendaBottom();

// The wallpaper chosen in Daybook > Settings, or "" for the default
// (/sleep.bmp, then the sleep folders). Read from the settings file.
std::string wallpaperPath();

// Every .bmp the picker offers: the SD root, /sleepscreen, /sleep, /.sleep and
// /wallpapers. Full paths, sorted, at most 60.
std::vector<std::string> wallpaperChoices();

}  // namespace daybook
