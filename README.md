# windhawk-taskbar-suite

Original [Windhawk](https://windhawk.net) mods for a glass-style Windows 11 desktop:
taskbar, tray, notifications and media. Each mod is a single file in `mods/`
and is meant to work on its own.

All code in this repo is written from scratch and licensed MIT (see `LICENSE`).

## Mods

| Mod | File | Status |
|---|---|---|
| Taskbar Now Playing Widget | `mods/taskbar-now-playing-widget.wh.cpp` | v0.1.0, untested draft |

Planned: taskbar styling and notification styling.

### Taskbar Now Playing Widget
A small widget on the taskbar showing the current track (Spotify, browsers,
any app that uses the Windows media session) with previous, play/pause and next
buttons. It draws its own window and does not hook Explorer, so a bug in it
should not affect the taskbar.

Settings: width, height, offset X and Y, opacity, and hide when nothing plays.

## Install a mod locally
1. Open Windhawk and choose **Create a new mod**.
2. Paste the contents of the `.wh.cpp` file over the template.
3. Click **Compile Mod**, then enable the mod.

## Suggested companions
- Windhawk's **Windows 11 Taskbar Styler** and **Start Menu Styler** for the
  glass taskbar and Start menu itself. This repo does not replace them.
- The sibling repo [windhawk-system-mods](../windhawk-system-mods) for system
  dialogs such as the file copy window.

## Submitting to the Windhawk catalog
Open a pull request to `ramensoftware/windhawk-mods` that adds only
`mods/<mod-id>.wh.cpp`. The `@github` line in the file must match the
username of the person opening the pull request.

## Known limitations
- Primary taskbar only.
- Mods here are young. Report problems with your Windows version and a
  screenshot.
