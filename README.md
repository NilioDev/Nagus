# Nagus

**Play the 1999 Windows space strategy game *Star Trek: The Next Generation –
Birth of the Federation* on today's computers, phones and tablets.**

Nagus is a free, open-source, from-scratch engine for the original game.
Bring your own copy: Nagus reads its files and aims to play the game as the
original does, with the same rules and screens, scaled sharply to any screen
and with gamepad, mouse, keyboard and touch controls.

> [!NOTE]
> **Status: just started.** Nagus imports your game (the CD image, the zip
> it came in, a copy of the CD or the installed game folder) and the
> official 1.0.2 patch, and shows the original's main menu with its own
> pictures, fonts and pointer. *New Single Player Game* opens the game
> set-up screen, where galaxy shape and size, minor races, difficulty,
> the starting level of each empire, random events, tactical combat and
> victory conditions can be set (Nagus remembers them), and then the
> empire choice, with each empire's portrait and description. Making the
> galaxy and playing the game itself come next; for now Nagus says so and
> goes back to the main menu. Buttons that do not work yet are shown dimmed.

Nagus is not affiliated with or endorsed by the makers of the original
game. It contains none of the original game's files: you need your own copy.

## Planned

- 🎯 The original game: same rules, same look, same computer players.
- 🖥️ Windows, macOS, Linux and Android from one codebase.
- 🎮 Full gamepad support, as well as mouse, keyboard and touch.
- 🎵 The original's music, sounds and films.

## Try it

Download Nagus from the
[Releases page](https://github.com/NilioDev/Nagus/releases/latest); no
GitHub account is needed.

- 🐧 **Linux:** the `.AppImage`. Make it executable (right-click,
  *Properties*, *Allow executing file as program*, or `chmod +x`) and
  double-click it.
- 🪟 **Windows:** the `windows-x64` zip. Unpack it and start `Nagus.exe`.
- 🍎 **macOS:** the `macos` zip. Unpack it and open `Nagus.app`. It is not
  signed, so the first time right-click it and choose *Open*.
- 🤖 **Android:** the `.apk`. Open it on your phone, tablet or TV box and
  allow installing apps from your browser or file manager when asked. A
  newer APK may ask you to uninstall the older one first; your imported
  game and settings are then imported again.

Want the very newest build? Every build on `main` also makes test packages:
open the [latest CI run on main](https://github.com/NilioDev/Nagus/actions/workflows/ci.yml?query=branch%3Amain)
and download one at the bottom of the page (this needs a GitHub account).

## Add your game

You hand Nagus your own copy of the game **once**. On first start it asks
for it:

- drop the file or folder onto the window, or
- click, tap, or press **Enter** or the gamepad's **A** button to choose the
  file (on a phone: pick it in the file picker), or **F** to choose a folder.

Nagus takes any of these:

- the CD image (`.bin` with its `.cue`, or `.iso`), or the `.zip` it came in;
- a copy of the CD's files, or the installed game folder.

Nagus copies the files it needs into its own library and uses them from then
on, so you can delete or move the original afterwards. Then it asks for the
official **1.0.2 patch** (`botf102e.zip`); give it the same way, or press
**S** or **B** to play without it. Nagus never changes your files.

The library is in `~/.local/share/NilioDev/Nagus/library` on Linux,
`%APPDATA%\NilioDev\Nagus\library` on Windows, in *Application Support* on
macOS and in the app's own storage on Android.

If there is a `gamedata` folder with the game and patch next to the program,
next to the `.AppImage` file or in the current folder, Nagus imports it by
itself. You can also give files when starting it:
`./Nagus game.zip botf102e.zip`.

## Controls

| Action | Mouse / touch | Keyboard | Gamepad |
| --- | --- | --- | --- |
| Move the pointer | Move the mouse, or tap where you want | | Left stick |
| Jump to the next button in a direction | | Arrow keys | D-pad or right stick |
| Press the button under the pointer | Click or tap | Enter or Space | A |
| Back, Cancel | Right click | Esc or Backspace | B or Back |
| Accept, start a new game | | | Start |
| Next / previous empire (empire choice) | | Tab or Page Down / Page Up | RB / LB |
| Full screen on or off | | F11 or Alt+Enter | |

Settings on the set-up screen change with each click on their button. The
original's pointer is drawn as part of the picture; it hides while you use
touch.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
