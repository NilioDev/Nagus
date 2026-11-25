// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pictures. The original keeps its art as uncompressed Targa (.tga) files,
// almost all with 16-bit pixels (5 bits per colour), where the colour
// magenta (0x7C1F) marks see-through pixels. See docs/formats/images.md.
#pragma once

#include "nagus/util/bytes.hpp"

#include <cstdint>
#include <vector>

namespace nagus {

// 0xAARRGGBB; alpha is 0 (see-through) or 255.
using Pixel = std::uint32_t;

constexpr Pixel rgb(int r, int g, int b) {
    return 0xff000000u | (static_cast<Pixel>(r & 255) << 16) | (static_cast<Pixel>(g & 255) << 8) |
           static_cast<Pixel>(b & 255);
}

// A 15-bit colour of the original (xRRRRRGGGGGBBBBB) as a pixel.
constexpr Pixel fromRgb555(std::uint16_t c) {
    const int r = (c >> 10) & 31;
    const int g = (c >> 5) & 31;
    const int b = c & 31;
    return rgb((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2));
}

struct Image {
    int width = 0;
    int height = 0;
    std::vector<Pixel> pixels; // rows from the top

    Image() = default;
    Image(int w, int h, Pixel fill = 0) : width(w), height(h), pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), fill) {}

    Pixel at(int x, int y) const { return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)]; }
    Pixel& at(int x, int y) { return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)]; }
    bool empty() const { return width == 0 || height == 0; }
};

// Reads an uncompressed Targa picture (16, 24 or 32 bits per pixel).
Image readTga(ByteView data);

} // namespace nagus
