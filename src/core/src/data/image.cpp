// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Targa layout is the public one (Truevision TGA 2.0): an 18-byte header,
// an optional ID field, then the pixels row by row, from the bottom row up
// unless bit 5 of the descriptor byte says from the top.
#include "nagus/data/image.hpp"

#include <array>
#include <utility>

namespace nagus {

namespace {

constexpr int kMaxSide = 8192;
constexpr std::uint16_t kSeeThrough = 0x7c1f;

} // namespace

Image readTga(ByteView data) {
    Reader r(data);
    const std::uint8_t idLength = r.u8();
    const std::uint8_t colourMapType = r.u8();
    const std::uint8_t type = r.u8();
    r.skip(5); // colour map spec
    r.skip(4); // origin
    const int width = r.u16();
    const int height = r.u16();
    const int bits = r.u8();
    const std::uint8_t descriptor = r.u8();
    if (colourMapType != 0 || type != 2) {
        throw FormatError("unsupported Targa picture (only uncompressed true colour)");
    }
    if (bits != 16 && bits != 24 && bits != 32) {
        throw FormatError("unsupported Targa pixel size");
    }
    if (width <= 0 || height <= 0 || width > kMaxSide || height > kMaxSide) {
        throw FormatError("bad Targa picture size");
    }
    r.skip(idLength);
    const std::size_t bpp = static_cast<std::size_t>(bits) / 8;
    const ByteView px = r.bytes(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * bpp);
    Image img(width, height);
    const bool topDown = (descriptor & 0x20) != 0;
    for (int y = 0; y < height; ++y) {
        const int row = topDown ? y : height - 1 - y;
        const std::uint8_t* src = px.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * bpp;
        for (int x = 0; x < width; ++x, src += bpp) {
            Pixel p = 0;
            if (bpp == 2) {
                const auto c = static_cast<std::uint16_t>(src[0] | (src[1] << 8));
                p = (c & 0x7fff) == kSeeThrough ? 0 : fromRgb555(c);
            } else if (bpp == 3) {
                p = rgb(src[2], src[1], src[0]);
            } else {
                p = src[3] >= 128 ? rgb(src[2], src[1], src[0]) : 0;
            }
            img.at(x, row) = p;
        }
    }
    return img;
}

// GIF, from the public specification (GIF89a): a header and logical screen
// descriptor with an optional global colour table, then blocks: extensions
// (0x21), each a run of sub-blocks, and images (0x2c) with an optional local
// colour table and LZW-coded pixel indices in sub-blocks; 0x3b ends it.
Image readGif(ByteView data) {
    Reader r(data);
    const ByteView magic = r.bytes(6);
    if (magic[0] != 'G' || magic[1] != 'I' || magic[2] != 'F') {
        throw FormatError("not a GIF picture");
    }
    r.skip(4); // screen size
    const std::uint8_t screenFlags = r.u8();
    r.skip(2); // background, aspect
    std::array<Pixel, 256> global{};
    if (screenFlags & 0x80) {
        const std::size_t n = std::size_t{2} << (screenFlags & 7);
        for (std::size_t i = 0; i < n; ++i) {
            const int red = r.u8();
            const int green = r.u8();
            const int blue = r.u8();
            global[i] = rgb(red, green, blue);
        }
    }
    int transparent = -1;
    for (;;) {
        const std::uint8_t block = r.u8();
        if (block == 0x3b) {
            throw FormatError("GIF without a picture");
        }
        if (block == 0x21) {
            const std::uint8_t label = r.u8();
            bool first = true;
            for (std::uint8_t n = r.u8(); n != 0; n = r.u8()) {
                const ByteView sub = r.bytes(n);
                if (label == 0xf9 && first && n >= 4) {
                    transparent = (sub[0] & 1) ? sub[3] : -1;
                }
                first = false;
            }
            continue;
        }
        if (block != 0x2c) {
            throw FormatError("bad GIF block");
        }
        r.skip(4); // position
        const int width = r.u16();
        const int height = r.u16();
        const std::uint8_t flags = r.u8();
        if (width <= 0 || height <= 0 || width > kMaxSide || height > kMaxSide) {
            throw FormatError("bad GIF picture size");
        }
        std::array<Pixel, 256> colours = global;
        if (flags & 0x80) {
            const std::size_t n = std::size_t{2} << (flags & 7);
            for (std::size_t i = 0; i < n; ++i) {
                const int red = r.u8();
                const int green = r.u8();
                const int blue = r.u8();
                colours[i] = rgb(red, green, blue);
            }
        }
        const int minBits = r.u8();
        if (minBits < 2 || minBits > 8) {
            throw FormatError("bad GIF code size");
        }
        // Gather the sub-blocks.
        std::vector<std::uint8_t> code;
        for (std::uint8_t n = r.u8(); n != 0; n = r.u8()) {
            const ByteView sub = r.bytes(n);
            code.insert(code.end(), sub.begin(), sub.end());
        }
        // LZW: each table entry is a prefix entry and a last byte.
        const std::size_t total = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        std::vector<std::uint8_t> out;
        out.reserve(total);
        constexpr int kMaxCodes = 4096;
        std::vector<int> prefix(kMaxCodes, -1);
        std::vector<std::uint8_t> last(kMaxCodes, 0);
        std::vector<std::uint8_t> stack;
        const int clear = 1 << minBits;
        const int end = clear + 1;
        for (int i = 0; i < clear; ++i) {
            last[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);
        }
        int bits = minBits + 1;
        int next = clear + 2;
        int previous = -1;
        std::size_t bitPos = 0;
        while (out.size() < total && bitPos + static_cast<std::size_t>(bits) <= code.size() * 8) {
            int c = 0;
            for (int b = 0; b < bits; ++b, ++bitPos) {
                c |= ((code[bitPos / 8] >> (bitPos % 8)) & 1) << b;
            }
            if (c == clear) {
                bits = minBits + 1;
                next = clear + 2;
                previous = -1;
                continue;
            }
            if (c == end) {
                break;
            }
            if (previous < 0) {
                if (c >= clear) {
                    throw FormatError("bad GIF code");
                }
                out.push_back(static_cast<std::uint8_t>(c));
                previous = c;
                continue;
            }
            if (c > next) {
                throw FormatError("bad GIF code");
            }
            // A known string, or (c == next) the previous one and its own
            // first byte.
            stack.clear();
            for (int at = c == next ? previous : c;; at = prefix[static_cast<std::size_t>(at)]) {
                stack.push_back(last[static_cast<std::size_t>(at)]);
                if (at < clear) {
                    break;
                }
            }
            const std::uint8_t head = stack.back();
            for (auto it = stack.rbegin(); it != stack.rend() && out.size() < total; ++it) {
                out.push_back(*it);
            }
            if (c == next && out.size() < total) {
                out.push_back(head);
            }
            if (next < kMaxCodes) {
                prefix[static_cast<std::size_t>(next)] = previous;
                last[static_cast<std::size_t>(next)] = head;
                ++next;
                if (next == (1 << bits) && bits < 12) {
                    ++bits;
                }
            }
            previous = c;
        }
        out.resize(total, 0);
        Image img(width, height);
        const bool interlaced = (flags & 0x40) != 0;
        std::vector<int> rows;
        if (interlaced) {
            for (const auto& [start, step] : {std::pair{0, 8}, std::pair{4, 8}, std::pair{2, 4}, std::pair{1, 2}}) {
                for (int y = start; y < height; y += step) {
                    rows.push_back(y);
                }
            }
        } else {
            for (int y = 0; y < height; ++y) {
                rows.push_back(y);
            }
        }
        for (std::size_t n = 0; n < rows.size(); ++n) {
            for (int x = 0; x < width; ++x) {
                const std::uint8_t index = out[n * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
                img.at(x, rows[n]) = index == transparent ? 0u : colours[index];
            }
        }
        return img;
    }
}

} // namespace nagus
