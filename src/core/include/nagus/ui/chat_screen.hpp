// SPDX-License-Identifier: GPL-3.0-or-later
//
// The chat window of a multiplayer game: what the players said, a line to
// type, and a switch per empire to ignore what that player says. See
// docs/rules/multiplayer.md.
#pragma once

#include "nagus/net/session.hpp"
#include "nagus/ui/screen.hpp"

#include <array>
#include <deque>

namespace nagus {

class ChatWindow {
public:
    static constexpr std::uint32_t kClose = 0x63;
    static constexpr std::uint32_t kSend = 0x1ad;
    // The other four empires' pictures and their Ignore switches.
    static constexpr std::uint32_t kPictures[4] = {0x1ae, 0x1af, 0x1b0, 0x1b1};
    static constexpr std::uint32_t kIgnore[4] = {0x1b2, 0x1b3, 0x1b4, 0x1b5};
    static constexpr std::uint32_t kUp = 0x1b6;
    static constexpr std::uint32_t kDown = 0x1b7;
    static constexpr std::uint32_t kLine = 0x62;
    static constexpr std::uint32_t kLog = 0x61;

    enum class Result { None, Send, Close };

    // player: this player's empire; humans: which empires players have.
    ChatWindow(Assets& assets, int player, const std::array<bool, kEmpires>& humans,
               const std::array<std::string, kEmpires>& names, std::array<bool, kEmpires>& ignored);

    void draw(Canvas& canvas, const std::deque<ChatLine>& log);
    void pointerMoved(Point p) { panel_.pointerMoved(p); }
    void pointerPressed(Point p) { panel_.pointerPressed(p); }
    Result pointerReleased(Point p);
    Result command(Command c, Point& pointer);
    // Enter sends the line.
    Result typed(std::string_view text);
    // The line typed; taken once it is sent.
    std::string takeLine();
    const std::string& line() const { return line_; }
    // The empire shown in one of the four places.
    int empireAt(int place) const;
    Rect rect(std::uint32_t id) const { return panel_.rect(id); }
    bool usable(std::uint32_t id) const { return panel_.usable(id); }

private:
    void refresh();

    Assets& assets_;
    Panel panel_;
    int player_;
    std::array<bool, kEmpires> humans_;
    std::array<std::string, kEmpires> names_;
    std::array<bool, kEmpires>& ignored_;
    std::string line_;
    // Lines scrolled back from the newest.
    int scroll_ = 0;
    int shownLines_ = 0;
};

} // namespace nagus
