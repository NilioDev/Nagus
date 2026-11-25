# Nagus

**Play the 1999 Windows space strategy game *Star Trek: The Next Generation –
Birth of the Federation* on today's computers, phones and tablets.**

Nagus is a free, open-source, from-scratch engine for the original game.
Bring your own copy: Nagus reads its files and aims to play the game as the
original does, with the same rules and screens, scaled sharply to any screen
and with gamepad, mouse, keyboard and touch controls.

> [!NOTE]
> **Status: playable from start to end.** Nagus imports your game (the CD image, the zip
> it came in, a copy of the CD or the installed game folder) and the
> official 1.0.2 patch, and shows the original's main menu with its own
> pictures, fonts and pointer. *New Single Player Game* opens the game
> set-up screen, where galaxy shape and size, minor races, difficulty,
> the starting level of each empire, random events, tactical combat and
> victory conditions can be set (Nagus remembers them), and then the
> empire choice, with each empire's portrait and description. Nagus then
> makes a galaxy in the chosen shape and size, with the empires' home
> systems and the minor races, and shows it on your empire's main screen:
> move over the map, pick a sector to see its star and turning planets,
> and switch between the close view and the whole galaxy. Your home system
> opens on the system screen: see its planets, put people to work on farms,
> factories, power plants, research and intelligence, switch structures on
> and off, choose what to build and buy it with credits. *Turn* ends the
> turn: colonies grow, make food, industry, research and credits, and finish
> what they build. *Economic*, *Military* and *Events* below the map switch
> the map's mode, each with its own layers chosen under *Overlay* (system
> names, ship ranges, paths, defences, territory, trade routes, build
> orders, population); *Events* lists last turn's events below the map. In
> *Economic* mode, one of your systems shows its trade routes on the left:
> click *Trade Routes* and then a system of a race you have a friendship
> (or better) with to trade, and earn credits each turn. *Summary*
> on the map shows last turn's events, your
> treaties and a list of your systems (click one to open it). On the research screen you share your research between
> the six fields and see how far each has come; its *Field Database* and
> *Object Database* describe every technology, building and ship class. The marker window (right
> click, Esc or B) takes you between the map, your systems, research and the
> main menu. Build ships at systems with a shipyard; choose a fleet in the
> list on the left and click a sector on the map to send it there, and use
> *Orders* → *Colonize* with a colony ship to found new colonies;
> *Redeploy* splits and joins fleets, and *Orders* also has avoid or engage,
> raid, scrap, intercept, entering a wormhole and training crews at an
> academy (crews grow from green to legendary, also in battle). Romulan and
> some Klingon ships can *Cloak* to hide from the other empires. Colony
> ships *Terraform* planets: only terran and terraformed planets hold
> people, so a new colony needs one first. Troop
> transports build outposts and starbases (*Orders*), which extend your
> ships' range, fight off enemies and, as starbases, repair your ships. *Save Game*
> and *Load Game* in the main menu keep up to twelve games. The other four
> empires are played by the computer: they build, settle new systems and
> send out scouts and warships. The map shows only what your empire has
> explored, with each empire's space in its colour. Empires at war fight
> when their fleets meet. With tactical combat set to *Manual* in the game
> set-up, your battles open the combat screen, which shows the battle in
> 3D with the game's own ship models (or flat, from above): let the battle
> be fought automatically, or give your ships a maneuver each round
> (assault, charge, circle, harry, flyby, strafe, evade, ram, retreat or
> hail);
> *Orders* → *Attack
> System* bombards an enemy colony, and troop transports invade it. On the
> diplomacy screen (marker window) you meet the other races: propose
> treaties from non-aggression to alliance, give gifts, declare war, answer
> the other empires' proposals and win minor races over until they join
> your empire. On the intelligence screen you share your intelligence
> between security at home and espionage and sabotage against each other
> empire, choose what your agents aim at (general, science, economy or
> military), read what they did and what was done to you, and compare the
> empires. The original's music plays, the main theme in the menus and
> your empire's theme in the game, and buttons click. The opening film plays
> when Nagus starts, and your empire's film when a new game begins (any
> click, Enter, Esc or gamepad button skips it). A game ends in victory by
> conquest, by domination or (in team play) with allies, or in defeat; you
> can also retire from the main menu. The original's end pictures and
> speech then play, and the game goes into the hall of fame (main menu).
> *Options* in the main menu sets the volume of the music, sound effects,
> speech and all together, and switches tooltips, tactical combat, random events and
> saving each turn (on by default: *Continue* in the main menu then picks up
> where you left off, even after closing Nagus). With random events on, plague, earthquakes, comets, terrorists,
> immigrants and a ship virus strike now and then. *New Multi-Player Game*
> plays with friends over a network (TCP/IP): one player hosts, the others
> join with the host's address, everyone chooses an empire, and each turn
> ends when every player has given their orders (see
> [multiplayer](docs/rules/multiplayer.md); over the internet the host
> forwards port 47621), and the players can chat during the game. Rest the pointer on a button to read what it does,
> as in the original. Buttons that do not work yet are shown dimmed.

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
Saved games are kept in a `saves` folder beside `library`.

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
| Skip a film | Click or tap | Enter, Space or Esc | A, B or Start |
| Next / previous empire (empire choice), next / previous home system (map) | | Tab or Page Down / Page Up | RB / LB |
| Move over the galaxy map (pointer on the map) | Click a sector | Arrow keys | D-pad or right stick |
| Close view or whole galaxy | Click *Map* | | A on *Map* |
| Open one of your systems | Click it again, or click the panel below the map | | A on the panel below the map |
| Next / previous tab (system screen) | | Tab or Page Down / Page Up | RB / LB |
| Choose from the build list, then build it | Click a line, then click it again | Arrow keys on the list, Enter | D-pad on the list, A |
| Take an item out of the build queue | Click it | | A on it |
| Empire summary: events, treaties, your systems | Click *Summary* on the map | Tab or Page Down / Page Up between the lists | A on *Summary*, RB / LB |
| Marker window: go to the map, your systems, diplomacy, intelligence, research or the main menu | Right click | Esc | B |
| Choose a fleet in the chosen sector | Click it in the list on the left (its arrows scroll through many) | | A on it |
| Send the chosen fleet | Click a sector on the map (its own sector stops it) | | D-pad to the sector, A |
| Give an order (colonize, attack system) | Click *Orders* below the map | | A on *Orders* |
| Choose a race on the diplomacy screen | Click it in the list at the bottom | Tab or Page Down / Page Up | RB / LB |
| Next / previous page (intelligence screen) | Click *Management*, *Report* or *Empire Status* | Tab or Page Down / Page Up | RB / LB |
| Change a field's share of research | Click the bar or its arrows | | A on the arrows |
| Switch the combat view between 3D and flat | Click *Playback* or *Tactical* | Tab or Page Down / Page Up | RB / LB |
| Back to the main menu while playing (*Continue* returns) | Right click, then the emblem | Esc, then the emblem | B, then A on the emblem |
| Save or load a game | *Save Game* or *Load Game* in the main menu, click a place, then *Accept* (or click the place again) | Arrow keys, Enter | D-pad, A, or Start to accept |
| Change a volume or switch an option (*Options*) | Click its bar or button | Arrow keys | D-pad |
| Chat (multiplayer game) | Click a chat line at the top of the screen | F2, then type; Enter sends, Esc closes | Y |
| Type a name or address (multiplayer) | Click the field and type | Type; Backspace erases, Enter ends | A on the field opens the keyboard where there is one |
| Full screen on or off | | F11 or Alt+Enter | |

Settings on the set-up screen change with each click on their button. The
original's pointer is drawn as part of the picture; it hides while you use
touch.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
