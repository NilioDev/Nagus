// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/settings.hpp"
#include "nagus/ui/shell.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace nagus;
using namespace nagus::test;
namespace fs = std::filesystem;

namespace {

Point centre(const Rect& r) { return {r.centerX(), r.centerY()}; }

void click(Shell& shell, Point p) {
    shell.pointerMoved(p);
    shell.pointerPressed(p);
    shell.pointerReleased(p);
}

Rect element(Assets& assets, const char* layout, std::uint32_t id) {
    const auto* e = assets.layout(layout).find(id);
    return e ? e->rect : Rect{};
}

} // namespace

NGS_TEST("settings are remembered and kept in range") {
    const fs::path file = fs::temp_directory_path() / "nagus-test-settings.txt";
    GameSettings s;
    s.galaxyShape = 3;
    s.civilization[4] = 2;
    s.randomEvents = false;
    s.empire = 4;
    saveSettings(file, s);
    const GameSettings back = loadSettings(file);
    NGS_CHECK(back.galaxyShape == 3 && back.civilization[4] == 2 && !back.randomEvents && back.empire == 4);
    GameSettings wild;
    wild.difficulty = 99;
    wild.empire = -3;
    wild.clamp();
    NGS_CHECK(wild.difficulty == 4 && wild.empire == 0);
    fs::remove(file);
    NGS_CHECK(loadSettings(file).difficulty == GameSettings{}.difficulty);
}

NGS_TEST("main menu to set-up to empire choice and back") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path settings = fs::temp_directory_path() / "nagus-test-menu-settings.txt";
    fs::remove(settings);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Shell shell(res, settings);
    Canvas canvas;
    shell.draw(canvas, 0);
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);

    // A disabled button does nothing.
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kOptions)));
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);

    click(shell, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
    NGS_CHECK(shell.showing() == Shell::Showing::GameSetup);
    shell.draw(canvas, 0);

    // Galaxy shape changes with each click and is remembered.
    click(shell, centre(element(assets, "gmesetup.wdf", 0x2412)));
    click(shell, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    NGS_CHECK(shell.showing() == Shell::Showing::EmpireSelect);
    NGS_CHECK(loadSettings(settings).galaxyShape == 1);
    shell.draw(canvas, 0);

    // Pick the Klingons with the pointer, then go on with the gamepad.
    shell.setSeed(42);
    click(shell, centre(element(assets, "emppick.wdf", 0x2394)));
    shell.command(Command::Start);
    NGS_CHECK(loadSettings(settings).empire == 3);
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
    NGS_CHECK(shell.game() != nullptr && shell.game()->player == 3);
    shell.draw(canvas, 0);

    // The galaxy screen starts on the player's home system.
    const Game& game = *shell.game();
    GalaxyScreen& screen = *shell.galaxyScreen();
    const auto& home = game.galaxy.systems[static_cast<std::size_t>(game.galaxy.homes[3])];
    NGS_CHECK(screen.chosen() == home.sector);
    NGS_CHECK(home.name == "Qo'nos");
    // With the pointer on the map, the d-pad moves the chosen sector.
    const auto at = screen.sectorCentre(home.sector);
    NGS_CHECK(at.has_value());
    shell.pointerMoved(*at);
    shell.command(Command::Right);
    if (home.sector.x + 1 < game.galaxy.width) {
        NGS_CHECK(screen.chosen() == (Sector{home.sector.x + 1, home.sector.y}));
    }
    // The Map button shows the whole galaxy, where every sector is in view.
    click(shell, centre(element(assets, "kmaingal.wdf", GalaxyScreen::kWholeMap)));
    NGS_CHECK(screen.wholeMap());
    NGS_CHECK(screen.sectorCentre({game.galaxy.width - 1, game.galaxy.height - 1}).has_value());
    shell.draw(canvas, 0);

    // Back opens the marker window; Back again closes it.
    shell.command(Command::Back);
    NGS_CHECK(shell.markerOpen());
    shell.command(Command::Back);
    NGS_CHECK(!shell.markerOpen() && shell.showing() == Shell::Showing::Galaxy);
    // Its emblem opens the main menu, where Continue goes back to the game.
    shell.command(Command::Back);
    click(shell, centre(*shell.markerButton(Shell::kMarkerMenu)));
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kContinue)));
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
    fs::remove(settings);
}

NGS_TEST("the home system's screen: people at work, the build queue and the next turn") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path settings = fs::temp_directory_path() / "nagus-test-system-settings.txt";
    fs::remove(settings);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Shell shell(res, settings);
    Canvas canvas;
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
    click(shell, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    shell.setSeed(5);
    shell.command(Command::Start); // the Federation, chosen by default
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
    const Game& game = *shell.game();

    // A click on the panel below the map opens the chosen home system.
    click(shell, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kInfo)));
    NGS_CHECK(shell.showing() == Shell::Showing::System);
    SystemScreen& screen = *shell.systemScreen();
    const Colony& colony = game.colonies[static_cast<std::size_t>(screen.colony())];
    NGS_CHECK(colony.owner == game.player && colony.home);
    shell.draw(canvas, 0);

    // Production: one fewer worker on the farms, then back.
    click(shell, centre(element(assets, "hsolars.wdf", SystemScreen::kProduction)));
    NGS_CHECK(screen.tab() == SystemScreen::Tab::Production);
    const Rect middle = element(assets, "hsolars.wdf", SystemScreen::kMiddle);
    const int farmers = colony.working[static_cast<std::size_t>(Output::Food)];
    Rect less = element(assets, "hlabor.wdf", SystemScreen::kLaborRows[0][2]);
    less.x += middle.x;
    less.y += middle.y;
    click(shell, centre(less));
    NGS_CHECK(colony.working[static_cast<std::size_t>(Output::Food)] == farmers - 1);
    shell.draw(canvas, 0);

    // The build list: a click chooses a line, a second click builds it.
    shell.command(Command::Next);
    shell.command(Command::Next);
    NGS_CHECK(screen.tab() == SystemScreen::Tab::BuildList);
    shell.draw(canvas, 0);
    NGS_CHECK(!screen.options().empty());
    NGS_CHECK(screen.options().size() >= 2);
    const auto line = screen.optionRect(1);
    NGS_CHECK(line.has_value());
    click(shell, centre(*line));
    NGS_CHECK(screen.chosenOption() == 1 && colony.queue.empty());
    click(shell, centre(*line));
    NGS_CHECK(colony.queue.size() == 1);
    shell.draw(canvas, 0);

    // The marker window leads to research, where a click on More gives
    // biotech a larger share.
    shell.command(Command::Back);
    NGS_CHECK(shell.markerOpen());
    click(shell, centre(*shell.markerButton(Shell::kMarkerResearch)));
    NGS_CHECK(shell.showing() == Shell::Showing::Research);
    const int biotech = game.empires[static_cast<std::size_t>(game.player)].share[0];
    click(shell, centre(shell.researchScreen()->fieldRect(0, ResearchScreen::kMore)));
    NGS_CHECK(game.empires[static_cast<std::size_t>(game.player)].share[0] == biotech + ResearchScreen::kShareStep);
    shell.draw(canvas, 0);

    // Back on the map, the turn ends with the Turn button.
    shell.command(Command::Back);
    click(shell, centre(*shell.markerButton(Shell::kMarkerGalaxy)));
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
    // A fleet at home: chosen in the list, then sent to a sector nearby.
    GalaxyScreen& map = *shell.galaxyScreen();
    NGS_CHECK(map.fleetsHere().size() == 2 && map.fleetLine(0).has_value());
    click(shell, centre(*map.fleetLine(0)));
    NGS_CHECK(map.chosenFleet() > 0 && map.moving());
    const Sector home = map.chosen();
    std::optional<Sector> target;
    for (const Sector d : {Sector{1, 0}, Sector{-1, 0}, Sector{0, 1}, Sector{0, -1}}) {
        const Sector s{home.x + d.x, home.y + d.y};
        if (!target && map.sectorCentre(s)) {
            target = s;
        }
    }
    NGS_CHECK(target.has_value());
    click(shell, *map.sectorCentre(*target));
    NGS_CHECK(!map.moving());
    const Fleet* sent = game.fleet(map.chosenFleet());
    NGS_CHECK(sent != nullptr && sent->destination == target);
    shell.draw(canvas, 0);

    const int progress = colony.progress;
    for (int t = 0; t < 30 && !colony.queue.empty(); ++t) {
        click(shell, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kTurn)));
    }
    NGS_CHECK(game.turn > 1);
    NGS_CHECK(colony.queue.empty() || colony.progress > progress);
    shell.draw(canvas, 0);
    fs::remove(settings);
}

NGS_TEST("the gamepad's d-pad moves the pointer between buttons") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Shell shell(res, fs::temp_directory_path() / "nagus-test-pad-settings.txt");
    const Rect newGame = element(assets, "opening.wdf", MainMenu::kNewSinglePlayer);
    const Rect quit = element(assets, "opening.wdf", MainMenu::kQuit);
    const Rect load = element(assets, "opening.wdf", MainMenu::kLoad);
    shell.pointerMoved({10, 10});
    shell.command(Command::Down); // to the nearest usable button
    shell.command(Command::Down);
    const Point p = shell.pointer();
    NGS_CHECK(newGame.contains(p.x, p.y) || load.contains(p.x, p.y) || quit.contains(p.x, p.y));
    // Up from Load Game is New Single Player Game; press A there.
    shell.pointerMoved(centre(load));
    shell.command(Command::Up);
    NGS_CHECK(newGame.contains(shell.pointer().x, shell.pointer().y));
    shell.clickAtPointer();
    NGS_CHECK(shell.showing() == Shell::Showing::GameSetup);
    shell.command(Command::Back);
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);
}

NGS_TEST("saving from the main menu and loading the game again") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path dir = fs::temp_directory_path() / "nagus-test-saves";
    fs::remove_all(dir);
    fs::create_directories(dir);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Shell shell(res, dir / "settings.txt");
    Canvas canvas;
    // Without a game there is nothing to save, but the list can be opened.
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kSave)));
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kLoad)));
    NGS_CHECK(shell.showing() == Shell::Showing::Load);
    NGS_CHECK(shell.loadSaveScreen()->chosen() == -1);
    shell.draw(canvas, 0);
    shell.command(Command::Back);
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);

    click(shell, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
    click(shell, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    shell.setSeed(9);
    shell.command(Command::Start);
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
    const Bytes before = saveGame(*shell.game(), {});

    // Back opens the marker window; its menu button goes to the main menu.
    shell.command(Command::Back);
    const auto menu = shell.markerButton(Shell::kMarkerMenu);
    NGS_CHECK(menu.has_value());
    click(shell, centre(*menu));
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kSave)));
    NGS_CHECK(shell.showing() == Shell::Showing::Save);
    NGS_CHECK(shell.loadSaveScreen()->chosen() == 0);
    shell.draw(canvas, 0);
    click(shell, centre(element(assets, "loadsave.wdf", LoadSaveScreen::kAccept)));
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
    NGS_CHECK(fs::exists(saveSlotFile(shell.savesFolder(), 0)));
    const SaveSlot slot = readSaveSlot(saveSlotFile(shell.savesFolder(), 0));
    NGS_CHECK(slot.used && !slot.damaged && slot.info.turn == shell.game()->turn);

    // A damaged file in another place cannot be chosen.
    {
        std::ofstream bad(saveSlotFile(shell.savesFolder(), 3), std::ios::binary);
        bad << "NAGUSSAV and then nonsense";
    }
    shell.command(Command::Back);
    click(shell, centre(*shell.markerButton(Shell::kMarkerMenu)));
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kLoad)));
    NGS_CHECK(shell.showing() == Shell::Showing::Load);
    LoadSaveScreen& list = *shell.loadSaveScreen();
    NGS_CHECK(list.chosen() == 0);
    list.choose(3);
    NGS_CHECK(list.chosen() == 0);
    shell.draw(canvas, 0);
    // A second click on the chosen line loads it.
    click(shell, centre(element(assets, "loadsave.wdf", LoadSaveScreen::slotButton(0))));
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
    NGS_CHECK(saveGame(*shell.game(), {}) == before);
    fs::remove_all(dir);
}

NGS_TEST("the diplomacy screen: gifts and a declaration of war") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path settings = fs::temp_directory_path() / "nagus-test-diplomacy-settings.txt";
    fs::remove(settings);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Shell shell(res, settings);
    Canvas canvas;
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
    click(shell, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    shell.setSeed(6);
    shell.command(Command::Start);
    Game& game = const_cast<Game&>(*shell.game());
    // Everyone meets everyone.
    std::fill(game.explored.begin(), game.explored.end(), std::uint8_t{0x1f});
    (void)endTurn(game);

    shell.command(Command::Back);
    const auto r = shell.markerButton(Shell::kMarkerDiplomacy);
    NGS_CHECK(r.has_value());
    click(shell, centre(*r));
    NGS_CHECK(shell.showing() == Shell::Showing::Diplomacy);
    DiplomacyScreen& d = *shell.diplomacyScreen();
    NGS_CHECK(d.races().size() > kEmpires);
    shell.draw(canvas, 0);

    // A gift to a minor race, by clicking.
    const auto minor = std::find_if(d.races().begin(), d.races().end(), [](int race) { return race >= kEmpires; });
    NGS_CHECK(minor != d.races().end());
    const int race = *minor;
    const auto line = d.raceLine(static_cast<int>(minor - d.races().begin()));
    if (line) {
        click(shell, centre(*line));
    } else {
        d.choose(race);
    }
    NGS_CHECK(d.chosen() == race);
    click(shell, centre(*d.button(DiplomacyScreen::kPropose)));
    NGS_CHECK(d.tab() == DiplomacyScreen::Tab::Propose);
    click(shell, centre(*d.button(DiplomacyScreen::kGift)));
    NGS_CHECK(d.dialog() == DiplomacyScreen::Dialog::Gift);
    shell.draw(canvas, 0);
    const int credits = game.empires[1].credits;
    const int before = regard(game, race, 1);
    click(shell, centre(*d.button(DiplomacyScreen::kSend)));
    NGS_CHECK(d.dialog() == DiplomacyScreen::Dialog::None);
    NGS_CHECK(game.empires[1].credits < credits && regard(game, race, 1) > before && !d.reply().empty());

    // War on the Klingons.
    d.choose(3);
    click(shell, centre(*d.button(DiplomacyScreen::kDeclareWar)));
    NGS_CHECK(d.dialog() == DiplomacyScreen::Dialog::War);
    click(shell, centre(*d.button(DiplomacyScreen::kSend)));
    NGS_CHECK(atWar(game, 1, 3));
    // The race information.
    click(shell, centre(*d.button(DiplomacyScreen::kRaceInfo)));
    NGS_CHECK(d.tab() == DiplomacyScreen::Tab::RaceInfo);
    shell.draw(canvas, 0);
    // Back opens the marker window.
    shell.command(Command::Back);
    NGS_CHECK(shell.markerOpen());
}

NGS_TEST("the intelligence screen: the plan, the reports and the empires") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path settings = fs::temp_directory_path() / "nagus-test-intel-settings.txt";
    fs::remove(settings);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Shell shell(res, settings);
    Canvas canvas;
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
    click(shell, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    shell.setSeed(6);
    shell.command(Command::Start);
    Game& game = const_cast<Game&>(*shell.game());
    std::fill(game.explored.begin(), game.explored.end(), std::uint8_t{0x1f});
    (void)endTurn(game);

    shell.command(Command::Back);
    const auto r = shell.markerButton(Shell::kMarkerIntel);
    NGS_CHECK(r.has_value());
    click(shell, centre(*r));
    NGS_CHECK(shell.showing() == Shell::Showing::Intel);
    IntelScreen& s = *shell.intelScreen();
    NGS_CHECK(s.tab() == IntelScreen::Tab::Management);
    shell.draw(canvas, 0);
    // In the game the empire's theme plays, and buttons click.
    NGS_CHECK(shell.wantedTrack() == 1 + game.player && shell.jukebox().musicTrack() == 1 + game.player);
    NGS_CHECK(shell.clickSound() == 76 + game.player && shell.jukebox().soundsPlaying());

    // Security, and espionage against the first empire shown.
    EmpireState& me = game.empires[static_cast<std::size_t>(game.player)];
    NGS_CHECK(me.security == 100);
    click(shell, centre(*s.button(IntelScreen::kSecurityLess)));
    NGS_CHECK(me.security == 100 - kIntelStep);
    click(shell, centre(*s.button(IntelScreen::kSecurityMore)));
    NGS_CHECK(me.security == 100);
    const int target = s.quarterEmpire(0);
    NGS_CHECK(target >= 0 && target != game.player);
    const int before = me.intel[static_cast<std::size_t>(target)].espionage;
    click(shell, centre(*s.button(IntelScreen::kRowMore[0])));
    NGS_CHECK(me.intel[static_cast<std::size_t>(target)].espionage == before + kIntelStep);
    click(shell, centre(*s.button(IntelScreen::kFocus[3])));
    NGS_CHECK(me.intel[static_cast<std::size_t>(target)].focus == IntelFocus::Military);

    // The report and the empires' status, by tab.
    shell.command(Command::Next);
    NGS_CHECK(s.tab() == IntelScreen::Tab::Report);
    shell.draw(canvas, 0);
    click(shell, centre(*s.button(IntelScreen::kStatus)));
    NGS_CHECK(s.tab() == IntelScreen::Tab::Status);
    shell.draw(canvas, 0);
    shell.command(Command::Back);
    NGS_CHECK(shell.markerOpen());
}

NGS_TEST("films: the opening film and the empire's film") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path settings = fs::temp_directory_path() / "nagus-test-film-settings.txt";
    fs::remove(settings);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Shell shell(res, settings);
    Canvas canvas;
    shell.enableFilms();
    NGS_CHECK(shell.showing() == Shell::Showing::Film);
    shell.draw(canvas, 1000);
    NGS_CHECK(shell.filmScreen()->shownFrame() == 0 && shell.jukebox().soundsPlaying());
    NGS_CHECK(shell.wantedTrack() == -1);
    shell.draw(canvas, 2000); // a second on: frame 24
    NGS_CHECK(shell.filmScreen()->shownFrame() == 24);
    // A click ends it.
    click(shell, {400, 300});
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu && !shell.jukebox().soundsPlaying());

    click(shell, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
    click(shell, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    shell.setSeed(3);
    shell.command(Command::Start);
    NGS_CHECK(shell.showing() == Shell::Showing::Film);
    shell.draw(canvas, 0);
    // At its end the game begins.
    shell.draw(canvas, 60 * 1000);
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
}

NGS_TEST("the end of a game: pictures, speech and the hall of fame") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path dir = fs::temp_directory_path() / "nagus-test-ending";
    fs::remove_all(dir);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    // The pictures come from the second archive.
    NGS_CHECK(assets.image("hw.tga").width == 800 && assets.image("hw.tga").height == 600);
    Shell shell(res, dir / "settings.txt");
    Canvas canvas;
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
    click(shell, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    shell.setSeed(9);
    shell.command(Command::Start);
    Game& game = const_cast<Game&>(*shell.game());
    NGS_CHECK(game.player == 1);
    game.ending = Ending::Domination;
    shell.draw(canvas, 0);
    NGS_CHECK(shell.showing() == Shell::Showing::Ending && shell.game() == nullptr);
    NGS_CHECK(shell.endingScreen()->picture() == "hw.tga" && shell.jukebox().soundsPlaying());
    NGS_CHECK(shell.wantedTrack() == -1);
    shell.draw(canvas, 7000); // past the first wait
    NGS_CHECK(shell.endingScreen()->picture() == "h_fchq.tga");
    // A click goes on to the hall of fame, with this game in it.
    click(shell, {400, 300});
    NGS_CHECK(shell.showing() == Shell::Showing::Fame);
    NGS_CHECK(shell.fameScreen()->entries().size() == 1);
    NGS_CHECK(shell.fameScreen()->entries()[0].description.rfind("Victory by domination", 0) == 0);
    shell.draw(canvas, 0);
    shell.command(Command::Back);
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);

    // The hall of fame from the main menu.
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kHallOfFame)));
    NGS_CHECK(shell.showing() == Shell::Showing::Fame && shell.fameScreen()->entries().size() == 1);
    click(shell, centre(element(assets, "fame.wdf", FameScreen::kReturn)));
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);

    // Retiring from a game.
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
    click(shell, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    shell.command(Command::Start);
    shell.command(Command::Back);
    const auto menu = shell.markerButton(Shell::kMarkerMenu);
    NGS_CHECK(menu.has_value());
    click(shell, centre(*menu));
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kRetire)));
    NGS_CHECK(shell.showing() == Shell::Showing::Fame && shell.game() == nullptr);
    NGS_CHECK(shell.fameScreen()->entries().size() == 2);
    fs::remove_all(dir);
}
