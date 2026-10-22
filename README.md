<p align="center">
  <img src="docs/icon.png" width="96" alt="Yami icon">
</p>

<h1 align="center">Yami</h1>

<p align="center">Monitor brightness and dimming from the Windows tray.</p>

<p align="center">
  <img src="docs/screenshot.png" alt="Yami panel">
</p>

Yami (闇 "darkness") is a small tray app that controls monitor brightness and can dim the screen further than the monitor itself allows. It runs as a single exe and doesn't need installing.

## Features

- Brightness control for external monitors (DDC/CI)
- Extra dimming for any display (including laptops)
- Each monitor has its own dim level
- `Ctrl+Alt+Shift+D` turns off dimming on all displays
- Smart auto adjust that follows the room light or the time of day, and calibrates itself from the levels you pick
- Can start with Windows and stay in the tray

## Installation

#### Requirements

- Windows 10 (1703 or later) or Windows 11
- Brightness control needs DDC/CI turned on in the monitor's on-screen menu. Displays without it (like most laptop screens) only get dimming.

#### Download

1. Download `Yami.exe` from the [latest release](../../releases/latest).
2. Run it. Yami adds its icon to the tray and opens the panel.
3. Turn on **Run at startup** in the tray menu if you want Yami to start with Windows.

#### Uninstall

1. Turn off **Run at startup** in the tray menu and exit Yami.
2. Delete `Yami.exe` and the `HKCU\Software\Yami` registry key.
3. If you unlocked [stronger dimming](#stronger-dimming), also delete the `GdiIcmGammaRange` value.

## Usage

#### Controls

- Left-click the tray icon to show or hide the panel and right-click it for the menu.
- Drag a slider or scroll over it to move by 1. Clicking the track jumps by 10.
- Press `Esc` or the close button to hide the panel.
- Click the **?** button in the panel to see all mouse and keyboard actions.
- Use Exit in the tray menu to quit. Quitting only turns off the extra dimming, but hardware brightness stays where you set it.

#### Auto adjust

Yami can change brightness and dim by itself. Click the sun button next to the close button in the panel (or open **Auto adjust** in the tray menu) and pick what it follows:

- **Follow light sensor** follows the room light. It needs an ambient light sensor that Windows lists under **Sensors** in Device Manager. Most laptops have one, but desktops need a USB light sensor that uses the standard HID sensor protocol.
- **Follow time of day** follows the clock, so it works on any PC.

The button is dimmed while auto adjust is off, and it shows a clock while following the time of day.

Auto adjust uses smart calibration, which means it learns from you instead of using fixed levels. Set the sliders to the levels you like and Yami saves them for the current light level (or the current time). After you do this in a few different conditions, Yami moves between your saved levels on its own.
A monitor keeps its current levels until you save at least one level for it.

While auto adjust is on, a bar under the monitors shows the current light level or time:

- Drag along the bar (or scroll over it) to preview another condition. The monitors switch to the levels saved for it, and moving a slider saves new levels for that condition. This lets you set the night levels during the day.
- Each mark on the bar is a saved condition. Right-click a mark to remove it.
- Click **Now** (or hide the panel) to go back to the current condition.
- **Reset calibration** in the same menu removes all saved levels for the current mode.

> [!NOTE]
> Turning dim off (with the hotkey or the tray menu) also turns auto adjust off, so the dimming doesn't come back by itself. Turn it on again with the sun button or the tray menu.

#### Stronger dimming

By default Windows doesn't let gamma dimming go past 50%. The **Unlock stronger dimming** option raises the limit to 90% by setting the registry value below.

> [!NOTE]
> Needs admin rights and applies to all users, and takes effect after a restart.

```
HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ICM\GdiIcmGammaRange = 256 (DWORD)
```

Delete the value to undo it.

## Build (Developers)

Requires Visual Studio 2022 or Build Tools with the C++ desktop workload.

```
build.bat
```

The exe is written to `build\Yami.exe`.

---

#### Credits: Created by Claude Opus 5.5
#### License: [MIT](LICENSE)


