// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/settings.hpp"
#include "nagus/ui/shell.hpp"

#include <filesystem>

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
    click(shell, centre(element(assets, "opening.wdf", MainMenu::kLoad)));
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

    // Back opens the main menu, where Continue goes back to the game.
    shell.command(Command::Back);
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

    // Back on the map, the turn ends with the Turn button.
    shell.command(Command::Back);
    NGS_CHECK(shell.showing() == Shell::Showing::Galaxy);
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
    shell.pointerMoved({10, 10});
    shell.command(Command::Down); // to the nearest usable button
    shell.command(Command::Down);
    const Point p = shell.pointer();
    NGS_CHECK(newGame.contains(p.x, p.y) || quit.contains(p.x, p.y));
    // Move to New Single Player Game and press A.
    shell.pointerMoved(centre(quit));
    shell.command(Command::Up);
    NGS_CHECK(newGame.contains(shell.pointer().x, shell.pointer().y));
    shell.clickAtPointer();
    NGS_CHECK(shell.showing() == Shell::Showing::GameSetup);
    shell.command(Command::Back);
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);
}
