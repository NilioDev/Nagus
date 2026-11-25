// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/data/layout.hpp"

namespace nagus {

namespace {

constexpr std::size_t kHeaderSize = 105;
constexpr std::size_t kNameSize = 13;
constexpr int kMaxCoordinate = 4096;

std::string pictureName(std::string name) {
    return lower(name) == "none" ? std::string() : name;
}

Rect readRect(Reader& r) {
    Rect rect;
    rect.x = r.i32();
    rect.y = r.i32();
    rect.w = r.i32();
    rect.h = r.i32();
    if (rect.x < -kMaxCoordinate || rect.y < -kMaxCoordinate || rect.x > kMaxCoordinate || rect.y > kMaxCoordinate ||
        rect.w < 0 || rect.h < 0 || rect.w > kMaxCoordinate || rect.h > kMaxCoordinate) {
        throw FormatError("bad position in screen layout");
    }
    return rect;
}

} // namespace

const LayoutElement* Layout::find(std::uint32_t wanted) const {
    for (const auto& e : elements) {
        if (e.id == wanted) {
            return &e;
        }
    }
    return nullptr;
}

Layout readLayout(ByteView data) {
    Reader r(data);
    if (r.u32() != 2) {
        throw FormatError("unknown screen layout version");
    }
    r.u32();
    Layout layout;
    layout.id = r.u32();
    layout.rect = readRect(r);
    layout.background = pictureName(r.text(kNameSize));
    r.seek(kHeaderSize);
    while (r.left() > 0) {
        const std::size_t start = r.pos();
        const std::uint32_t type = r.u32();
        LayoutElement e;
        e.id = r.u32();
        e.rect = readRect(r);
        switch (type) {
        case 2: { // button: 76 bytes
            e.kind = LayoutElement::Kind::Button;
            r.text(kNameSize);
            r.i32();
            e.buttonStyle = r.u8();
            e.repeats = r.u8() != 0;
            r.skip(7);
            e.upImage = pictureName(r.text(kNameSize));
            e.downImage = pictureName(r.text(kNameSize));
            break;
        }
        case 7: // slider: 119 bytes
        case 8: { // scroll bar: 73 bytes
            e.kind = type == 7 ? LayoutElement::Kind::Slider : LayoutElement::Kind::ScrollBar;
            const std::size_t size = type == 7 ? 119 : 73;
            e.images.push_back(pictureName(r.text(kNameSize)));
            r.seek(start + size);
            break;
        }
        case 13: { // picture slot: 41 bytes
            e.kind = LayoutElement::Kind::Picture;
            r.text(kNameSize);
            r.i32();
            break;
        }
        case 14: { // text field: 101 bytes
            e.kind = LayoutElement::Kind::Text;
            r.text(kNameSize);
            r.skip(8 + 4 + 4);
            const std::uint8_t red = r.u8();
            const std::uint8_t green = r.u8();
            const std::uint8_t blue = r.u8();
            e.colour = rgb(red, green, blue);
            e.textFlags = r.u8();
            r.skip(27);
            // Without this flag the game's code chooses the colour.
            e.ownColour = r.u32() != 0;
            e.font = r.text(kNameSize);
            break;
        }
        default:
            throw FormatError("unknown element in screen layout");
        }
        layout.elements.push_back(std::move(e));
    }
    return layout;
}

} // namespace nagus
