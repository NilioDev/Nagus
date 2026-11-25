// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/data/animation.hpp"

namespace nagus {

namespace {

constexpr std::size_t kHeader = 16;
constexpr int kMaxFrames = 1024;

} // namespace

Animation readAnimation(ByteView data) {
    Reader r(data);
    Animation a;
    a.frameWidth = r.u16();
    a.frameHeight = r.u16();
    a.frames = r.u16();
    r.u16(); // the same number again
    const int sheetWidth = r.u16();
    const int sheetHeight = r.u16();
    const std::uint32_t size = r.u32();
    if (a.frameWidth <= 0 || a.frameHeight <= 0 || a.frames <= 0 || a.frames > kMaxFrames ||
        size > data.size() - kHeader) {
        throw FormatError("bad animation");
    }
    a.sheet = readTga(data.subspan(kHeader, size));
    const int columns = a.sheet.width / a.frameWidth;
    const int rows = a.sheet.height / a.frameHeight;
    if (a.sheet.width != sheetWidth || a.sheet.height != sheetHeight || columns <= 0 || columns * rows < a.frames) {
        throw FormatError("bad animation");
    }
    return a;
}

Image Animation::frame(int i) const {
    Image f(frameWidth, frameHeight);
    if (frames <= 0) {
        return f;
    }
    i = ((i % frames) + frames) % frames;
    const int columns = sheet.width / frameWidth;
    const int x0 = (i % columns) * frameWidth;
    const int y0 = (i / columns) * frameHeight;
    for (int y = 0; y < frameHeight; ++y) {
        for (int x = 0; x < frameWidth; ++x) {
            f.at(x, y) = sheet.at(x0 + x, y0 + y);
        }
    }
    return f;
}

} // namespace nagus
