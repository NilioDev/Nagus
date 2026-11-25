// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/screen.hpp"

#include <cmath>
#include <limits>

namespace nagus {

Panel::Panel(Assets& assets, const std::string& layoutName) : assets_(assets), layout_(&assets.layout(layoutName)) {}

Point Panel::origin() const { return origin_ ? *origin_ : Point{layout_->rect.x, layout_->rect.y}; }

Rect Panel::rect(std::uint32_t id) const {
    if (const auto* e = layout_->find(id)) {
        const Point o = origin();
        return {o.x + e->rect.x, o.y + e->rect.y, e->rect.w, e->rect.h};
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
        canvas.draw(assets_.image(l.background), origin().x, origin().y);
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
        const Rect r{origin().x + e.rect.x, origin().y + e.rect.y, e.rect.w, e.rect.h};
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
        const Rect r{origin().x + it->rect.x, origin().y + it->rect.y, it->rect.w, it->rect.h};
        if (r.contains(p.x, p.y)) {
            return it->id;
        }
    }
    return std::nullopt;
}

void Panel::pointerMoved(Point p) {
    assets_.hover(this, buttonAt(p));
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
        assets_.clicked();
        return held;
    }
    return std::nullopt;
}

std::optional<Point> Panel::step(Command direction, Point from) const {
    const Layout& l = *layout_;
    const auto current = buttonAt(from);
    Point start = from;
    if (current) {
        const Rect r = rect(*current);
        start = {r.centerX(), r.centerY()};
    }
    std::optional<Point> best;
    double bestScore = std::numeric_limits<double>::max();
    for (const auto& e : l.elements) {
        if (e.kind != LayoutElement::Kind::Button || !usable(e.id) || e.id == current) {
            continue;
        }
        const Point o = origin();
        const Point c{o.x + e.rect.centerX(), o.y + e.rect.centerY()};
        const int dx = c.x - start.x;
        const int dy = c.y - start.y;
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

// Breaks a line into lines no wider than the box, between words (or inside
// a word too long for a line).
std::vector<std::string> wrapText(const Font& font, const std::string& text, int width) {
    std::vector<std::string> out;
    std::string line;
    std::size_t i = 0;
    while (i < text.size()) {
        std::size_t end = text.find(' ', i);
        if (end == std::string::npos) {
            end = text.size();
        }
        std::string word = text.substr(i, end - i);
        i = end + 1;
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (font.width(candidate) <= width) {
            line = candidate;
            continue;
        }
        if (!line.empty()) {
            out.push_back(line);
        }
        line.clear();
        while (font.width(word) > width && word.size() > 1) {
            std::size_t n = word.size() - 1;
            while (n > 1 && font.width(word.substr(0, n)) > width) {
                --n;
            }
            out.push_back(word.substr(0, n));
            word.erase(0, n);
        }
        line = word;
    }
    if (!line.empty() || out.empty()) {
        out.push_back(line);
    }
    return out;
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
