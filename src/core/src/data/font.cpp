// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/data/font.hpp"

#include <algorithm>
#include <map>
#include <string>

namespace nagus {

namespace {

// The characters of the five rows of the sheet, in order, as Windows-1252
// bytes. Worked out by looking at the sheets (docs/formats/fonts.md).
const std::array<std::string, 5>& sheetRows() {
    static const std::array<std::string, 5> rows{
        "1234567890",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        "abcdefghijklmnopqrstuvwxyz",
        "`-=\\[];',./~!@#$%^&*()_+|{}:\"<>?",
        "\xe4\xe2\xe0\xea\xe8\xe9\xef\xee\xf6\xf4\xfc\xf1\xc7\xe7\xc4\xd6\xdc\xdf\xae\xa9\x99\x85\x96\xbf\xa1\xb7",
    };
    return rows;
}

constexpr int kMaxSide = 4096;

} // namespace

Font::Font(ByteView data) {
    map_.fill(-1);
    Reader r(data);
    const int cell = r.u8();
    r.seek(6);
    const std::size_t count = r.u16();
    if (cell < 3 || count == 0 || count > 256) {
        throw FormatError("bad font header");
    }
    r.seek(20);
    std::vector<std::uint16_t> widths(count);
    for (auto& v : widths) {
        v = r.u16();
    }
    // The sheet follows the table of widths and 12 more bytes.
    r.seek(20 + 2 * count + 12);
    const int w = r.u16();
    const int h = r.u16();
    if (w <= 0 || h <= 0 || w > kMaxSide || h > kMaxSide) {
        throw FormatError("bad font sheet size");
    }
    const ByteView px = r.bytes(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 2);
    // The sheet is stored from the bottom row up.
    auto at = [&](int x, int y) {
        const std::size_t i = (static_cast<std::size_t>(h - 1 - y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)) * 2;
        return static_cast<std::uint16_t>(px[i] | (px[i + 1] << 8));
    };
    // The separating lines' colour: the commonest colour other than black on
    // the top row, which is a line above the first row of glyphs.
    std::map<std::uint16_t, int> topRow;
    for (int x = 0; x < w; ++x) {
        if (at(x, 0) != 0) {
            ++topRow[at(x, 0)];
        }
    }
    if (topRow.empty()) {
        throw FormatError("font sheet without lines");
    }
    const std::uint16_t line =
        std::max_element(topRow.begin(), topRow.end(), [](const auto& a, const auto& b) { return a.second < b.second; })->first;
    height_ = cell - 1;
    const auto& rows = sheetRows();
    int brightest = 0;
    std::size_t index = 0;
    for (std::size_t row = 0; row < rows.size(); ++row) {
        const int top = static_cast<int>(row) * cell + 1;
        if (top + height_ > h) {
            throw FormatError("font sheet too small");
        }
        int x = 0;
        while (x < w && at(x, top) != line) {
            ++x;
        }
        for (const char ch : rows[row]) {
            // Some lines are two pixels wide.
            while (x < w && at(x, top) == line) {
                ++x;
            }
            int right = x;
            while (right < w && at(right, top) != line) {
                ++right;
            }
            const bool last = ch == rows[row].back();
            if (right >= w && last) {
                // The last glyph of a row may lack its closing line: the
                // table of widths tells where it ends.
                right = std::min(w, x + std::max(1, static_cast<int>(widths[index]) - 1));
            } else if (right >= w) {
                throw FormatError("font sheet has fewer glyphs than expected");
            }
            ++index;
            Glyph g;
            g.width = right - x;
            g.alpha.resize(static_cast<std::size_t>(g.width) * static_cast<std::size_t>(height_));
            for (int gy = 0; gy < height_; ++gy) {
                for (int gx = 0; gx < g.width; ++gx) {
                    const std::uint16_t c = at(x + gx, top + gy);
                    int level = 0;
                    if (c != line && c != 0x7c1f) {
                        level = std::max({(c >> 10) & 31, (c >> 5) & 31, c & 31}) * 255 / 31;
                    }
                    g.alpha[static_cast<std::size_t>(gy * g.width + gx)] = static_cast<std::uint8_t>(level);
                }
            }
            for (const std::uint8_t a : g.alpha) {
                brightest = std::max<int>(brightest, a);
            }
            map_[static_cast<std::uint8_t>(ch)] = static_cast<std::int16_t>(glyphs_.size());
            glyphs_.push_back(std::move(g));
            x = right;
        }
    }
    // The sheets are drawn in shades of grey or of one colour: the brightest
    // shade is the full text colour.
    if (brightest > 0) {
        for (auto& g : glyphs_) {
            for (auto& a : g.alpha) {
                a = static_cast<std::uint8_t>(a * 255 / brightest);
            }
        }
    }
    space_ = std::max(2, height_ / 3);
}

const Font::Glyph* Font::glyph(std::uint8_t c) const {
    const int i = map_[c];
    return i < 0 ? nullptr : &glyphs_[static_cast<std::size_t>(i)];
}

int Font::advance(std::uint8_t c) const {
    if (const Glyph* g = glyph(c)) {
        return g->width + 1;
    }
    return space_;
}

int Font::width(std::string_view text) const {
    int total = 0;
    for (const char c : text) {
        total += advance(static_cast<std::uint8_t>(c));
    }
    return total > 0 ? total - 1 : 0;
}

} // namespace nagus
