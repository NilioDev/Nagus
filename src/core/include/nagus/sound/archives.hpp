// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game's sound archives (see docs/formats/sound.md): sfx.snd and
// english.snd hold numbered sounds, music.snd the music as tracks of
// pieces. They are read piece by piece, never whole.
#pragma once

#include "nagus/sound/pcm.hpp"
#include "nagus/util/source.hpp"

#include <memory>

namespace nagus {

// sfx.snd (WAV files) and english.snd (speech as IMA ADPCM).
class SoundArchive {
public:
    explicit SoundArchive(std::shared_ptr<Source> source);

    std::size_t size() const { return entries_.size(); }
    // A sound; throws FormatError if it cannot be read.
    Pcm sound(std::size_t index) const;

private:
    struct Entry {
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
        std::uint32_t length = 0; // bytes of sound once decoded (speech)
        std::uint32_t flags = 0;
    };
    std::shared_ptr<Source> source_;
    std::vector<Entry> entries_;
};

// music.snd: tracks, each a list of pieces played in turn.
class MusicArchive {
public:
    explicit MusicArchive(std::shared_ptr<Source> source);

    int tracks() const { return static_cast<int>(tracks_.size()); }
    int pieces(int track) const;
    // A piece of a track; throws FormatError if it cannot be read.
    Pcm piece(int track, int index) const;

private:
    struct Piece {
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
        std::uint32_t length = 0; // bytes once decoded
        std::uint32_t flags = 0;
    };
    std::shared_ptr<Source> source_;
    std::vector<std::vector<Piece>> tracks_;
};

// The sample rate of the game's music and speech.
inline constexpr int kSoundRate = 22050;
// Largest sound read (bytes in the archive).
inline constexpr std::uint32_t kMaxSoundBytes = 16u << 20;

} // namespace nagus
