// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/game/combat.hpp"
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

template <typename T>
void click(T& screen, Point p) {
    screen.pointerMoved(p);
    screen.pointerPressed(p);
    screen.pointerReleased(p);
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
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kRetire)));
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

NGS_TEST("the empire summary: last turn's events, treaties and the systems") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path settings = fs::temp_directory_path() / "nagus-test-summary-settings.txt";
    fs::remove(settings);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Shell shell(res, settings);
    Canvas canvas;
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
    click(shell, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    shell.setSeed(5);
    shell.command(Command::Start);
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
    // A few turns, for something to report and races to meet.
    for (int t = 0; t < 4; ++t) {
        click(shell, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kTurn)));
    }
    click(shell, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kSummary)));
    NGS_CHECK(shell.showing() == Shell::Showing::Summary);
    SummaryScreen& s = *shell.summaryScreen();
    NGS_CHECK(s.page() == SummaryScreen::Page::Events);
    NGS_CHECK(s.rows().size() >= shell.galaxyScreen()->events().size());
    shell.draw(canvas, 0);
    shell.command(Command::Next);
    NGS_CHECK(s.page() == SummaryScreen::Page::Relations);
    for (const auto& r : s.rows()) {
        NGS_CHECK(!r.cells[0].empty() && !r.cells[1].empty());
    }
    click(shell, centre(s.rect(SummaryScreen::kSystems)));
    NGS_CHECK(s.page() == SummaryScreen::Page::Systems);
    std::size_t mine = 0;
    for (const auto& c : shell.game()->colonies) {
        mine += c.owner == shell.game()->player ? 1u : 0u;
    }
    NGS_CHECK(s.rows().size() == mine && mine >= 1);
    shell.draw(canvas, 0);
    // A system opens on the system screen.
    const int colony = s.rows()[0].colony;
    click(shell, centre(*s.rowRect(0)));
    NGS_CHECK(shell.showing() == Shell::Showing::System);
    NGS_CHECK(shell.systemScreen()->colony() == colony);
    // Close goes back to the map.
    shell.command(Command::Back);
    click(shell, centre(*shell.markerButton(Shell::kMarkerGalaxy)));
    click(shell, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kSummary)));
    click(shell, centre(shell.summaryScreen()->rect(SummaryScreen::kClose)));
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

    // Structures: a click on one asks how many of its kind to scrap.
    click(shell, centre(element(assets, "hsolars.wdf", SystemScreen::kStructures)));
    NGS_CHECK(screen.tab() == SystemScreen::Tab::Structures);
    const std::size_t structures = colony.structures.size();
    NGS_CHECK(structures > 0 && screen.structureRect(0).has_value());
    const int kind = colony.structures[0].building;
    click(shell, centre(*screen.structureRect(0)));
    NGS_CHECK(screen.scrapOpen() && screen.scrapCount() == 1);
    shell.draw(canvas, 0);
    click(shell, centre(*screen.scrapButton(SystemScreen::kScrapUp))); // no more than there are
    const auto sameKind = static_cast<int>(std::count_if(colony.structures.begin(), colony.structures.end(),
                                                         [&](const Structure& st) { return st.building == kind; }));
    NGS_CHECK(screen.scrapCount() == std::min(2, sameKind));
    click(shell, centre(*screen.scrapButton(SystemScreen::kScrapDown)));
    shell.command(Command::Back); // cancels
    NGS_CHECK(!screen.scrapOpen() && colony.structures.size() == structures);
    click(shell, centre(*screen.structureRect(0)));
    click(shell, centre(*screen.scrapButton(SystemScreen::kScrapApply)));
    NGS_CHECK(!screen.scrapOpen() && colony.structures.size() == structures - 1);
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
    // Up twice from Load Game is New Single Player Game; press A there.
    shell.pointerMoved(centre(load));
    shell.command(Command::Up);
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

NGS_TEST("a button's tooltip shows when the pointer rests on it") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path dir = fs::temp_directory_path() / "nagus-test-tooltips";
    fs::remove_all(dir);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Canvas canvas;
    Shell shell(res, dir / "settings.txt");
    const Point single = centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer));
    shell.pointerMoved(single);
    shell.draw(canvas, 1000);
    NGS_CHECK(!shell.tooltipShown()); // not at once
    shell.draw(canvas, 1500);
    NGS_CHECK(!shell.tooltipShown());
    shell.draw(canvas, 1800);
    NGS_CHECK(shell.tooltipShown() == MainMenu::kNewSinglePlayer);
    // Another button starts again; beside the buttons there is none.
    shell.pointerMoved(centre(element(assets, "opening.wdf", MainMenu::kQuit)));
    shell.draw(canvas, 1900);
    NGS_CHECK(!shell.tooltipShown());
    shell.draw(canvas, 2700);
    NGS_CHECK(shell.tooltipShown() == MainMenu::kQuit);
    shell.pointerMoved({2, 2});
    shell.draw(canvas, 4000);
    NGS_CHECK(!shell.tooltipShown());
    // A press hides it; touch shows none.
    shell.pointerMoved(single);
    shell.draw(canvas, 5000);
    shell.pointerPressed(single);
    shell.draw(canvas, 6000);
    NGS_CHECK(!shell.tooltipShown());
    shell.pointerMoved({2, 2});
    shell.draw(canvas, 6100);
    shell.setPointerVisible(false);
    shell.pointerMoved(single);
    shell.draw(canvas, 6200);
    shell.draw(canvas, 7200);
    NGS_CHECK(!shell.tooltipShown());
    // With the pointer back it shows again.
    shell.setPointerVisible(true);
    shell.draw(canvas, 8000);
    NGS_CHECK(shell.tooltipShown() == MainMenu::kNewSinglePlayer);
    fs::remove_all(dir);
}

NGS_TEST("the options: volumes, kept with the settings") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path dir = fs::temp_directory_path() / "nagus-test-options";
    fs::remove_all(dir);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Canvas canvas;
    {
        Shell shell(res, dir / "settings.txt");
        click(shell, centre(element(assets, "opening.wdf", MainMenu::kOptions)));
        NGS_CHECK(shell.showing() == Shell::Showing::Options);
        OptionsScreen& o = *shell.optionsScreen();
        shell.draw(canvas, 0);
        // Music and speech set on their bars, sound effects by the gamepad.
        const Rect m = o.bar(0);
        click(shell, {m.x + m.w * 6 / 10, m.centerY()});
        NGS_CHECK(o.settings().volume[0] == 60 && shell.jukebox().musicVolume() == 60);
        shell.command(Command::Down);
        shell.command(Command::Left);
        NGS_CHECK(o.chosenRow() == 1 && o.settings().volume[1] == 70 && shell.jukebox().effectsVolume() == 70);
        const Rect b = o.bar(2);
        click(shell, {b.x + b.w / 2, b.centerY()});
        NGS_CHECK(o.settings().volume[2] == 50);
        // The switches: tooltips and tactical combat by a click, saving each
        // turn by the gamepad.
        NGS_CHECK(o.settings().tooltips);
        click(shell, centre(*o.button(OptionsScreen::kTooltips)));
        NGS_CHECK(!o.settings().tooltips && o.chosenRow() == OptionsScreen::kVolumes);
        click(shell, centre(*o.button(OptionsScreen::kTooltips)));
        NGS_CHECK(o.settings().tooltips);
        const bool automatic = o.settings().automaticCombat;
        click(shell, centre(*o.button(OptionsScreen::kTactical)));
        NGS_CHECK(o.settings().automaticCombat != automatic);
        NGS_CHECK(o.chosenRow() == OptionsScreen::kVolumes + 1);
        shell.command(Command::Down);
        shell.command(Command::Down);
        NGS_CHECK(o.chosenRow() == OptionsScreen::kVolumes + 3);
        shell.command(Command::Right);
        NGS_CHECK(!o.settings().autosave);
        click(shell, centre(*o.button(OptionsScreen::kAccept)));
        NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);
        // Cancel keeps what was there.
        click(shell, centre(element(assets, "opening.wdf", MainMenu::kOptions)));
        const Rect all = shell.optionsScreen()->bar(3);
        click(shell, {all.x + 1, all.centerY()});
        NGS_CHECK(shell.jukebox().musicVolume() == 0);
        click(shell, centre(*shell.optionsScreen()->button(OptionsScreen::kCancel)));
        NGS_CHECK(shell.showing() == Shell::Showing::MainMenu && shell.jukebox().musicVolume() == 60);
        NGS_CHECK(shell.settings().volume[3] == 100 && !shell.settings().autosave);
    }
    // Kept for the next start.
    const GameSettings s = loadSettings(dir / "settings.txt");
    NGS_CHECK(s.volume[0] == 60 && s.volume[1] == 70 && s.volume[2] == 50 && !s.autosave);
    Shell again(res, dir / "settings.txt");
    NGS_CHECK(again.jukebox().musicVolume() == 60);
    NGS_CHECK(again.jukebox().speechVolume() == 50);
    NGS_CHECK(again.settings().volume[2] == 50);
    fs::remove_all(dir);
}

NGS_TEST("tactical combat: choosing manual orders, fighting rounds and back to the map") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path settings = fs::temp_directory_path() / "nagus-test-combat-settings.txt";
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
    NGS_CHECK(!game.settings.automaticCombat);
    // Klingon warships wait at the player's home, at war.
    for (auto& r : game.relations) {
        r.treaty = Treaty::War;
    }
    const Sector home = game.galaxy.systems[static_cast<std::size_t>(game.galaxy.homes[static_cast<std::size_t>(game.player)])].sector;
    int warship = -1;
    for (std::size_t i = 0; i < game.economy.ships.size() && warship < 0; ++i) {
        if (game.economy.ships[i].race == 3 && isArmed(game.economy.ships[i]) && game.economy.ships[i].role != ShipRole::Scout) {
            warship = static_cast<int>(i);
        }
    }
    for (int i = 0; i < 2; ++i) {
        game.addFleet(3, home, warship);
        game.addFleet(game.player, home, warship);
    }
    shell.draw(canvas, 0);
    const int turn = game.turn;
    click(shell, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kTurn)));
    NGS_CHECK(shell.showing() == Shell::Showing::Combat);
    NGS_CHECK(game.turn == turn); // halfway
    CombatScreen& c = *shell.combatScreen();
    NGS_CHECK(c.phase() == CombatScreen::Phase::Choose);
    shell.draw(canvas, 0);
    click(shell, centre(*c.button(CombatScreen::kManual)));
    NGS_CHECK(c.phase() == CombatScreen::Phase::Orders);
    // The battle shows in 3D with the game's models; Tactical shows it flat,
    // and the shoulder buttons switch back.
    NGS_CHECK(c.view() == CombatScreen::View::Visual);
    NGS_CHECK(c.hasModel(0));
    NGS_CHECK(!c.maneuverText(Maneuver::Hail).empty() && c.maneuverText(Maneuver::Hail).size() < 20);
    shell.draw(canvas, 50);
    for (std::size_t i = 0; i < c.battle().units().size(); ++i) {
        const Rect r = c.unitRect(i);
        NGS_CHECK(CombatScreen::kView.contains(r.centerX(), r.centerY()));
    }
    click(shell, centre(*c.button(CombatScreen::kTactical)));
    NGS_CHECK(c.view() == CombatScreen::View::Tactical);
    shell.command(Command::Next);
    NGS_CHECK(c.view() == CombatScreen::View::Visual);
    // The camera panel: turn, zoom, the list of views, the status labels.
    click(shell, centre(*c.button(CombatScreen::kCameraLeft)));
    NGS_CHECK(c.cameraTurn() < 0);
    click(shell, centre(*c.button(CombatScreen::kZoomIn)));
    NGS_CHECK(c.cameraZoom() < 1);
    NGS_CHECK(!c.button(CombatScreen::kChaseView));
    click(shell, centre(*c.button(CombatScreen::kViews)));
    NGS_CHECK(c.viewsOpen());
    for (const std::uint32_t v : {CombatScreen::kTopView, CombatScreen::kChaseView, CombatScreen::kFrontView, CombatScreen::kEnemyView,
                                  CombatScreen::kShipView}) {
        if (!c.viewsOpen()) {
            click(shell, centre(*c.button(CombatScreen::kViews)));
        }
        click(shell, centre(*c.button(v)));
        NGS_CHECK(!c.viewsOpen() && c.cameraZoom() == 1.0f);
        shell.draw(canvas, 60);
    }
    NGS_CHECK(c.cameraView() == CombatScreen::CameraView::Ship);
    click(shell, centre(*c.button(CombatScreen::kPanorama)));
    NGS_CHECK(c.cameraView() == CombatScreen::CameraView::Panorama && c.cameraTurn() == 0.0f);
    click(shell, centre(*c.button(CombatScreen::kStatus)));
    NGS_CHECK(!c.statusShown());
    click(shell, centre(*c.button(CombatScreen::kStatus)));
    // Dragging across the view turns the camera.
    shell.pointerMoved({400, 200});
    shell.pointerPressed({400, 200});
    shell.pointerMoved({450, 200});
    shell.pointerReleased({450, 200});
    NGS_CHECK(c.cameraTurn() > 0.4f);
    // The gamepad chooses the maneuver and waits on Turn.
    shell.command(Command::Right);
    NGS_CHECK(c.chosen() == Maneuver::Charge);
    NGS_CHECK(shell.pointer().x == centre(*c.button(CombatScreen::kTurn)).x);
    click(shell, centre(c.maneuverRect(Maneuver::Circle)));
    NGS_CHECK(c.chosen() == Maneuver::Circle);
    shell.command(Command::Left);
    NGS_CHECK(c.chosen() == Maneuver::Charge);
    shell.clickAtPointer();
    shell.draw(canvas, 100);
    NGS_CHECK(c.battle().round() == 1);
    // The round plays as a tape, which the buttons below the camera panel
    // play again, stop, wind and rewind.
    NGS_CHECK(!c.battle().lastShots().empty() && c.tapeLength() > 0);
    NGS_CHECK(c.tapeRate() == 1 && !c.button(CombatScreen::kPlay));
    shell.draw(canvas, 300);
    NGS_CHECK(c.tapeTime() == 200);
    click(shell, centre(*c.button(CombatScreen::kStop)));
    NGS_CHECK(c.tapeRate() == 0 && c.button(CombatScreen::kPlay));
    click(shell, centre(*c.button(CombatScreen::kTapeEnd)));
    NGS_CHECK(c.tapeTime() == c.tapeLength());
    click(shell, centre(*c.button(CombatScreen::kRewind)));
    NGS_CHECK(c.tapeRate() < 0);
    shell.draw(canvas, 400);
    NGS_CHECK(c.tapeTime() == c.tapeLength() - 300);
    click(shell, centre(*c.button(CombatScreen::kTapeStart)));
    NGS_CHECK(c.tapeTime() == 0 && c.tapeRate() == 0);
    click(shell, centre(*c.button(CombatScreen::kPlay)));
    click(shell, centre(*c.button(CombatScreen::kForward)));
    NGS_CHECK(c.tapeRate() == 3);
    for (std::uint64_t ms = 500; ms < 500 + 20000; ms += 200) {
        shell.draw(canvas, ms);
    }
    NGS_CHECK(c.tapeTime() == c.tapeLength() && c.tapeRate() == 0);
    for (int i = 0; i < kTacticalRounds && c.phase() == CombatScreen::Phase::Orders; ++i) {
        click(shell, centre(*c.button(CombatScreen::kTurn)));
        shell.draw(canvas, 200);
    }
    NGS_CHECK(c.phase() == CombatScreen::Phase::Over && !c.resultText().empty());
    click(shell, centre(*c.button(CombatScreen::kFinish)));
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
    NGS_CHECK(game.turn == turn + 1);

    // Next time: fought automatically.
    game.addFleet(3, home, warship);
    game.addFleet(game.player, home, warship);
    click(shell, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kTurn)));
    if (shell.showing() == Shell::Showing::Combat) {
        click(shell, centre(*shell.combatScreen()->button(CombatScreen::kAutomatic)));
        NGS_CHECK(shell.combatScreen()->phase() == CombatScreen::Phase::Over);
        shell.draw(canvas, 300);
        shell.command(Command::Start);
    }
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy && game.turn == turn + 2);

    // And then hailing: the battle ends at once, or is fought for the player.
    game.addFleet(3, home, warship);
    game.addFleet(game.player, home, warship);
    click(shell, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kTurn)));
    if (shell.showing() == Shell::Showing::Combat) {
        NGS_CHECK(shell.combatScreen()->button(CombatScreen::kHail).has_value());
        click(shell, centre(*shell.combatScreen()->button(CombatScreen::kHail)));
        NGS_CHECK(shell.combatScreen()->phase() == CombatScreen::Phase::Over);
        shell.command(Command::Start);
    }
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy && game.turn == turn + 3);
    fs::remove(settings);
}

NGS_TEST("saving each turn, and Continue after a new start") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path dir = fs::temp_directory_path() / "nagus-test-autosave";
    fs::remove_all(dir);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Canvas canvas;
    int turn = 0;
    {
        Shell shell(res, dir / "settings.txt");
        click(shell, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
        click(shell, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
        shell.setSeed(6);
        shell.command(Command::Start);
        shell.draw(canvas, 0);
        NGS_CHECK(!fs::exists(shell.autosaveFile())); // nothing to keep yet
        Game& game = const_cast<Game&>(*shell.game());
        game.settings.automaticCombat = true;
        click(shell, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kTurn)));
        shell.draw(canvas, 0);
        NGS_CHECK(fs::exists(shell.autosaveFile()));
        turn = game.turn;
    }
    Shell again(res, dir / "settings.txt");
    NGS_CHECK(again.game() == nullptr);
    click(again, centre(element(assets, "opening.wdf", MainMenu::kContinue)));
    NGS_CHECK(again.showing() == Shell::Showing::Galaxy);
    NGS_CHECK(again.game() != nullptr && again.game()->turn == turn);
    fs::remove_all(dir);
}

NGS_TEST("a multiplayer game: hosting, joining, empires and a turn together") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const fs::path dir = fs::temp_directory_path() / "nagus-test-multiplayer";
    fs::remove_all(dir);
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    Canvas canvas;
    Shell host(res, dir / "host" / "settings.txt");
    Shell guest(res, dir / "guest" / "settings.txt");
    host.setGamePort(0);
    host.setSeed(5);
    std::uint64_t ms = 0;
    auto pump = [&](auto done) {
        for (int i = 0; i < 3000 && !done(); ++i) {
            ms += 5;
            host.draw(canvas, ms);
            guest.draw(canvas, ms);
        }
        return done();
    };
    auto type = [&](Shell& s, std::uint32_t id, const std::string& text) {
        click(s, centre(s.multiplayerScreen()->field(id)));
        NGS_CHECK(s.wantsText());
        s.typed(text);
        s.typed("\n");
    };

    // The host: a name, Host, the game set-up, then the lobby.
    click(host, centre(element(assets, "opening.wdf", MainMenu::kNewMultiPlayer)));
    NGS_CHECK(host.showing() == Shell::Showing::Multiplayer);
    MultiplayerScreen& hm = *host.multiplayerScreen();
    click(host, centre(*hm.button(MultiplayerScreen::kHost)));
    NGS_CHECK(host.showing() == Shell::Showing::Multiplayer && !hm.status().empty()); // no name yet
    type(host, MultiplayerScreen::kPlayerName, "Picard");
    NGS_CHECK(hm.playerName() == "Picard" && !host.wantsText());
    click(host, centre(*hm.button(MultiplayerScreen::kHost)));
    NGS_CHECK(host.showing() == Shell::Showing::GameSetup && host.hostSession() != nullptr);
    click(host, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    NGS_CHECK(host.showing() == Shell::Showing::Multiplayer);
    host.draw(canvas, ms);
    NGS_CHECK(host.multiplayerScreen()->mode() == MultiplayerScreen::Mode::Lobby);

    // The guest joins with the host's address.
    click(guest, centre(element(assets, "opening.wdf", MainMenu::kNewMultiPlayer)));
    MultiplayerScreen& gm = *guest.multiplayerScreen();
    type(guest, MultiplayerScreen::kPlayerName, "Riker");
    click(guest, centre(*gm.button(MultiplayerScreen::kJoin)));
    NGS_CHECK(gm.focus() == MultiplayerScreen::kAddress);
    guest.typed("127.0.0.1:" + std::to_string(host.hostSession()->port()) + "\n");
    click(guest, centre(*gm.button(MultiplayerScreen::kConnect)));
    NGS_CHECK(guest.clientSession() != nullptr);
    NGS_CHECK(pump([&] { return gm.mode() == MultiplayerScreen::Mode::Lobby && guest.clientSession()->lobby().players.size() == 2; }));

    // Empires: the host has the Federation; the guest cannot choose it.
    click(host, centre(*host.multiplayerScreen()->button(MultiplayerScreen::kChooseEmpire)));
    NGS_CHECK(host.showing() == Shell::Showing::EmpireSelect);
    click(host, centre(element(assets, "emppick.wdf", EmpireSelect::kAccept)));
    NGS_CHECK(host.showing() == Shell::Showing::Multiplayer && host.hostSession()->lobby().players[0].empire == 1);
    NGS_CHECK(pump([&] { return guest.clientSession()->lobby().players[0].empire == 1; }));
    click(guest, centre(*gm.button(MultiplayerScreen::kChooseEmpire)));
    NGS_CHECK(guest.showing() == Shell::Showing::EmpireSelect);
    click(guest, centre(element(assets, "emppick.wdf", EmpireSelect::kAccept)));
    NGS_CHECK(pump([&] { return host.hostSession()->canStart(); }));
    NGS_CHECK(host.hostSession()->lobby().players[1].empire != 1);

    // The host starts; both play.
    click(host, centre(*host.multiplayerScreen()->button(MultiplayerScreen::kStart)));
    NGS_CHECK(host.showing() == Shell::Showing::Galaxy);
    NGS_CHECK(pump([&] { return guest.showing() == Shell::Showing::Galaxy; }));
    NGS_CHECK(guest.game()->player != 1 && guest.game()->humans[1] && host.game()->multiplayer());
    const int turn = host.game()->turn;
    click(host, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kTurn)));
    NGS_CHECK(host.waitingForPlayers());
    host.draw(canvas, ms);
    NGS_CHECK(host.game()->turn == turn);
    const char* guestMap = guest.game()->player == 0 ? "cmaingal.wdf" : guest.game()->player == 2 ? "fmaingal.wdf"
                           : guest.game()->player == 3 ? "kmaingal.wdf" : "rmaingal.wdf";
    click(guest, centre(element(assets, guestMap, GalaxyScreen::kTurn)));
    NGS_CHECK(pump([&] { return !host.waitingForPlayers() && !guest.waitingForPlayers(); }));
    NGS_CHECK(host.game()->turn == turn + 1 && guest.game()->turn == turn + 1);

    // Chat: the guest types a line and sends it; the host sees it at the
    // top, opens the chat from it and answers.
    guest.command(Command::Chat);
    NGS_CHECK(guest.chatWindow() != nullptr);
    NGS_CHECK(guest.wantsText());
    guest.typed("Hello there");
    guest.typed("\n");
    NGS_CHECK(guest.chatWindow()->line().empty());
    NGS_CHECK(pump([&] { return host.chatLog().size() == 1; }));
    NGS_CHECK(host.chatLog()[0].text == "Hello there" && host.chatLog()[0].empire == guest.game()->player);
    NGS_CHECK(pump([&] { return guest.chatLog().size() == 1; })); // back through the host
    host.draw(canvas, ms);
    click(host, {400, 20});
    NGS_CHECK(host.chatWindow() != nullptr);
    host.typed("Hi");
    click(host, centre(host.chatWindow()->rect(ChatWindow::kSend)));
    NGS_CHECK(pump([&] { return guest.chatLog().size() == 2; }));
    NGS_CHECK(guest.chatLog()[1].text == "Hi" && guest.chatLog()[1].empire == 1);
    // Ignored: the host no longer hears the guest.
    int place = 0;
    while (host.chatWindow()->empireAt(place) != guest.game()->player) {
        ++place;
    }
    click(host, centre(host.chatWindow()->rect(ChatWindow::kIgnore[place])));
    guest.typed("Can you hear me?\n");
    NGS_CHECK(pump([&] { return guest.chatLog().size() == 3; }));
    host.draw(canvas, ms);
    NGS_CHECK(host.chatLog().size() == 2);
    click(host, centre(host.chatWindow()->rect(ChatWindow::kClose)));
    guest.command(Command::Back);
    NGS_CHECK(host.chatWindow() == nullptr && guest.chatWindow() == nullptr);

    // The host leaves: the guest's game cannot go on.
    host.command(Command::Back);
    click(host, centre(*host.markerButton(Shell::kMarkerMenu)));
    NGS_CHECK(host.showing() == Shell::Showing::MainMenu);
    click(host, centre(element(assets, "opening.wdf", MainMenu::kNewSinglePlayer)));
    click(host, centre(element(assets, "gmesetup.wdf", GameSetup::kAccept)));
    click(host, centre(element(assets, "emppick.wdf", EmpireSelect::kAccept)));
    NGS_CHECK(!host.multiplayer());
    NGS_CHECK(pump([&] { return guest.noticeOpen(); }));
    NGS_CHECK(guest.game() == nullptr && !guest.multiplayer());
    fs::remove_all(dir);
}

NGS_TEST("the fleet list on the map scrolls through many fleets") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    const EconomyData economy = loadEconomyData(*res);
    GameSettings settings;
    settings.empire = 1;
    Game g = newGame(settings, loadGalaxyData(*res, assets.lexicon()), economy, 3);
    const Sector home = g.galaxy.systems[static_cast<std::size_t>(g.galaxy.homes[1])].sector;
    const int cls = g.fleets.front().ships.front().shipClass;
    for (int i = 0; i < 15; ++i) {
        g.addFleet(1, home, cls);
    }
    GalaxyScreen screen(assets, g);
    screen.choose(home);
    Canvas canvas;
    screen.draw(canvas, 0);
    const auto here = screen.fleetsHere();
    NGS_CHECK(here.size() >= 15);
    NGS_CHECK(screen.fleetLine(0) && !screen.fleetLine(GalaxyScreen::kFleetLines));
    const Rect down = element(assets, "hmaingal.wdf", GalaxyScreen::kListDown);
    click(screen, centre(down));
    const int last = static_cast<int>(here.size()) - 1;
    NGS_CHECK(screen.firstFleetLine() == std::min(GalaxyScreen::kFleetLines - 1, last + 1 - GalaxyScreen::kFleetLines));
    NGS_CHECK(!screen.fleetLine(0) && screen.fleetLine(GalaxyScreen::kFleetLines));
    // The last one can be chosen.
    click(screen, centre(down));
    NGS_CHECK(screen.fleetLine(last).has_value());
    click(screen, centre(*screen.fleetLine(last)));
    NGS_CHECK(screen.chosenFleet() == here.back());
    click(screen, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kListUp)));
    click(screen, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kListUp)));
    NGS_CHECK(screen.firstFleetLine() == 0);
}

NGS_TEST("redeploying ships between fleets, and the avoid order, on the map") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    const EconomyData economy = loadEconomyData(*res);
    GameSettings settings;
    settings.empire = 1;
    Game g = newGame(settings, loadGalaxyData(*res, assets.lexicon()), economy, 3);
    const Sector home = g.galaxy.systems[static_cast<std::size_t>(g.galaxy.homes[1])].sector;
    g.fleets.clear();
    const int cls = economy.ships.size() > 22 ? 22 : 0;
    Fleet& f = g.addFleet(1, home, cls);
    f.ships.push_back(f.ships.front());
    f.ships.push_back(f.ships.front());
    const int id = f.id;
    GalaxyScreen screen(assets, g);
    screen.choose(home);
    Canvas canvas;
    screen.draw(canvas, 0);
    click(screen, centre(*screen.fleetLine(0)));
    NGS_CHECK(screen.chosenFleet() == id);
    screen.draw(canvas, 0);
    const Rect info = element(assets, "hmaingal.wdf", GalaxyScreen::kInfo);
    const auto* redeploy = assets.layout("h_tfship.wdf").find(GalaxyScreen::kFleetRedeploy);
    NGS_CHECK(redeploy != nullptr);
    click(screen, {info.x + redeploy->rect.x + redeploy->rect.w / 2, info.y + redeploy->rect.y + redeploy->rect.h / 2});
    NGS_CHECK(screen.redeploying());
    // A ship taken out, then joined back.
    click(screen, centre(*screen.shipCell(0)));
    NGS_CHECK(g.fleets.size() == 2 && g.fleet(id)->ships.size() == 2);
    screen.draw(canvas, 0);
    click(screen, centre(*screen.fleetLine(1)));
    NGS_CHECK(g.fleets.size() == 1 && g.fleet(id)->ships.size() == 3);
    // Orders: avoid battles.
    click(screen, {info.x + redeploy->rect.x + redeploy->rect.w / 2, info.y + redeploy->rect.y + redeploy->rect.h / 2});
    NGS_CHECK(!screen.redeploying());
    const auto* orders = assets.layout("h_tfship.wdf").find(GalaxyScreen::kFleetOrders);
    click(screen, {info.x + orders->rect.x + orders->rect.w / 2, info.y + orders->rect.y + orders->rect.h / 2});
    NGS_CHECK(screen.orderButton(GalaxyScreen::kOrderAvoid).has_value());
    screen.draw(canvas, 0);
    click(screen, centre(*screen.orderButton(GalaxyScreen::kOrderAvoid)));
    NGS_CHECK(g.fleet(id)->avoid);
}

NGS_TEST("the map's modes and layers") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    const EconomyData economy = loadEconomyData(*res);
    GameSettings settings;
    settings.empire = 1;
    Game g = newGame(settings, loadGalaxyData(*res, assets.lexicon()), economy, 3);
    GalaxyScreen screen(assets, g);
    Canvas canvas;
    screen.draw(canvas, 0);
    using L = GalaxyScreen::Layer;
    NGS_CHECK(screen.mode() == GalaxyScreen::Mode::Economy && screen.shows(L::Names) && !screen.shows(L::Ranges));
    click(screen, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kOverlay)));
    NGS_CHECK(screen.overlayOpen());
    screen.draw(canvas, 0);
    NGS_CHECK(screen.layerButton(L::TradeRoutes) && screen.layerButton(L::Bonuses));
    NGS_CHECK(!screen.shows(L::Bonuses));
    click(screen, centre(*screen.layerButton(L::Bonuses)));
    NGS_CHECK(screen.shows(L::Bonuses));
    screen.draw(canvas, 0);
    click(screen, centre(*screen.layerButton(L::Bonuses)));
    click(screen, centre(*screen.layerButton(L::Names)));
    click(screen, centre(*screen.layerButton(L::Ranges)));
    NGS_CHECK(!screen.shows(L::Names) && screen.shows(L::Ranges));
    screen.draw(canvas, 0);
    // Each mode keeps its own.
    click(screen, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kMilitary)));
    NGS_CHECK(screen.mode() == GalaxyScreen::Mode::Military && screen.shows(L::Names) && screen.shows(L::Paths));
    click(screen, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kEconomy)));
    NGS_CHECK(!screen.shows(L::Names) && screen.shows(L::Ranges));
    NGS_CHECK(screen.cancel() && !screen.overlayOpen());
    click(screen, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kEvents)));
    NGS_CHECK(screen.mode() == GalaxyScreen::Mode::Events && screen.chosenFleet() < 0);
    screen.draw(canvas, 0);
}

NGS_TEST("setting up and ending a trade route on the map") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    const EconomyData economy = loadEconomyData(*res);
    GameSettings settings;
    settings.empire = 1;
    Game g = newGame(settings, loadGalaxyData(*res, assets.lexicon()), economy, 3);
    g.fleets.clear();
    // A minor race nearby, a friend.
    const int sol = g.galaxy.homes[1];
    const Sector home = g.galaxy.systems[static_cast<std::size_t>(sol)].sector;
    int minor = -1;
    for (std::size_t i = 0; i < g.galaxy.systems.size(); ++i) {
        const StarSystem& s = g.galaxy.systems[i];
        if (s.race >= kEmpires && (minor < 0 || distance(s.sector, home) <
                                                     distance(g.galaxy.systems[static_cast<std::size_t>(minor)].sector, home))) {
            minor = static_cast<int>(i);
        }
    }
    NGS_CHECK(minor >= 0);
    const int race = g.galaxy.systems[static_cast<std::size_t>(minor)].race;
    g.relations[static_cast<std::size_t>(1) * g.races.size() + static_cast<std::size_t>(race)].treaty = Treaty::Friendship;
    GalaxyScreen screen(assets, g);
    Canvas canvas;
    screen.choose(home);
    screen.draw(canvas, 0);
    NGS_CHECK(screen.mode() == GalaxyScreen::Mode::Economy && screen.routeHeader() && screen.routesHere().empty());
    click(screen, centre(*screen.routeHeader()));
    NGS_CHECK(screen.choosingPartner());
    const Sector there = g.galaxy.systems[static_cast<std::size_t>(minor)].sector;
    screen.choose(home); // keeps the partner in view
    click(screen, centre(*screen.routeHeader()));
    const auto c = screen.sectorCentre(there);
    NGS_CHECK(c.has_value());
    if (c) {
        click(screen, *c);
    }
    NGS_CHECK(!screen.choosingPartner() && g.tradeRoutes.size() == 1 && g.tradeRoutes[0].to == minor);
    screen.draw(canvas, 0);
    // Clicking it ends it; in military mode the list has none.
    click(screen, centre(*screen.routeLine(0)));
    NGS_CHECK(g.tradeRoutes.empty());
    click(screen, centre(element(assets, "hmaingal.wdf", GalaxyScreen::kMilitary)));
    NGS_CHECK(!screen.routeHeader());
}

NGS_TEST("the research screen's databases: technologies, buildings and ships") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    auto res = std::make_shared<Resources>(*lib);
    Assets assets(res);
    const EconomyData economy = loadEconomyData(*res);
    GameSettings settings;
    settings.empire = 1;
    Game g = newGame(settings, loadGalaxyData(*res, assets.lexicon()), economy, 3);
    ResearchScreen screen(assets, g);
    Canvas canvas;
    screen.draw(canvas, 0);
    NGS_CHECK(screen.page() == ResearchScreen::Page::Management);

    // The technology field database: each level of a field, with its text.
    click(screen, centre(element(assets, "htech.wdf", ResearchScreen::kFieldDatabase)));
    NGS_CHECK(screen.page() == ResearchScreen::Page::Fields && screen.chosenField() == 0 && screen.entries() == 11);
    click(screen, centre(*screen.entryRect(2)));
    NGS_CHECK(screen.chosenEntry() == 2 && screen.entryTitle() == "Bionics");
    NGS_CHECK(screen.entryText().starts_with("BIONICS:"));
    screen.draw(canvas, 0);
    screen.command(Command::Next);
    NGS_CHECK(screen.chosenField() == 1 && screen.chosenEntry() == 0);

    // The object database: the Federation's buildings by kind, and ships.
    click(screen, centre(element(assets, "htech.wdf", ResearchScreen::kObjectDatabase)));
    NGS_CHECK(screen.page() == ResearchScreen::Page::Objects && screen.entries() > 5);
    screen.command(Command::Previous);
    NGS_CHECK(screen.chosenCategory() == ResearchScreen::kCategories - 1);
    for (const int ship : screen.categoryItems(screen.chosenCategory())) {
        NGS_CHECK(economy.ships[static_cast<std::size_t>(ship)].race == 1);
    }
    screen.command(Command::Down);
    screen.command(Command::Down);
    NGS_CHECK(screen.entryTitle().starts_with("Colony Ship") && screen.entryText().starts_with("Edward-class"));
    screen.draw(canvas, 0);
    click(screen, centre(screen.objectRect(ResearchScreen::kObjStatistics)));
    NGS_CHECK(screen.statistics() && screen.entryText().starts_with("Hull "));
    screen.draw(canvas, 0);
    // The ship turns as a 3D model; its buttons turn it another way or stop it.
    NGS_CHECK(screen.shipModelShown() && screen.spin() == ResearchScreen::Spin::Right);
    click(screen, centre(screen.objectRect(ResearchScreen::kTurnUp)));
    NGS_CHECK(screen.spin() == ResearchScreen::Spin::Up);
    click(screen, centre(screen.objectRect(ResearchScreen::kTurnUp)));
    NGS_CHECK(screen.spin() == ResearchScreen::Spin::None);
    screen.draw(canvas, 100);
    // The list's arrows page through it.
    if (screen.entries() > 12) {
        click(screen, centre(screen.objectRect(ResearchScreen::kObjListDown)));
        NGS_CHECK(screen.listFirst() > 0);
        click(screen, centre(screen.objectRect(ResearchScreen::kObjListUp)));
        NGS_CHECK(screen.listFirst() == 0);
    }
    // Every building is in one of the five other kinds, and has a text.
    std::size_t listed = 0;
    for (int c = 0; c + 1 < ResearchScreen::kCategories; ++c) {
        listed += screen.categoryItems(c).size();
    }
    NGS_CHECK(listed > 50);
    NGS_CHECK(!textAt(res->file("edifdesc.bst"), economy.buildings[0].description).empty());
    // Back to the shares.
    click(screen, centre(element(assets, "htech.wdf", ResearchScreen::kManagement)));
    NGS_CHECK(screen.page() == ResearchScreen::Page::Management);
}
