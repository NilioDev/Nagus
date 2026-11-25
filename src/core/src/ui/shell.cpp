// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/shell.hpp"

namespace nagus {

namespace {

constexpr const char* kPointer = "generic.cur";
constexpr const char* kNoticeFont = "human20.fnt";

// Our own words: there is no game behind the menus yet.
constexpr const char* kNotYet =
    "Your empire is chosen. Nagus cannot make the galaxy and play the game yet: "
    "that comes in a later version. Click or press a button to go back to the main menu.";

} // namespace

Shell::Shell(std::shared_ptr<Resources> resources, std::filesystem::path settingsFile)
    : assets_(std::move(resources)), settingsFile_(std::move(settingsFile)) {
    settings_ = loadSettings(settingsFile_);
    show(Showing::MainMenu);
}

void Shell::show(Showing s) {
    showing_ = s;
    mainMenu_.reset();
    setup_.reset();
    empires_.reset();
    switch (s) {
    case Showing::MainMenu: mainMenu_ = std::make_unique<MainMenu>(assets_); break;
    case Showing::GameSetup: setup_ = std::make_unique<GameSetup>(assets_, settings_); break;
    case Showing::EmpireSelect: empires_ = std::make_unique<EmpireSelect>(assets_, settings_); break;
    }
    screen().pointerMoved(pointer_);
}

MenuScreen& Shell::screen() {
    switch (showing_) {
    case Showing::GameSetup: return *setup_;
    case Showing::EmpireSelect: return *empires_;
    case Showing::MainMenu: break;
    }
    return *mainMenu_;
}

void Shell::settle() {
    const Outcome o = screen().takeOutcome();
    switch (showing_) {
    case Showing::MainMenu:
        if (o == Outcome::NewGame) {
            show(Showing::GameSetup);
        } else if (o == Outcome::Quit) {
            quit_ = true;
        }
        break;
    case Showing::GameSetup:
        if (o == Outcome::Accept) {
            settings_ = setup_->settings();
            saveSettings(settingsFile_, settings_);
            show(Showing::EmpireSelect);
        } else if (o == Outcome::Back) {
            show(Showing::MainMenu);
        }
        break;
    case Showing::EmpireSelect:
        if (o == Outcome::Accept) {
            settings_ = empires_->settings();
            saveSettings(settingsFile_, settings_);
            notice_ = kNotYet;
        } else if (o == Outcome::Back) {
            show(Showing::GameSetup);
        }
        break;
    }
}

void Shell::draw(Canvas& canvas, std::uint64_t ms) {
    canvas.clear();
    screen().draw(canvas, ms);
    if (!notice_.empty()) {
        canvas.blend({0, 0, canvas.width(), canvas.height()}, rgb(0, 0, 0), 160);
        const Rect box{150, 220, 500, 160};
        canvas.fill(box, rgb(0, 0, 0));
        canvas.frame(box, rgb(255, 153, 0), 3);
        drawParagraph(canvas, assets_.font(kNoticeFont), notice_, {box.x + 20, box.y + 20, box.w - 40, box.h - 40},
                      rgb(255, 204, 102));
    }
    if (pointerVisible_) {
        const Cursor& c = assets_.cursor(kPointer);
        canvas.draw(c.frame(ms), pointer_.x - c.hotX, pointer_.y - c.hotY);
    }
}

void Shell::pointerMoved(Point p) {
    pointer_ = p;
    if (notice_.empty()) {
        screen().pointerMoved(p);
    }
}

void Shell::pointerPressed(Point p) {
    pointer_ = p;
    if (notice_.empty()) {
        screen().pointerPressed(p);
    }
}

void Shell::pointerReleased(Point p) {
    pointer_ = p;
    if (!notice_.empty()) {
        notice_.clear();
        show(Showing::MainMenu);
        return;
    }
    screen().pointerReleased(p);
    settle();
}

void Shell::clickAtPointer() {
    pointerPressed(pointer_);
    pointerReleased(pointer_);
}

void Shell::command(Command c) {
    if (!notice_.empty()) {
        notice_.clear();
        show(Showing::MainMenu);
        return;
    }
    if (const auto jump = screen().command(c)) {
        pointer_ = *jump;
        screen().pointerMoved(pointer_);
    }
    settle();
}

} // namespace nagus
