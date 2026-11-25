// SPDX-License-Identifier: GPL-3.0-or-later
//
// The options (main menu), on the original's options.wdf: the volumes,
// tactical combat, random events and saving each turn.
#pragma once

#include "nagus/settings.hpp"
#include "nagus/sound/jukebox.hpp"
#include "nagus/ui/menus.hpp"

namespace nagus {

class OptionsScreen : public MenuScreen {
public:
    static constexpr std::uint32_t kAccept = 0x1fa4;
    static constexpr std::uint32_t kCancel = 0x1fa5;
    static constexpr std::uint32_t kTitle = 0x1fba;
    // The volume bars and their names: music, sound effects, speech, all.
    static constexpr std::uint32_t kBars[4] = {0x1fa6, 0x1fa7, 0x1fa8, 0x1fa9};
    static constexpr std::uint32_t kBarNames[4] = {0x1fbb, 0x1fbc, 0x1fbd, 0x1fbe};
    // The switches and their values.
    static constexpr std::uint32_t kTooltips = 0x1fab;
    static constexpr std::uint32_t kTactical = 0x1faa;
    static constexpr std::uint32_t kEvents = 0x1fac;
    static constexpr std::uint32_t kAutosave = 0x1fad;
    static constexpr std::uint32_t kValues[4] = {0x1fe0, 0x1fe1, 0x1fe2, 0x1fe3};
    // The multiplayer timers: names, values and arrows.
    static constexpr std::uint32_t kTimerNames[2] = {0x1fb4, 0x1fb8};
    static constexpr std::uint32_t kTimerValues[2] = {0x1fb5, 0x1fb9};
    static constexpr std::uint32_t kTimerArrows[4] = {0x1fb3, 0x1fb7, 0x1fb2, 0x1fb6};

    static constexpr int kVolumes = 4;
    static constexpr int kStep = 10;
    // For the keys and gamepad: the bars, then the four switches, then
    // Accept.
    static constexpr int kRows = kVolumes + 5;

    OptionsScreen(Assets& assets, Jukebox& jukebox, GameSettings settings);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    const GameSettings& settings() const { return settings_; }
    Rect bar(int row) const;
    std::optional<Rect> button(std::uint32_t id) const;
    int chosenRow() const { return row_; }

private:
    void change(int row, int volume);
    void toggle(std::uint32_t id);
    void refresh();
    std::uint32_t rowElement(int row) const;
    void field(Canvas& canvas, std::uint32_t id, const std::string& text, bool dim = false);

    Assets& assets_;
    Jukebox& jukebox_;
    Panel panel_;
    GameSettings settings_;
    GameSettings before_;
    int row_ = 0;
    Point pointer_{};
};

} // namespace nagus
