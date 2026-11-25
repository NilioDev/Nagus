// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shows one of the game's films with its sound, centred on the screen.
// Any click, key or button ends it.
#pragma once

#include "nagus/sound/jukebox.hpp"
#include "nagus/ui/menus.hpp"
#include "nagus/video/avi.hpp"

#include <memory>

namespace nagus {

class FilmScreen : public MenuScreen {
public:
    // Opens a film of the library, with the sound of the film itself or of
    // a separate WAV file (soundFile, may be empty). Throws FormatError if
    // the film cannot be opened.
    FilmScreen(const Resources& resources, Jukebox& jukebox, const std::string& film, const std::string& soundFile);
    ~FilmScreen() override;

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    // The frame shown last (-1 before the first).
    int shownFrame() const { return shown_; }
    bool done() const { return done_; }

private:
    void finish();

    Jukebox& jukebox_;
    AviFile avi_;
    CinepakDecoder decoder_;
    std::shared_ptr<const Pcm> sound_;
    std::optional<std::uint64_t> start_;
    int shown_ = -1;
    bool done_ = false;
};

} // namespace nagus
