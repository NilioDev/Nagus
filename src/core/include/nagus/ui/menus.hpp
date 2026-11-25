// SPDX-License-Identifier: GPL-3.0-or-later
//
// The screens before a game: the main menu, the game set-up
// ("Configuration") and the choice of empire. See docs/rules/menus.md.
#pragma once

#include "nagus/settings.hpp"
#include "nagus/ui/screen.hpp"

#include <optional>

namespace nagus {

// What a screen asks the shell to do next.
enum class Outcome { None, NewGame, Continue, Back, Accept, Quit, Open, Save, Load, HallOfFame, Retire, Options, Combat };

class MenuScreen : public Screen {
public:
    Outcome takeOutcome() {
        const Outcome o = outcome_;
        outcome_ = Outcome::None;
        return o;
    }

protected:
    Outcome outcome_ = Outcome::None;
};

class MainMenu : public MenuScreen {
public:
    // Button numbers in the main menu's layout.
    static constexpr std::uint32_t kContinue = 0x8b;
    static constexpr std::uint32_t kNewSinglePlayer = 0x8c;
    static constexpr std::uint32_t kNewMultiPlayer = 0x8d;
    static constexpr std::uint32_t kLoad = 0x8e;
    static constexpr std::uint32_t kQuit = 0x8f;
    static constexpr std::uint32_t kHallOfFame = 0x90;
    static constexpr std::uint32_t kSave = 0x163;
    static constexpr std::uint32_t kOptions = 0x164;
    static constexpr std::uint32_t kRetire = 0x2580;

    // With a game running, Continue goes back to it.
    MainMenu(Assets& assets, bool gameRunning = false);
    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override {
        last_ = p;
        panel_.pointerMoved(p);
    }
    void pointerPressed(Point p) override { panel_.pointerPressed(p); }
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

private:
    Panel panel_;
    Point last_{};
};

class GameSetup : public MenuScreen {
public:
    static constexpr std::uint32_t kAccept = 0x23f1;
    static constexpr std::uint32_t kCancel = 0x23f2;

    GameSetup(Assets& assets, GameSettings settings);
    const GameSettings& settings() const { return settings_; }

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override { panel_.pointerPressed(p); }
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    // Clicks a button as the pointer would (for tests).
    void click(std::uint32_t id);

private:
    Assets& assets_;
    Panel panel_;
    GameSettings settings_;
    Point last_{};
};

class EmpireSelect : public MenuScreen {
public:
    static constexpr std::uint32_t kAccept = 0x2396;
    static constexpr std::uint32_t kCancel = 0x2397;

    EmpireSelect(Assets& assets, GameSettings settings);
    const GameSettings& settings() const { return settings_; }

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override { panel_.pointerPressed(p); }
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    void click(std::uint32_t id);

private:
    void choose(int empire);

    Assets& assets_;
    Panel panel_;
    GameSettings settings_;
    Point last_{};
};

} // namespace nagus
