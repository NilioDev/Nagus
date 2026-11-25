// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/load_save_screen.hpp"

#include <cctype>

namespace nagus {

namespace {

constexpr const char* kFont = "human15.fnt";
constexpr const char* kTitleFont = "human28.fnt";
constexpr std::size_t kLoadGame = 608; // "Load Game"
constexpr std::size_t kEmpty = 276;    // "empty"
constexpr Pixel kText = 0xffffcc66u;
constexpr Pixel kDim = 0xff9098b0u;
constexpr Pixel kChosen = 0xffffffffu;

} // namespace

LoadSaveScreen::LoadSaveScreen(Assets& assets, Mode mode, std::array<SaveSlot, kSaveSlots> slots)
    : assets_(assets), panel_(assets, "loadsave.wdf"), mode_(mode), slots_(std::move(slots)) {
    for (int i = 0; i < kSaveSlots; ++i) {
        panel_.setEnabled(slotButton(i), canUse(i));
    }
    // Saving starts on the first free place; loading on the newest game.
    for (int i = 0; i < kSaveSlots && chosen_ < 0; ++i) {
        if (mode_ == Mode::Save ? !slots_[static_cast<std::size_t>(i)].used : canUse(i)) {
            chosen_ = i;
        }
    }
    if (mode_ == Mode::Load) {
        for (int i = 0; i < kSaveSlots; ++i) {
            const auto& s = slots_[static_cast<std::size_t>(i)];
            if (canUse(i) && chosen_ >= 0 && s.info.turn > slots_[static_cast<std::size_t>(chosen_)].info.turn) {
                chosen_ = i;
            }
        }
    }
    if (chosen_ < 0 && mode_ == Mode::Save) {
        chosen_ = 0;
    }
    choose(chosen_);
}

bool LoadSaveScreen::canUse(int slot) const {
    const auto& s = slots_[static_cast<std::size_t>(slot)];
    return mode_ == Mode::Save || (s.used && !s.damaged);
}

void LoadSaveScreen::choose(int slot) {
    if (slot >= 0 && slot < kSaveSlots && canUse(slot)) {
        chosen_ = slot;
    }
    for (int i = 0; i < kSaveSlots; ++i) {
        panel_.setOn(slotButton(i), i == chosen_);
    }
    panel_.setEnabled(kAccept, chosen_ >= 0);
}

void LoadSaveScreen::draw(Canvas& canvas, std::uint64_t) {
    panel_.draw(canvas);
    const Lexicon& lex = assets_.lexicon();
    std::string title = mode_ == Mode::Load ? lex[kLoadGame] : std::string("Save Game");
    for (char& ch : title) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    canvas.text(assets_.font(kTitleFont), title, panel_.rect(kTitle), kText, Align::Center);
    const Font& font = assets_.font(kFont);
    for (int i = 0; i < kSaveSlots; ++i) {
        const auto& s = slots_[static_cast<std::size_t>(i)];
        const Pixel colour = i == chosen_ ? kChosen : (canUse(i) ? kText : kDim);
        const Rect name = panel_.rect(slotName(i));
        const Rect text = panel_.rect(slotText(i));
        if (!s.used) {
            canvas.text(font, lex[kEmpty], {name.x + 4, name.y, name.w - 8, name.h}, kDim, Align::Left);
        } else if (s.damaged) {
            canvas.text(font, "?", {name.x + 4, name.y, name.w - 8, name.h}, kDim, Align::Left);
            canvas.text(font, "Cannot be read", {text.x + 4, text.y, text.w - 8, text.h}, kDim, Align::Left);
        } else {
            canvas.text(font, s.info.name, {name.x + 4, name.y, name.w - 8, name.h}, colour, Align::Left);
            canvas.text(font, s.info.description, {text.x + 4, text.y, text.w - 8, text.h}, colour, Align::Left);
        }
    }
}

void LoadSaveScreen::pointerMoved(Point p) {
    last_ = p;
    panel_.pointerMoved(p);
}

void LoadSaveScreen::click(std::uint32_t id) {
    if (id == kCancel) {
        outcome_ = Outcome::Back;
    } else if (id == kAccept) {
        if (chosen_ >= 0) {
            outcome_ = Outcome::Accept;
        }
    } else {
        for (int i = 0; i < kSaveSlots; ++i) {
            if (id == slotButton(i)) {
                // A click on the chosen place again takes it.
                if (i == chosen_) {
                    outcome_ = Outcome::Accept;
                }
                choose(i);
            }
        }
    }
}

void LoadSaveScreen::pointerReleased(Point p) {
    last_ = p;
    if (const auto id = panel_.pointerReleased(p)) {
        click(*id);
    }
}

std::optional<Point> LoadSaveScreen::command(Command c) {
    if (c == Command::Back) {
        outcome_ = Outcome::Back;
        return std::nullopt;
    }
    if (c == Command::Start) {
        click(kAccept);
        return std::nullopt;
    }
    return panel_.step(c, last_);
}

} // namespace nagus
