# Daybook

A calendar and to-do lists in one app, on the Apps shelf. Touch only.

## What it does

- **Home.** A month grid, a week, or an upcoming list (the last choice is kept). Under the month sits the selected day.
  The bar at the bottom: Today, M / W, List view, then To-Do's, Find, Settings.
- **Events.** Name, place, note, all day or from-to times, several days, and repeats (daily, weekly, monthly, yearly,
  every N, until a day). A 31st that lands in a short month moves to the month's end.
- **To-dos.** Up to eight lists; each to-do may have a due day, which also shows on the calendar. Open ones past their
  day are listed first as overdue.
- **Find.** Words typed anywhere in an event's name, place or note, or in a to-do. Case and accents do not matter.
- **Undo.** After a delete, a bar at the bottom brings the event, to-do or list back until something else is saved.

Limits: 400 events, 500 to-dos, 8 lists; dates 1970 to 2200 (the form will not step past them).

## Where the data lives

`/.crosspoint/calendar/` on the card: `events.txt`, `tasks.txt`, `lists.txt`, `settings.txt`. Plain text, one record
per line, tab separated, first line `V1`. Every write goes to a `.tmp` file that is then renamed over the real one, so a
power cut leaves the old file, never half of a new one. Bad lines are skipped, never fatal.

All the rules (dates, repeats, search, file formats, the sleep-screen layout) are in `DaybookCore`, which has no
Arduino, renderer or card in it, so `host-tests/daybook/run.sh` runs them on a PC. The two Activity files are the
screens and the card.

## Agenda on the sleep screen

Daybook > Settings > *Agenda on the sleep screen* draws a small panel over the Custom sleep screen: today's events that
are not over yet, today's open to-dos, a count of overdue ones, and optionally tomorrow. Size is Small (counts only),
Normal or Large; the panel sits at the top or the bottom. It is drawn from the files at the moment the device sleeps,
and carries an "as of 21:40" stamp because the screen does not change while the device is asleep. After 21:00 tomorrow
gets the room, because that is what you look at before bed.

This needs the one hook in `SleepActivity.cpp` (`daybook::agendaEnabled()` and `daybook::drawAgenda()`).

## Web page

Settings > *Web page for phone or PC (plugin)* writes a CrossPoint web UI plugin to the card
(`/.crosspoint/plugins/daybook/`: `manifest.json`, `README.md`, `plugin.js`; an existing copy in `/plugins` or
`/.plugins` is updated in place). Turn on Settings > Connectivity > Plugins, open File Transfer, and Daybook is on the
Plugins page. It reads and writes the same four files, so the page and the device agree. The script is one string in
`DaybookPlugin.cpp`, about 32 KB of flash.

## Not verified

Not run on a device by whoever wrote this note: panel drawing, touch feel, and the sleep screen are checked on the host
with a recording renderer, not on the e-ink panel.
