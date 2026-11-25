// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/options_screen.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr Rect kBox{180, 170, 440, 270};
constexpr Pixel kOrange = 0xffff9900u;
constexpr Pixel kText = 0xffffcc66u;
constexpr Pixel kBarEmpty = 0xff303848u;
constexpr Pixel kChosen = 0xffffffffu;
// Lexicon numbers.
constexpr std::size_t kVolumeControl = 1416;
constexpr std::size_t kNames[OptionsScreen::kRows] = {797, 1113, 1413}; // MUSIC, SFX, VOICE
constexpr std::size_t kAccept = 1;
// A sound effect to hear the new volume by.
constexpr int kTestSound = 76;

} // namespace

OptionsScreen::OptionsScreen(Assets& assets, Jukebox& jukebox, GameSettings settings)
    : assets_(assets), jukebox_(jukebox), background_(assets, "opening.wdf"), settings_(settings) {
    for (const auto& e : background_.layout().elements) {
        background_.setEnabled(e.id, false);
    }
}

Rect OptionsScreen::bar(int row) const { return {kBox.x + 170, kBox.y + 70 + row * 50, 200, 24}; }
Rect OptionsScreen::less(int row) const { return {kBox.x + 140, kBox.y + 70 + row * 50, 24, 24}; }
Rect OptionsScreen::more(int row) const { return {kBox.x + 376, kBox.y + 70 + row * 50, 24, 24}; }
Rect OptionsScreen::accept() const { return {kBox.x + kBox.w / 2 - 70, kBox.y + kBox.h - 50, 140, 30}; }

void OptionsScreen::change(int row, int volume) {
    settings_.volume[static_cast<std::size_t>(row)] = std::clamp(volume, 0, 100);
    jukebox_.setVolumes(settings_.volume[0], settings_.volume[1], settings_.volume[2]);
    if (row == 1) {
        jukebox_.playEffect(kTestSound + std::clamp(settings_.empire, 0, kEmpires - 1));
    }
}

void OptionsScreen::draw(Canvas& canvas, std::uint64_t) {
    background_.draw(canvas);
    canvas.blend({0, 0, canvas.width(), canvas.height()}, 0xff000000u, 160);
    canvas.fill(kBox, 0xff000000u);
    canvas.frame(kBox, kOrange, 3);
    const Lexicon& lex = assets_.lexicon();
    const Font& big = assets_.font("human20.fnt");
    canvas.text(big, lex[kVolumeControl], {kBox.x, kBox.y + 20, kBox.w, 30}, kOrange, Align::Center);
    for (int r = 0; r < kRows; ++r) {
        const Pixel colour = r == row_ ? kChosen : kText;
        const Rect b = bar(r);
        canvas.text(big, lex[kNames[r]], {kBox.x + 20, b.y, 110, b.h}, colour, Align::Left);
        canvas.fill(less(r), kOrange);
        canvas.fill(more(r), kOrange);
        canvas.text(big, "-", less(r), 0xff000000u, Align::Center);
        canvas.text(big, "+", more(r), 0xff000000u, Align::Center);
        const int v = settings_.volume[static_cast<std::size_t>(r)];
        canvas.fill(b, kBarEmpty);
        canvas.fill({b.x, b.y, b.w * v / 100, b.h}, kOrange);
        if (r == row_) {
            canvas.frame(b, kChosen, 1);
        }
    }
    const Rect a = accept();
    canvas.fill(a, row_ == kRows ? kChosen : kOrange);
    canvas.text(big, lex[kAccept], a, 0xff000000u, Align::Center);
}

void OptionsScreen::pointerReleased(Point p) {
    pointer_ = p;
    if (accept().contains(p.x, p.y)) {
        assets_.clicked();
        outcome_ = Outcome::Accept;
        return;
    }
    for (int r = 0; r < kRows; ++r) {
        const int v = settings_.volume[static_cast<std::size_t>(r)];
        if (less(r).contains(p.x, p.y)) {
            row_ = r;
            change(r, v - kStep);
        } else if (more(r).contains(p.x, p.y)) {
            row_ = r;
            change(r, v + kStep);
        } else if (const Rect b = bar(r); b.contains(p.x, p.y)) {
            row_ = r;
            // Rounded to the step.
            change(r, ((p.x - b.x) * 100 / b.w + kStep / 2) / kStep * kStep);
        }
    }
}

std::optional<Point> OptionsScreen::command(Command c) {
    switch (c) {
    case Command::Back:
    case Command::Start: outcome_ = Outcome::Accept; return std::nullopt;
    case Command::Up: row_ = std::max(0, row_ - 1); break;
    case Command::Down: row_ = std::min(kRows, row_ + 1); break;
    case Command::Left:
    case Command::Right:
        if (row_ < kRows) {
            change(row_, settings_.volume[static_cast<std::size_t>(row_)] + (c == Command::Right ? kStep : -kStep));
        }
        break;
    default: return std::nullopt;
    }
    // The pointer follows the chosen row, so A (a click) acts on it.
    const Rect r = row_ < kRows ? bar(row_) : accept();
    if (row_ < kRows) {
        const int v = settings_.volume[static_cast<std::size_t>(row_)];
        return Point{r.x + r.w * v / 100, r.centerY()};
    }
    return Point{r.centerX(), r.centerY()};
}

} // namespace nagus
