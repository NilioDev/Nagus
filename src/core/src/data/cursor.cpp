// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/data/cursor.hpp"

namespace nagus {

namespace {

constexpr int kMaxFrames = 256;

} // namespace

Cursor readCursor(ByteView data) {
    Reader r(data);
    Cursor c;
    c.hotX = r.u16();
    c.hotY = r.u16();
    const int frames = r.u16();
    c.frameMs = r.u16();
    r.u16(); // width
    r.u16(); // height of all frames
    r.u32(); // size of the picture that follows
    if (frames <= 0 || frames > kMaxFrames) {
        throw FormatError("bad pointer file");
    }
    // One Targa picture follows, with the frames stacked from the top.
    const Image all = readTga(data.subspan(r.pos()));
    const int h = all.height / frames;
    if (h <= 0) {
        throw FormatError("bad pointer file");
    }
    for (int i = 0; i < frames; ++i) {
        Image f(all.width, h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < all.width; ++x) {
                f.at(x, y) = all.at(x, i * h + y);
            }
        }
        c.frames.push_back(std::move(f));
    }
    return c;
}

} // namespace nagus
