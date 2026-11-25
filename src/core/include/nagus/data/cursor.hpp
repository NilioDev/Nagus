// SPDX-License-Identifier: GPL-3.0-or-later
//
// Mouse pointers (.cur): one or more Targa frames with a hot spot and a frame
// time. See docs/formats/images.md.
#pragma once

#include "nagus/data/image.hpp"

#include <vector>

namespace nagus {

struct Cursor {
    int hotX = 0;
    int hotY = 0;
    int frameMs = 0;
    std::vector<Image> frames;

    const Image& frame(std::uint64_t ms) const {
        if (frames.size() < 2 || frameMs <= 0) {
            return frames.front();
        }
        return frames[(ms / static_cast<std::uint64_t>(frameMs)) % frames.size()];
    }
};

Cursor readCursor(ByteView data);

} // namespace nagus
