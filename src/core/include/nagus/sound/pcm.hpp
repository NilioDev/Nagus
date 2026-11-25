// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sound as 16-bit samples, and the readers for the game's sound data: WAV
// files (plain or IMA ADPCM) and raw IMA ADPCM blocks as kept in the
// game's sound archives (see docs/formats/sound.md).
#pragma once

#include "nagus/util/bytes.hpp"

#include <cstdint>
#include <vector>

namespace nagus {

struct Pcm {
    int rate = 22050;
    int channels = 1;
    std::vector<std::int16_t> samples; // interleaved by channel

    std::size_t frames() const { return channels > 0 ? samples.size() / static_cast<std::size_t>(channels) : 0; }
};

// Decodes IMA ADPCM in blocks of `blockAlign` bytes (one or two channels),
// giving at most `maxFrames` frames. Throws FormatError on bad input.
std::vector<std::int16_t> decodeImaAdpcm(ByteView data, int channels, std::size_t blockAlign, std::size_t maxFrames);

// Reads a WAV file: 8- or 16-bit PCM or IMA ADPCM, one or two channels.
// Throws FormatError for anything else.
Pcm readWav(ByteView data);

} // namespace nagus
