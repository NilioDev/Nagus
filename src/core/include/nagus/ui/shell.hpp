// SPDX-License-Identifier: GPL-3.0-or-later
//
// Runs the screens: which one is shown, where the pointer is, and what
// happens when a screen is done. The app feeds it input and shows what it
// draws.
#pragma once

#include "nagus/settings.hpp"
#include "nagus/ui/menus.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace nagus {

class Shell {
public:
    Shell(std::shared_ptr<Resources> resources, std::filesystem::path settingsFile);

    void draw(Canvas& canvas, std::uint64_t ms);

    void pointerMoved(Point p);
    void pointerPressed(Point p);
    void pointerReleased(Point p);
    void command(Command c);
    // The gamepad's A button: a click where the pointer is.
    void clickAtPointer();

    Point pointer() const { return pointer_; }
    // The original's pointer is drawn while a mouse or gamepad is in use,
    // and hidden for touch.
    void setPointerVisible(bool visible) { pointerVisible_ = visible; }
    bool quit() const { return quit_; }

    enum class Showing { MainMenu, GameSetup, EmpireSelect };
    Showing showing() const { return showing_; }
    bool noticeOpen() const { return !notice_.empty(); }

private:
    void settle();
    void show(Showing s);
    MenuScreen& screen();

    Assets assets_;
    std::filesystem::path settingsFile_;
    GameSettings settings_;
    Showing showing_ = Showing::MainMenu;
    std::unique_ptr<MainMenu> mainMenu_;
    std::unique_ptr<GameSetup> setup_;
    std::unique_ptr<EmpireSelect> empires_;
    std::string notice_;
    Point pointer_{kScreenWidth / 2, kScreenHeight / 2};
    bool pointerVisible_ = true;
    bool quit_ = false;
};

} // namespace nagus
