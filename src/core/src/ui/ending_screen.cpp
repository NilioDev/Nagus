// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/ending_screen.hpp"

#include "nagus/data/resources.hpp"

#include <algorithm>
#include <sstream>

namespace nagus {

namespace {

constexpr std::size_t kMaxSteps = 200;
constexpr int kMaxWait = 60000;
constexpr std::uintmax_t kMaxVoice = 32u << 20;

} // namespace

std::vector<ScriptStep> readEndingScript(std::string_view text) {
    std::vector<ScriptStep> steps;
    std::istringstream in{std::string(text)};
    std::string line;
    while (std::getline(in, line) && steps.size() < kMaxSteps) {
        std::istringstream words(line);
        std::string word;
        std::string arg;
        words >> word >> arg;
        word = lower(word);
        ScriptStep s;
        if (word == "voiceover" && !arg.empty()) {
            s.kind = ScriptStep::Kind::Voice;
            s.name = lower(arg);
        } else if (word == "show" && !arg.empty()) {
            s.kind = ScriptStep::Kind::Show;
            s.name = lower(arg);
        } else if (word == "wait") {
            s.kind = ScriptStep::Kind::Wait;
            int hundredths = 0;
            try {
                hundredths = std::stoi(arg);
            } catch (const std::exception&) {
                continue;
            }
            s.time = std::clamp(hundredths * 10, 0, kMaxWait);
        } else if (word == "voicewait") {
            s.kind = ScriptStep::Kind::WaitForVoice;
        } else if (word == "end") {
            s.kind = ScriptStep::Kind::End;
        } else {
            continue;
        }
        steps.push_back(s);
    }
    return steps;
}

EndingScreen::EndingScreen(Assets& assets, Jukebox& jukebox, std::vector<ScriptStep> script)
    : assets_(assets), jukebox_(jukebox), script_(std::move(script)) {}

EndingScreen::~EndingScreen() { jukebox_.stopSounds(); }

void EndingScreen::finish() {
    if (!done_) {
        done_ = true;
        jukebox_.stopSounds();
        outcome_ = Outcome::Accept;
    }
}

void EndingScreen::run(std::uint64_t now) {
    while (!done_) {
        if (waitUntil_) {
            if (now < *waitUntil_) {
                return;
            }
            waitUntil_.reset();
        }
        if (waitingForVoice_) {
            if (jukebox_.soundsPlaying()) {
                return;
            }
            waitingForVoice_ = false;
        }
        if (next_ >= script_.size()) {
            finish();
            return;
        }
        const ScriptStep& s = script_[next_++];
        switch (s.kind) {
        case ScriptStep::Kind::Voice: {
            try {
                FileSource src(assets_.resources().loose(s.name));
                if (src.size() <= kMaxVoice) {
                    jukebox_.playSound(std::make_shared<const Pcm>(readWav(src.readAll(0, static_cast<std::size_t>(src.size())))));
                }
            } catch (const FormatError&) {
                // The pictures show without the speech.
            }
            break;
        }
        case ScriptStep::Kind::Show: picture_ = s.name; break;
        case ScriptStep::Kind::Wait: waitUntil_ = now + static_cast<std::uint64_t>(s.time); break;
        case ScriptStep::Kind::WaitForVoice: waitingForVoice_ = true; break;
        case ScriptStep::Kind::End: finish(); break;
        }
    }
}

void EndingScreen::draw(Canvas& canvas, std::uint64_t ms) {
    run(ms);
    canvas.clear();
    if (!picture_.empty()) {
        const Image& img = assets_.image(picture_);
        canvas.draw(img, (canvas.width() - img.width) / 2, (canvas.height() - img.height) / 2);
    }
}

void EndingScreen::pointerReleased(Point) { finish(); }

std::optional<Point> EndingScreen::command(Command) {
    finish();
    return std::nullopt;
}

} // namespace nagus
