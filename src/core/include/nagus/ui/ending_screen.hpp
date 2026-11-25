// SPDX-License-Identifier: GPL-3.0-or-later
//
// The end of a game: the original's pictures and speech for the player's
// empire and the way the game ended, run from its little script
// (<x>eog<kind>.scr). Any click, key or button ends it early.
#pragma once

#include "nagus/sound/jukebox.hpp"
#include "nagus/ui/menus.hpp"

namespace nagus {

struct ScriptStep {
    enum class Kind { Voice, Show, Wait, WaitForVoice, End };
    Kind kind = Kind::End;
    std::string name; // the sound or picture
    int time = 0;     // milliseconds to wait
};

// Reads a script: VOICEOVER file, SHOW picture, WAIT hundredths,
// VOICEWAIT, END. Unknown lines are passed over; at most 200 steps.
std::vector<ScriptStep> readEndingScript(std::string_view text);

class EndingScreen : public MenuScreen {
public:
    EndingScreen(Assets& assets, Jukebox& jukebox, std::vector<ScriptStep> script);
    ~EndingScreen() override;

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    const std::string& picture() const { return picture_; }
    bool done() const { return done_; }

private:
    void run(std::uint64_t now);
    void finish();

    Assets& assets_;
    Jukebox& jukebox_;
    std::vector<ScriptStep> script_;
    std::size_t next_ = 0;
    std::optional<std::uint64_t> waitUntil_;
    bool waitingForVoice_ = false;
    std::string picture_;
    bool done_ = false;
};

} // namespace nagus
