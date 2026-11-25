// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/sound/pcm.hpp"

#include <algorithm>
#include <array>

namespace nagus {

namespace {

// The IMA ADPCM step sizes and index changes (public standard tables).
constexpr std::array<int, 89> kSteps = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,    28,
    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,   494,
    544,   598,   658,   724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,
    9493,  10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
constexpr std::array<int, 8> kIndexChange = {-1, -1, -1, -1, 2, 4, 6, 8};

struct Channel {
    int predictor = 0;
    int index = 0;

    std::int16_t next(int nibble) {
        const int step = kSteps[static_cast<std::size_t>(index)];
        int diff = step >> 3;
        if (nibble & 1) {
            diff += step >> 2;
        }
        if (nibble & 2) {
            diff += step >> 1;
        }
        if (nibble & 4) {
            diff += step;
        }
        predictor = std::clamp(predictor + ((nibble & 8) ? -diff : diff), -32768, 32767);
        index = std::clamp(index + kIndexChange[static_cast<std::size_t>(nibble & 7)], 0, 88);
        return static_cast<std::int16_t>(predictor);
    }
};

} // namespace

std::vector<std::int16_t> decodeImaAdpcm(ByteView data, int channels, std::size_t blockAlign, std::size_t maxFrames) {
    if (channels < 1 || channels > 2) {
        throw FormatError("ADPCM: unsupported channel count");
    }
    const auto ch = static_cast<std::size_t>(channels);
    // Each block starts with four bytes for each channel, and holds whole
    // groups of four bytes for each channel after that.
    if (blockAlign < 4 * ch + 4 * ch || blockAlign > 65536 || (blockAlign - 4 * ch) % (4 * ch) != 0) {
        throw FormatError("ADPCM: bad block size");
    }
    std::vector<std::int16_t> out;
    out.reserve(std::min(maxFrames, data.size() * 2 / ch + 2) * ch);
    std::array<std::int16_t, 16> group{};
    for (std::size_t block = 0; block + 4 * ch <= data.size() && out.size() < maxFrames * ch; block += blockAlign) {
        const std::size_t end = std::min(data.size(), block + blockAlign);
        std::array<Channel, 2> state{};
        for (std::size_t c = 0; c < ch; ++c) {
            const std::size_t at = block + 4 * c;
            state[c].predictor = static_cast<std::int16_t>(data[at] | (data[at + 1] << 8));
            state[c].index = data[at + 2];
            if (state[c].index > 88) {
                throw FormatError("ADPCM: bad step index");
            }
        }
        for (std::size_t c = 0; c < ch; ++c) {
            out.push_back(static_cast<std::int16_t>(state[c].predictor));
        }
        // Then for each channel in turn four bytes: eight samples, low
        // nibble first.
        for (std::size_t at = block + 4 * ch; at + 4 * ch <= end; at += 4 * ch) {
            for (std::size_t c = 0; c < ch; ++c) {
                for (std::size_t k = 0; k < 4; ++k) {
                    const std::uint8_t b = data[at + 4 * c + k];
                    group[(2 * k) * ch + c] = state[c].next(b & 0x0f);
                    group[(2 * k + 1) * ch + c] = state[c].next(b >> 4);
                }
            }
            out.insert(out.end(), group.begin(), group.begin() + static_cast<std::ptrdiff_t>(8 * ch));
        }
    }
    out.resize(std::min(out.size(), maxFrames * ch));
    return out;
}

Pcm readWav(ByteView data) {
    Reader r(data);
    if (r.text(4) != "RIFF") {
        throw FormatError("WAV: no RIFF header");
    }
    r.u32();
    if (r.text(4) != "WAVE") {
        throw FormatError("WAV: not a WAVE file");
    }
    int format = 0;
    int channels = 0;
    int rate = 0;
    int blockAlign = 0;
    int bits = 0;
    std::size_t factFrames = 0;
    bool haveFact = false;
    ByteView samples;
    bool haveData = false;
    while (r.pos() + 8 <= data.size()) {
        const std::string id = r.text(4);
        const std::uint32_t size = r.u32();
        const std::size_t at = r.pos();
        if (size > data.size() - at) {
            // Some files claim more than they hold: take what is there.
            if (id == "data") {
                samples = data.subspan(at);
                haveData = true;
            }
            break;
        }
        if (id == "fmt") { // (text() drops the trailing space)
            if (size < 16) {
                throw FormatError("WAV: short format");
            }
            format = r.u16();
            channels = r.u16();
            rate = static_cast<int>(r.u32());
            r.u32();
            blockAlign = r.u16();
            bits = r.u16();
        } else if (id == "fact" && size >= 4) {
            factFrames = r.u32();
            haveFact = true;
        } else if (id == "data") {
            samples = data.subspan(at, size);
            haveData = true;
        }
        r.seek(at + size + (size & 1u) > data.size() ? data.size() : at + size + (size & 1u));
    }
    if (!haveData || channels < 1 || channels > 2 || rate < 1000 || rate > 192000) {
        throw FormatError("WAV: missing or unsupported format");
    }
    Pcm pcm;
    pcm.rate = rate;
    pcm.channels = channels;
    if (format == 1 && bits == 16) {
        pcm.samples.resize(samples.size() / 2);
        for (std::size_t i = 0; i < pcm.samples.size(); ++i) {
            pcm.samples[i] = static_cast<std::int16_t>(samples[2 * i] | (samples[2 * i + 1] << 8));
        }
    } else if (format == 1 && bits == 8) {
        pcm.samples.resize(samples.size());
        for (std::size_t i = 0; i < samples.size(); ++i) {
            pcm.samples[i] = static_cast<std::int16_t>((samples[i] - 128) * 256);
        }
    } else if (format == 0x11 && bits == 4) {
        const std::size_t limit = haveFact ? factFrames : samples.size() * 2;
        pcm.samples = decodeImaAdpcm(samples, channels, static_cast<std::size_t>(blockAlign), limit);
    } else {
        throw FormatError("WAV: unsupported encoding");
    }
    pcm.samples.resize(pcm.samples.size() - pcm.samples.size() % static_cast<std::size_t>(channels));
    return pcm;
}

} // namespace nagus
