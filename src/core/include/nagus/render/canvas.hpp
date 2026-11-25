// SPDX-License-Identifier: GPL-3.0-or-later
//
// The picture the engine draws into: the original's 800×600 screen. The app
// scales it to the window.
#pragma once

#include "nagus/data/font.hpp"
#include "nagus/data/image.hpp"
#include "nagus/data/layout.hpp"

#include <string_view>

namespace nagus {

inline constexpr int kScreenWidth = 800;
inline constexpr int kScreenHeight = 600;

enum class Align { Left, Center, Right };

class Canvas {
public:
    Canvas(int w = kScreenWidth, int h = kScreenHeight) : image_(w, h, rgb(0, 0, 0)) {}

    int width() const { return image_.width; }
    int height() const { return image_.height; }
    const Image& image() const { return image_; }
    // The pixels, for drawing that the calls below do not cover (3D).
    Image& target() { return image_; }

    // Drawing outside the clip rectangle is left out (all of the canvas
    // unless set).
    void setClip(const Rect& r);
    void clearClip() { clip_ = {0, 0, width(), height()}; }
    const Rect& clip() const { return clip_; }

    void clear(Pixel colour = rgb(0, 0, 0));
    void fill(const Rect& r, Pixel colour);
    // Draws a picture with its see-through pixels left out.
    void draw(const Image& img, int x, int y);
    // Draws the part `part` of a picture with its top left corner at x, y.
    void draw(const Image& img, const Rect& part, int x, int y);
    // Darkens or tints a rectangle: `alpha` 0..255 of `colour` over it.
    void blend(const Rect& r, Pixel colour, int alpha);
    void frame(const Rect& r, Pixel colour, int thickness = 1);
    // One line of text, its top at `y`.
    void text(const Font& font, std::string_view s, int x, int y, Pixel colour);
    // One line of text placed in a box, centred vertically.
    void text(const Font& font, std::string_view s, const Rect& box, Pixel colour, Align align);

private:
    Image image_;
    Rect clip_{0, 0, image_.width, image_.height};
};

} // namespace nagus
