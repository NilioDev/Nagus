// SPDX-License-Identifier: GPL-3.0-or-later
//
// Animations (.ani): turning planets and stars. A 16-byte header and one
// Targa picture holding all frames in rows. See docs/formats/images.md.
#pragma once

#include "nagus/data/image.hpp"

namespace nagus {

struct Animation {
    int frameWidth = 0;
    int frameHeight = 0;
    int frames = 0;
    Image sheet;

    // Frame `i` (wrapping around) as its own picture.
    Image frame(int i) const;
};

Animation readAnimation(ByteView data);

} // namespace nagus
