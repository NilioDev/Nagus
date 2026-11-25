// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game's films: AVI files with Cinepak pictures and, in some, PCM
// sound (see docs/formats/films.md). The file is read as it plays; only the
// list of where each frame is, and the sound, are kept.
#pragma once

#include "nagus/data/image.hpp"
#include "nagus/sound/pcm.hpp"
#include "nagus/util/source.hpp"

#include <array>
#include <memory>

namespace nagus {

class AviFile {
public:
    explicit AviFile(std::shared_ptr<Source> source);

    int width() const { return width_; }
    int height() const { return height_; }
    // Microseconds each frame shows.
    std::uint32_t frameTime() const { return frameTime_; }
    std::size_t frames() const { return video_.size(); }
    // The compressed data of a frame.
    Bytes frame(std::size_t index) const;
    // The sound track (empty if the film has none).
    const Pcm& sound() const { return sound_; }

private:
    struct Chunk {
        std::uint64_t offset = 0;
        std::uint32_t size = 0;
    };
    void readList(std::uint64_t start, std::uint64_t end, int depth);

    std::shared_ptr<Source> source_;
    int width_ = 0;
    int height_ = 0;
    std::uint32_t frameTime_ = 0;
    int videoStream_ = -1;
    int audioStream_ = -1;
    int streams_ = 0;
    int audioFormat_ = 0;
    int audioBits_ = 0;
    std::vector<Chunk> video_;
    Pcm sound_;
};

// Decodes Cinepak frames, each building on the one before.
class CinepakDecoder {
public:
    CinepakDecoder(int width, int height);
    // Decodes a frame into the picture. Throws FormatError on bad data; the
    // picture then holds what was decoded.
    void decode(ByteView data);
    const Image& picture() const { return picture_; }

private:
    struct Entry {
        std::array<std::uint8_t, 4> y{};
        std::int8_t u = 0;
        std::int8_t v = 0;
    };
    struct Books {
        std::array<Entry, 256> v1{};
        std::array<Entry, 256> v4{};
    };
    void decodeStrip(ByteView data, Books& books, int x1, int y1, int x2, int y2, bool key);
    void readBook(ByteView data, std::array<Entry, 256>& book, bool partial, bool grey);
    void put(int x, int y, const Entry& e, int i);

    Image picture_;
    std::vector<Books> strips_;
};

} // namespace nagus
