// SPDX-License-Identifier: GPL-3.0-or-later
//
// The list of saved games, for saving the game being played or loading one.
// The screen only chooses a place; the shell reads and writes the files.
#pragma once

#include "nagus/game/save.hpp"
#include "nagus/ui/menus.hpp"

#include <array>

namespace nagus {

class LoadSaveScreen : public MenuScreen {
public:
    static constexpr std::uint32_t kAccept = 0x251f;
    static constexpr std::uint32_t kCancel = 0x2520;
    static constexpr std::uint32_t kTitle = 0x2521;
    // A slot's button, name and description.
    static constexpr std::uint32_t slotButton(int i) { return 0x2523 + 2 * static_cast<std::uint32_t>(i); }
    static constexpr std::uint32_t slotName(int i) { return 0x2522 + 2 * static_cast<std::uint32_t>(i); }
    static constexpr std::uint32_t slotText(int i) { return 0x253a + 2 * static_cast<std::uint32_t>(i); }

    enum class Mode { Save, Load };
    LoadSaveScreen(Assets& assets, Mode mode, std::array<SaveSlot, kSaveSlots> slots);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override { panel_.pointerPressed(p); }
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    Mode mode() const { return mode_; }
    // The chosen slot, or -1. Accept is the outcome once one is chosen.
    int chosen() const { return chosen_; }
    void choose(int slot);
    void click(std::uint32_t id);

private:
    bool canUse(int slot) const;

    Assets& assets_;
    Panel panel_;
    Mode mode_;
    std::array<SaveSlot, kSaveSlots> slots_;
    int chosen_ = -1;
    Point last_{};
};

} // namespace nagus
