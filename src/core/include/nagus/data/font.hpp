// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's fonts (.fnt): a sheet of 120 glyphs in five rows, separated
// by one-pixel lines. The engine colours the glyphs itself. See
// docs/formats/fonts.md.
#pragma once

#include "nagus/data/image.hpp"
#include "nagus/util/bytes.hpp"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace nagus {

class Font {
public:
    struct Glyph {
        int width = 0;
        std::vector<std::uint8_t> alpha; // width × height, 0..255
    };

    // Reads a font file.
    explicit Font(ByteView data);

    int height() const { return height_; }
    // Width of a line of text (Windows-1252 bytes, as in the game's texts).
    int width(std::string_view text) const;
    const Glyph* glyph(std::uint8_t c) const;
    int advance(std::uint8_t c) const;

private:
    int height_ = 0;
    int space_ = 0;
    std::vector<Glyph> glyphs_;
    std::array<std::int16_t, 256> map_{};
};

} // namespace nagus
