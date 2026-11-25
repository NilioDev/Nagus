// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/screen.hpp"

#include <cmath>
#include <limits>

namespace nagus {

Panel::Panel(Assets& assets, const std::string& layoutName) : assets_(assets), layout_(&assets.layout(layoutName)) {}

Rect Panel::rect(std::uint32_t id) const {
    if (const auto* e = layout_->find(id)) {
        return e->rect;
    }
    return {};
}

void Panel::setEnabled(std::uint32_t id, bool enabled) { states_[id].enabled = enabled; }
void Panel::setVisible(std::uint32_t id, bool visible) { states_[id].visible = visible; }
void Panel::setOn(std::uint32_t id, bool on) { states_[id].on = on; }

bool Panel::usable(std::uint32_t id) const {
    const auto it = states_.find(id);
    return it == states_.end() || (it->second.enabled && it->second.visible);
}

void Panel::draw(Canvas& canvas) {
    const Layout& l = *layout_;
    if (!l.background.empty()) {
        canvas.draw(assets_.image(l.background), l.rect.x, l.rect.y);
    }
    for (const auto& e : l.elements) {
        if (e.kind != LayoutElement::Kind::Button) {
            continue;
        }
        const auto it = states_.find(e.id);
        const State s = it == states_.end() ? State{} : it->second;
        if (!s.visible) {
            continue;
        }
        const bool down = s.on || (held_ == e.id && overHeld_);
        const std::string& name = down && !e.downImage.empty() ? e.downImage : e.upImage;
        const Rect r{l.rect.x + e.rect.x, l.rect.y + e.rect.y, e.rect.w, e.rect.h};
        canvas.draw(assets_.image(name), r.x, r.y);
        if (!s.enabled) {
            canvas.blend(r, rgb(0, 0, 0), 150);
        }
    }
}

std::optional<std::uint32_t> Panel::buttonAt(Point p) const {
    const Layout& l = *layout_;
    // Later elements lie on top.
    for (auto it = l.elements.rbegin(); it != l.elements.rend(); ++it) {
        if (it->kind != LayoutElement::Kind::Button || !usable(it->id)) {
            continue;
        }
        const Rect r{l.rect.x + it->rect.x, l.rect.y + it->rect.y, it->rect.w, it->rect.h};
        if (r.contains(p.x, p.y)) {
            return it->id;
        }
    }
    return std::nullopt;
}

void Panel::pointerMoved(Point p) {
    if (held_) {
        overHeld_ = buttonAt(p) == held_;
    }
}

void Panel::pointerPressed(Point p) {
    held_ = buttonAt(p);
    overHeld_ = held_.has_value();
}

std::optional<std::uint32_t> Panel::pointerReleased(Point p) {
    const auto held = held_;
    held_.reset();
    overHeld_ = false;
    if (held && buttonAt(p) == held) {
        return held;
    }
    return std::nullopt;
}

std::optional<Point> Panel::step(Command direction, Point from) const {
    const Layout& l = *layout_;
    const auto current = buttonAt(from);
    Point origin = from;
    if (current) {
        const Rect r = rect(*current);
        origin = {l.rect.x + r.centerX(), l.rect.y + r.centerY()};
    }
    std::optional<Point> best;
    double bestScore = std::numeric_limits<double>::max();
    for (const auto& e : l.elements) {
        if (e.kind != LayoutElement::Kind::Button || !usable(e.id) || e.id == current) {
            continue;
        }
        const Point c{l.rect.x + e.rect.centerX(), l.rect.y + e.rect.centerY()};
        const int dx = c.x - origin.x;
        const int dy = c.y - origin.y;
        if (!current) {
            // Nothing under the pointer yet: start at the button nearest it.
            const double d = std::hypot(dx, dy);
            if (d < bestScore) {
                bestScore = d;
                best = c;
            }
            continue;
        }
        int along = 0;
        int across = 0;
        switch (direction) {
        case Command::Up: along = -dy; across = dx; break;
        case Command::Down: along = dy; across = dx; break;
        case Command::Left: along = -dx; across = dy; break;
        case Command::Right: along = dx; across = dy; break;
        default: return std::nullopt;
        }
        if (along <= 0) {
            continue;
        }
        const double score = along + 2.0 * std::abs(across);
        if (score < bestScore) {
            bestScore = score;
            best = c;
        }
    }
    return best;
}

void drawParagraph(Canvas& canvas, const Font& font, const std::string& text, const Rect& box, Pixel colour) {
    int y = box.y;
    const int lineHeight = font.height() + 2;
    std::string line;
    std::size_t i = 0;
    auto flush = [&] {
        if (y + font.height() <= box.y + box.h) {
            canvas.text(font, line, box.x, y, colour);
        }
        y += lineHeight;
        line.clear();
    };
    while (i < text.size()) {
        if (text[i] == '\n') {
            flush();
            ++i;
            continue;
        }
        std::size_t end = i;
        while (end < text.size() && text[end] != ' ' && text[end] != '\n') {
            ++end;
        }
        const std::string word = text.substr(i, end - i);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && font.width(candidate) > box.w) {
            flush();
            line = word;
        } else {
            line = candidate;
        }
        i = end;
        while (i < text.size() && text[i] == ' ') {
            ++i;
        }
    }
    if (!line.empty()) {
        flush();
    }
}

} // namespace nagus
