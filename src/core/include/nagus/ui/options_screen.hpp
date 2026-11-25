// SPDX-License-Identifier: GPL-3.0-or-later
//
// The options (main menu): the volume of the music, the sound effects and
// the speech. The original has no screen layout for it, so this one is
// drawn over the main menu in the same colours.
#pragma once

#include "nagus/settings.hpp"
#include "nagus/sound/jukebox.hpp"
#include "nagus/ui/menus.hpp"

namespace nagus {

class OptionsScreen : public MenuScreen {
public:
    static constexpr int kRows = 3;
    static constexpr int kStep = 10;

    OptionsScreen(Assets& assets, Jukebox& jukebox, GameSettings settings);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override { pointer_ = p; }
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    const GameSettings& settings() const { return settings_; }
    // Where things are, for the pointer and tests.
    Rect bar(int row) const;
    Rect less(int row) const;
    Rect more(int row) const;
    Rect accept() const;
    int chosenRow() const { return row_; }

private:
    void change(int row, int volume);

    Assets& assets_;
    Jukebox& jukebox_;
    Panel background_;
    GameSettings settings_;
    int row_ = 0; // for the keys and gamepad; kRows is the Accept button
    Point pointer_{};
};

} // namespace nagus
