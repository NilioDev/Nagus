// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Targa layout is the public one (Truevision TGA 2.0): an 18-byte header,
// an optional ID field, then the pixels row by row, from the bottom row up
// unless bit 5 of the descriptor byte says from the top.
#include "nagus/data/image.hpp"

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

} // namespace nagus
