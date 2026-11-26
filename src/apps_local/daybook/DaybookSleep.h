#pragma once

#include <string>
#include <vector>

class GfxRenderer;

namespace daybook {

// Draws today's agenda (events, overdue tasks, tasks due today) as a white
// panel with a black border on top of whatever is in the frame buffer.
// Reads the Daybook files from the SD card. Returns false, drawing nothing,
// when the clock is not set or there is nothing to show.
bool drawAgenda(GfxRenderer& renderer);

// Whether Daybook > Settings has "Agenda on sleep screen" on.
bool agendaEnabled();

// The wallpaper chosen in Daybook > Settings, or "" for the default
// (/sleep.bmp, then the sleep folders). Read from the settings file.
std::string wallpaperPath();

// Every .bmp the picker offers: the SD root, /sleepscreen, /sleep, /.sleep and
// /wallpapers. Full paths, sorted, at most 60.
std::vector<std::string> wallpaperChoices();

}  // namespace daybook
