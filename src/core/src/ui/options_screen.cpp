// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/options_screen.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr Pixel kText = 0xffffcc66u;
constexpr Pixel kDim = 0xff707880u;
constexpr Pixel kChosen = 0xffffffffu;
// Lexicon numbers.
constexpr std::size_t kVolumeControl = 1416;
constexpr std::size_t kNames[3] = {797, 1113, 1413}; // MUSIC, SFX, VOICE
constexpr std::size_t kOn = 882;
constexpr std::size_t kOff = 880;
constexpr std::size_t kAutomatic = 52;
constexpr std::size_t kManual = 621;
constexpr std::size_t kTimers[2] = {1182, 1247}; // STRATEGIC TIMER, TACTICAL TIMER
// A sound effect to hear the new volume by.
constexpr int kTestSound = 76;

} // namespace

OptionsScreen::OptionsScreen(Assets& assets, Jukebox& jukebox, GameSettings settings)
    : assets_(assets), jukebox_(jukebox), panel_(assets, "options.wdf"), settings_(settings), before_(settings) {
    // The timers are for multiplayer games with a time limit, which Nagus
    // does not have.
    for (const auto id : kTimerArrows) {
        panel_.setEnabled(id, false);
    }
    refresh();
}

void OptionsScreen::refresh() {
    panel_.setOn(kTooltips, settings_.tooltips);
    panel_.setOn(kTactical, !settings_.automaticCombat);
    panel_.setOn(kEvents, settings_.randomEvents);
    panel_.setOn(kAutosave, settings_.autosave);
}

Rect OptionsScreen::bar(int row) const { return panel_.rect(kBars[row]); }

std::optional<Rect> OptionsScreen::button(std::uint32_t id) const {
    if (!panel_.usable(id)) {
        return std::nullopt;
    }
    return panel_.rect(id);
}

std::uint32_t OptionsScreen::rowElement(int row) const {
    if (row < kVolumes) {
        return kBars[row];
    }
    constexpr std::uint32_t kSwitches[4] = {kTooltips, kTactical, kEvents, kAutosave};
    return row < kVolumes + 4 ? kSwitches[row - kVolumes] : kAccept;
}

void OptionsScreen::change(int row, int volume) {
    settings_.volume[static_cast<std::size_t>(row)] = std::clamp(volume, 0, 100);
    jukebox_.setVolumes(settings_.heard(0), settings_.heard(1), settings_.heard(2));
    if (row == 1 || row == 3) {
        jukebox_.playEffect(kTestSound + std::clamp(settings_.empire, 0, kEmpires - 1));
    }
}

void OptionsScreen::toggle(std::uint32_t id) {
    if (id == kTooltips) {
        settings_.tooltips = !settings_.tooltips;
    } else if (id == kTactical) {
        settings_.automaticCombat = !settings_.automaticCombat;
    } else if (id == kEvents) {
        settings_.randomEvents = !settings_.randomEvents;
    } else if (id == kAutosave) {
        settings_.autosave = !settings_.autosave;
    }
    refresh();
}

void OptionsScreen::field(Canvas& canvas, std::uint32_t id, const std::string& text, bool dim) {
    const auto* e = panel_.layout().find(id);
    if (e == nullptr) {
        return;
    }
    const Font& font = assets_.font(e->font.empty() ? std::string("human15.fnt") : e->font);
    canvas.text(font, text, panel_.rect(id), dim ? kDim : kText, Align::Center);
}

void OptionsScreen::draw(Canvas& canvas, std::uint64_t) {
    panel_.draw(canvas);
    const Lexicon& lex = assets_.lexicon();
    field(canvas, kTitle, lex[kVolumeControl]);
    for (int r = 0; r < kVolumes; ++r) {
        field(canvas, kBarNames[r], r < 3 ? lex[kNames[r]] : "ALL");
        const Rect b = bar(r);
        const Image& full = assets_.image("vol-bar.tga");
        const int v = settings_.volume[static_cast<std::size_t>(r)];
        canvas.draw(full, {0, 0, std::min(full.width, b.w) * v / 100, full.height}, b.x, b.y);
        if (r == row_) {
            canvas.frame({b.x - 2, b.y - 2, b.w + 4, b.h + 4}, kChosen, 1);
        }
    }
    field(canvas, kValues[0], lex[settings_.tooltips ? kOn : kOff]);
    field(canvas, kValues[1], lex[settings_.automaticCombat ? kAutomatic : kManual]);
    field(canvas, kValues[2], lex[settings_.randomEvents ? kOn : kOff]);
    field(canvas, kValues[3], lex[settings_.autosave ? kOn : kOff]);
    for (int t = 0; t < 2; ++t) {
        field(canvas, kTimerNames[t], lex[kTimers[t]], true);
        field(canvas, kTimerValues[t], lex[kOff], true);
    }
    if (row_ >= kVolumes) {
        const Rect r = panel_.rect(rowElement(row_));
        canvas.frame({r.x - 2, r.y - 2, r.w + 4, r.h + 4}, kChosen, 1);
    }
}

void OptionsScreen::pointerMoved(Point p) {
    pointer_ = p;
    panel_.pointerMoved(p);
}

void OptionsScreen::pointerPressed(Point p) {
    pointer_ = p;
    panel_.pointerPressed(p);
}

void OptionsScreen::pointerReleased(Point p) {
    pointer_ = p;
    if (const auto id = panel_.pointerReleased(p)) {
        if (*id == kAccept) {
            outcome_ = Outcome::Accept;
        } else if (*id == kCancel) {
            jukebox_.setVolumes(before_.heard(0), before_.heard(1), before_.heard(2));
            outcome_ = Outcome::Back;
        } else {
            toggle(*id);
            for (int r = kVolumes; r < kRows; ++r) {
                row_ = rowElement(r) == *id ? r : row_;
            }
        }
        return;
    }
    for (int r = 0; r < kVolumes; ++r) {
        const Rect b = bar(r);
        if (b.contains(p.x, p.y)) {
            row_ = r;
            assets_.clicked();
            // Rounded to the step.
            change(r, ((p.x - b.x) * 100 / b.w + kStep / 2) / kStep * kStep);
        }
    }
}

std::optional<Point> OptionsScreen::command(Command c) {
    switch (c) {
    case Command::Back:
        jukebox_.setVolumes(before_.heard(0), before_.heard(1), before_.heard(2));
        outcome_ = Outcome::Back;
        return std::nullopt;
    case Command::Start: outcome_ = Outcome::Accept; return std::nullopt;
    case Command::Up: row_ = std::max(0, row_ - 1); break;
    case Command::Down: row_ = std::min(kRows - 1, row_ + 1); break;
    case Command::Left:
    case Command::Right:
        if (row_ < kVolumes) {
            change(row_, settings_.volume[static_cast<std::size_t>(row_)] + (c == Command::Right ? kStep : -kStep));
        } else if (row_ < kRows - 1) {
            toggle(rowElement(row_));
        }
        break;
    default: return std::nullopt;
    }
    // The pointer follows the chosen row, so A (a click) acts on it.
    const Rect r = panel_.rect(rowElement(row_));
    if (row_ < kVolumes) {
        const int v = settings_.volume[static_cast<std::size_t>(row_)];
        return Point{r.x + std::max(1, r.w * v / 100) - 1, r.centerY()};
    }
    return Point{r.centerX(), r.centerY()};
}

} // namespace nagus
