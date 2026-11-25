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
    click(shell, centre(element(assets, "emppick.wdf", 0x2394)));
    shell.command(Command::Start);
    NGS_CHECK(loadSettings(settings).empire == 3);
    NGS_CHECK(shell.noticeOpen());
    shell.draw(canvas, 0);
    shell.command(Command::Back);
    NGS_CHECK(!shell.noticeOpen());
    NGS_CHECK(shell.showing() == Shell::Showing::MainMenu);
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
