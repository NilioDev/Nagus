// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/render/canvas.hpp"

#include <algorithm>

namespace nagus {

namespace {

Pixel mix(Pixel under, Pixel over, int alpha) {
    auto channel = [&](int shift) {
        const int a = static_cast<int>((under >> shift) & 255u);
        const int b = static_cast<int>((over >> shift) & 255u);
        return static_cast<Pixel>((a * (255 - alpha) + b * alpha) / 255) << shift;
    };
    return 0xff000000u | channel(16) | channel(8) | channel(0);
}

} // namespace

void Canvas::setClip(const Rect& r) {
    const int x0 = std::max(0, r.x);
    const int y0 = std::max(0, r.y);
    const int x1 = std::min(width(), r.x + r.w);
    const int y1 = std::min(height(), r.y + r.h);
    clip_ = {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
}

void Canvas::clear(Pixel colour) { std::fill(image_.pixels.begin(), image_.pixels.end(), colour); }

void Canvas::fill(const Rect& r, Pixel colour) {
    const int x0 = std::max(clip_.x, r.x);
    const int y0 = std::max(clip_.y, r.y);
    const int x1 = std::min(clip_.x + clip_.w, r.x + r.w);
    const int y1 = std::min(clip_.y + clip_.h, r.y + r.h);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            image_.at(x, y) = colour;
        }
    }
}

void Canvas::blend(const Rect& r, Pixel colour, int alpha) {
    const int x0 = std::max(clip_.x, r.x);
    const int y0 = std::max(clip_.y, r.y);
    const int x1 = std::min(clip_.x + clip_.w, r.x + r.w);
    const int y1 = std::min(clip_.y + clip_.h, r.y + r.h);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            image_.at(x, y) = mix(image_.at(x, y), colour, alpha);
        }
    }
}

void Canvas::frame(const Rect& r, Pixel colour, int t) {
    fill({r.x, r.y, r.w, t}, colour);
    fill({r.x, r.y + r.h - t, r.w, t}, colour);
    fill({r.x, r.y, t, r.h}, colour);
    fill({r.x + r.w - t, r.y, t, r.h}, colour);
}

void Canvas::draw(const Image& img, int x, int y) {
    const int x0 = std::max(0, clip_.x - x);
    const int y0 = std::max(0, clip_.y - y);
    const int x1 = std::min(img.width, clip_.x + clip_.w - x);
    const int y1 = std::min(img.height, clip_.y + clip_.h - y);
    for (int sy = y0; sy < y1; ++sy) {
        for (int sx = x0; sx < x1; ++sx) {
            const Pixel p = img.at(sx, sy);
            if ((p >> 24) != 0) {
                image_.at(x + sx, y + sy) = p;
            }
        }
    }
}

void Canvas::text(const Font& font, std::string_view s, int x, int y, Pixel colour) {
    for (const char ch : s) {
        const auto c = static_cast<std::uint8_t>(ch);
        if (const Font::Glyph* g = font.glyph(c)) {
            for (int gy = 0; gy < font.height(); ++gy) {
                const int py = y + gy;
                if (py < clip_.y || py >= clip_.y + clip_.h) {
                    continue;
                }
                for (int gx = 0; gx < g->width; ++gx) {
                    const int px = x + gx;
                    const int a = g->alpha[static_cast<std::size_t>(gy * g->width + gx)];
                    if (a != 0 && px >= clip_.x && px < clip_.x + clip_.w) {
                        image_.at(px, py) = mix(image_.at(px, py), colour, a);
                    }
                }
            }
        }
        x += font.advance(c);
    }
}

void Canvas::text(const Font& font, std::string_view s, const Rect& box, Pixel colour, Align align) {
    const int w = font.width(s);
    int x = box.x;
    if (align == Align::Center) {
        x = box.x + (box.w - w) / 2;
    } else if (align == Align::Right) {
        x = box.x + box.w - w;
    }
    const int y = box.y + (box.h - font.height()) / 2;
    text(font, s, x, y, colour);
}

} // namespace nagus
