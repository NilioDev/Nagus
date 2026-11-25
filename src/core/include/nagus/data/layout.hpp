// SPDX-License-Identifier: GPL-3.0-or-later
//
// Screen layouts (.wdf files): the background picture of a screen or window
// and where each of its buttons, pictures and text fields goes. The game's
// code gives the elements their meaning by their numbers. See
// docs/formats/layouts.md.
#pragma once

#include "nagus/data/image.hpp"
#include "nagus/util/bytes.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nagus {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    int centerX() const { return x + w / 2; }
    int centerY() const { return y + h / 2; }
};

struct LayoutElement {
    enum class Kind { Button, Slider, ScrollBar, Picture, Text, List, Other };
    Kind kind = Kind::Picture;
    std::uint32_t id = 0;
    Rect rect;
    // Buttons: the picture when up and when pressed down.
    std::string upImage;
    std::string downImage;
    // Buttons: 2 for the original's toggle buttons (on/off pictures), and
    // whether holding the button repeats (the arrows of a number field).
    std::uint8_t buttonStyle = 0;
    bool repeats = false;
    // Sliders and scroll bars: their pictures.
    std::vector<std::string> images;
    // Text fields: colour (when the layout sets one) and font.
    Pixel colour = rgb(255, 255, 255);
    bool ownColour = false;
    std::uint8_t textFlags = 0;
    std::string font;
};

struct Layout {
    std::uint32_t id = 0;
    Rect rect;
    std::string background; // a picture name, or empty
    std::vector<LayoutElement> elements;

    const LayoutElement* find(std::uint32_t id) const;
};

Layout readLayout(ByteView data);

} // namespace nagus
