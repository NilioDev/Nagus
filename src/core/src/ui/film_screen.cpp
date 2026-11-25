// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/film_screen.hpp"

#include "nagus/data/resources.hpp"

#include <fstream>

namespace nagus {

namespace {

constexpr std::uintmax_t kMaxSoundFile = 64u << 20;

std::shared_ptr<Source> openFile(const Resources& res, const std::string& name) {
    return std::make_shared<FileSource>(res.loose(name));
}

} // namespace

FilmScreen::FilmScreen(const Resources& resources, Jukebox& jukebox, const std::string& film, const std::string& soundFile)
    : jukebox_(jukebox), avi_(openFile(resources, film)), decoder_(avi_.width(), avi_.height()) {
    if (!soundFile.empty()) {
        try {
            auto src = openFile(resources, soundFile);
            if (src->size() <= kMaxSoundFile) {
                sound_ = std::make_shared<const Pcm>(readWav(src->readAll(0, static_cast<std::size_t>(src->size()))));
            }
        } catch (const FormatError&) {
            // The film plays without its sound.
        }
    } else if (avi_.sound().frames() > 0) {
        sound_ = std::make_shared<const Pcm>(avi_.sound());
    }
}

FilmScreen::~FilmScreen() { jukebox_.stopSounds(); }

void FilmScreen::finish() {
    if (!done_) {
        done_ = true;
        jukebox_.stopSounds();
        outcome_ = Outcome::Accept;
    }
}

void FilmScreen::draw(Canvas& canvas, std::uint64_t ms) {
    if (!start_) {
        start_ = ms;
        jukebox_.stopSounds();
        jukebox_.playSound(sound_);
    }
    const std::uint64_t elapsed = ms - *start_;
    const auto wanted = static_cast<std::int64_t>(elapsed * 1000 / avi_.frameTime());
    if (wanted >= static_cast<std::int64_t>(avi_.frames())) {
        finish();
    }
    // Every frame builds on the one before, so none is passed over.
    while (shown_ < wanted && shown_ + 1 < static_cast<int>(avi_.frames())) {
        ++shown_;
        try {
            decoder_.decode(avi_.frame(static_cast<std::size_t>(shown_)));
        } catch (const FormatError&) {
            // A damaged frame leaves the picture as it was.
        }
    }
    canvas.clear();
    const Image& pic = decoder_.picture();
    canvas.draw(pic, (canvas.width() - pic.width) / 2, (canvas.height() - pic.height) / 2);
}

void FilmScreen::pointerReleased(Point) { finish(); }

std::optional<Point> FilmScreen::command(Command) {
    finish();
    return std::nullopt;
}

} // namespace nagus
