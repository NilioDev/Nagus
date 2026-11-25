// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/chat_screen.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr char kPrefixes[kEmpires] = {'c', 'h', 'f', 'k', 'r'};
constexpr Pixel kEmpireColours[kEmpires] = {0xffe0c040u, 0xff60a0ffu, 0xffffa040u, 0xffff5050u, 0xff40e070u};
constexpr Pixel kText = 0xffffcc66u;
constexpr const char* kFont = "human15.fnt";

} // namespace

ChatWindow::ChatWindow(Assets& assets, int player, const std::array<bool, kEmpires>& humans,
                       const std::array<std::string, kEmpires>& names, std::array<bool, kEmpires>& ignored)
    : assets_(assets), panel_(assets, std::string(1, kPrefixes[std::clamp(player, 0, kEmpires - 1)]) + "_chat.wdf"),
      player_(std::clamp(player, 0, kEmpires - 1)), humans_(humans), names_(names), ignored_(ignored) {
    const Rect r = panel_.layout().rect;
    panel_.setOrigin({(kScreenWidth - r.w) / 2, (kScreenHeight - r.h) / 2});
    refresh();
}

int ChatWindow::empireAt(int place) const {
    // The other empires in their order.
    int e = place;
    if (e >= player_) {
        ++e;
    }
    return e;
}

void ChatWindow::refresh() {
    for (int i = 0; i < 4; ++i) {
        const auto e = static_cast<std::size_t>(empireAt(i));
        panel_.setEnabled(kIgnore[i], humans_[e]);
        panel_.setOn(kIgnore[i], ignored_[e]);
    }
    panel_.setEnabled(kSend, !line_.empty());
}

void ChatWindow::draw(Canvas& canvas, const std::deque<ChatLine>& log) {
    refresh();
    panel_.draw(canvas);
    const Font& font = assets_.font(kFont);
    for (int i = 0; i < 4; ++i) {
        const int e = empireAt(i);
        const auto ue = static_cast<std::size_t>(e);
        // The empire's picture, lit while a player has it.
        const Image& pic = assets_.image("ch_" + std::string(1, kPrefixes[e]) + "icn" + (humans_[ue] ? "2" : "1") + ".tga");
        const Rect r = panel_.rect(kPictures[i]);
        canvas.draw(pic, r.x, r.y);
    }
    // The line being typed, with a cursor.
    const Rect in = panel_.rect(kLine);
    const auto typed = wrapText(font, line_ + "_", in.w - 8);
    const int lineHeight = font.height() + 2;
    const int fitIn = std::max(1, (in.h - 4) / lineHeight);
    for (std::size_t i = typed.size() > static_cast<std::size_t>(fitIn) ? typed.size() - static_cast<std::size_t>(fitIn) : 0, y = 0;
         i < typed.size(); ++i, ++y) {
        canvas.text(font, typed[i], in.x + 4, in.y + 2 + static_cast<int>(y) * lineHeight, kText);
    }
    // What was said, the newest at the bottom.
    struct Shown {
        std::string text;
        Pixel colour;
    };
    std::vector<Shown> lines;
    const Rect box = panel_.rect(kLog);
    for (const auto& l : log) {
        const Pixel c = l.empire >= 0 && l.empire < kEmpires ? kEmpireColours[l.empire] : kText;
        for (auto& w : wrapText(font, l.name + ": " + l.text, box.w - 8)) {
            lines.push_back({std::move(w), c});
        }
    }
    shownLines_ = std::max(1, box.h / lineHeight);
    const int most = std::max(0, static_cast<int>(lines.size()) - shownLines_);
    scroll_ = std::clamp(scroll_, 0, most);
    panel_.setEnabled(kUp, scroll_ < most);
    panel_.setEnabled(kDown, scroll_ > 0);
    const int last = static_cast<int>(lines.size()) - scroll_;
    const int first = std::max(0, last - shownLines_);
    for (int i = first; i < last; ++i) {
        const auto& s = lines[static_cast<std::size_t>(i)];
        canvas.text(font, s.text, box.x + 4, box.y + (i - first) * lineHeight, s.colour);
    }
}

ChatWindow::Result ChatWindow::pointerReleased(Point p) {
    const auto id = panel_.pointerReleased(p);
    if (!id) {
        return Result::None;
    }
    assets_.clicked();
    if (*id == kClose) {
        return Result::Close;
    }
    if (*id == kSend) {
        return line_.empty() ? Result::None : Result::Send;
    }
    if (*id == kUp) {
        scroll_ += 3;
    } else if (*id == kDown) {
        scroll_ = std::max(0, scroll_ - 3);
    }
    for (int i = 0; i < 4; ++i) {
        if (*id == kIgnore[i]) {
            const auto e = static_cast<std::size_t>(empireAt(i));
            ignored_[e] = !ignored_[e];
        }
    }
    refresh();
    return Result::None;
}

ChatWindow::Result ChatWindow::command(Command c, Point& pointer) {
    switch (c) {
    case Command::Back: return Result::Close;
    case Command::Start: return line_.empty() ? Result::None : Result::Send;
    case Command::Up: scroll_ += 1; return Result::None;
    case Command::Down: scroll_ = std::max(0, scroll_ - 1); return Result::None;
    case Command::Left:
    case Command::Right:
        if (const auto jump = panel_.step(c, pointer)) {
            pointer = *jump;
            panel_.pointerMoved(pointer);
        }
        return Result::None;
    default: return Result::None;
    }
}

ChatWindow::Result ChatWindow::typed(std::string_view text) {
    Result result = Result::None;
    for (const char c : text) {
        if ((c == '\n' || c == '\r') && !line_.empty()) {
            result = Result::Send;
        } else if (c == '\b') {
            if (!line_.empty()) {
                line_.pop_back();
            }
        } else if (c >= 32 && c < 127 && line_.size() < kMaxChat) {
            line_ += c;
        }
    }
    refresh();
    return result;
}

std::string ChatWindow::takeLine() {
    std::string out;
    out.swap(line_);
    scroll_ = 0;
    refresh();
    return out;
}

} // namespace nagus
