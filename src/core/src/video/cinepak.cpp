// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cinepak, from its public description: each frame is a list of strips;
// each strip has two code books of colours (v1: one entry for a 4×4 block,
// v4: four entries, one for each 2×2 part) and the list of blocks.
#include "nagus/video/avi.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr std::size_t kFrameHeader = 10;
constexpr std::size_t kStripHeader = 12;
constexpr std::size_t kMaxStrips = 32;

std::uint32_t be16(ByteView d, std::size_t at) {
    if (at + 2 > d.size()) {
        throw FormatError("Cinepak: data cut short");
    }
    return static_cast<std::uint32_t>(d[at] << 8 | d[at + 1]);
}

std::uint32_t be32(ByteView d, std::size_t at) { return be16(d, at) << 16 | be16(d, at + 2); }

// Bits read from big-endian 32-bit words, highest first.
class Flags {
public:
    Flags(ByteView d, std::size_t& at) : d_(d), at_(at) {}
    bool next() {
        if (left_ == 0) {
            word_ = be32(d_, at_);
            at_ += 4;
            left_ = 32;
        }
        --left_;
        return (word_ >> left_) & 1u;
    }

private:
    ByteView d_;
    std::size_t& at_;
    std::uint32_t word_ = 0;
    int left_ = 0;
};

std::uint8_t byteAt(ByteView d, std::size_t& at) {
    if (at >= d.size()) {
        throw FormatError("Cinepak: data cut short");
    }
    return d[at++];
}

} // namespace

CinepakDecoder::CinepakDecoder(int width, int height) : picture_(width, height, 0xff000000u) {}

void CinepakDecoder::put(int x, int y, const Entry& e, int i) {
    if (x < 0 || y < 0 || x >= picture_.width || y >= picture_.height) {
        return;
    }
    const int yy = e.y[static_cast<std::size_t>(i)];
    const int r = std::clamp(yy + 2 * e.v, 0, 255);
    const int g = std::clamp(yy - e.u / 2 - e.v, 0, 255);
    const int b = std::clamp(yy + 2 * e.u, 0, 255);
    picture_.at(x, y) = 0xff000000u | static_cast<Pixel>(r) << 16 | static_cast<Pixel>(g) << 8 | static_cast<Pixel>(b);
}

void CinepakDecoder::readBook(ByteView d, std::array<Entry, 256>& book, bool partial, bool grey) {
    const std::size_t entry = grey ? 4 : 6;
    std::size_t at = 0;
    if (!partial) {
        for (std::size_t i = 0; i < 256 && at + entry <= d.size(); ++i) {
            Entry& e = book[i];
            std::copy_n(d.begin() + static_cast<std::ptrdiff_t>(at), 4, e.y.begin());
            e.u = grey ? 0 : static_cast<std::int8_t>(d[at + 4]);
            e.v = grey ? 0 : static_cast<std::int8_t>(d[at + 5]);
            at += entry;
        }
        return;
    }
    Flags flags(d, at);
    for (std::size_t i = 0; i < 256 && at < d.size(); ++i) {
        if (!flags.next()) {
            continue;
        }
        if (at + entry > d.size()) {
            break;
        }
        Entry& e = book[i];
        std::copy_n(d.begin() + static_cast<std::ptrdiff_t>(at), 4, e.y.begin());
        e.u = grey ? 0 : static_cast<std::int8_t>(d[at + 4]);
        e.v = grey ? 0 : static_cast<std::int8_t>(d[at + 5]);
        at += entry;
    }
}

void CinepakDecoder::decodeStrip(ByteView d, Books& books, int x1, int y1, int x2, int y2, bool key) {
    std::size_t at = 0;
    while (at + 4 <= d.size()) {
        const std::uint32_t id = be16(d, at);
        const std::uint32_t size = be16(d, at + 2);
        if (size < 4 || size > d.size() - at) {
            throw FormatError("Cinepak: bad chunk size");
        }
        const ByteView body = d.subspan(at + 4, size - 4);
        at += size;
        switch (id) {
        case 0x2000: readBook(body, books.v4, false, false); break;
        case 0x2100: readBook(body, books.v4, true, false); break;
        case 0x2200: readBook(body, books.v1, false, false); break;
        case 0x2300: readBook(body, books.v1, true, false); break;
        case 0x2400: readBook(body, books.v4, false, true); break;
        case 0x2500: readBook(body, books.v4, true, true); break;
        case 0x2600: readBook(body, books.v1, false, true); break;
        case 0x2700: readBook(body, books.v1, true, true); break;
        case 0x3000:
        case 0x3100:
        case 0x3200: {
            // The blocks, row by row: in 0x3000 a bit for each says v4 or
            // v1; in 0x3100 a first bit says whether it changes at all; in
            // 0x3200 all are v1.
            std::size_t p = 0;
            Flags flags(body, p);
            for (int y = y1; y < y2; y += 4) {
                for (int x = x1; x < x2; x += 4) {
                    if (p >= body.size() && id != 0x3100) {
                        return;
                    }
                    bool coded = true;
                    if (id == 0x3100) {
                        if (p >= body.size()) {
                            return;
                        }
                        coded = flags.next();
                    }
                    if (!coded) {
                        continue;
                    }
                    const bool four = id == 0x3200 ? false : flags.next();
                    if (four) {
                        for (int part = 0; part < 4; ++part) {
                            const Entry& e = books.v4[byteAt(body, p)];
                            const int px = x + (part & 1) * 2;
                            const int py = y + (part >> 1) * 2;
                            put(px, py, e, 0);
                            put(px + 1, py, e, 1);
                            put(px, py + 1, e, 2);
                            put(px + 1, py + 1, e, 3);
                        }
                    } else {
                        const Entry& e = books.v1[byteAt(body, p)];
                        for (int k = 0; k < 16; ++k) {
                            const int px = k & 3;
                            const int py = k >> 2;
                            put(x + px, y + py, e, (py >> 1) * 2 + (px >> 1));
                        }
                    }
                }
            }
            (void)key;
            break;
        }
        default: break; // unknown chunks are passed over
        }
    }
}

void CinepakDecoder::decode(ByteView data) {
    if (data.size() < kFrameHeader) {
        throw FormatError("Cinepak: frame too short");
    }
    const std::uint8_t flags = data[0];
    const std::size_t length = static_cast<std::size_t>(data[1]) << 16 | be16(data, 2);
    const std::size_t strips = be16(data, 8);
    if (strips > kMaxStrips) {
        throw FormatError("Cinepak: too many strips");
    }
    const ByteView frame = data.subspan(0, std::min(data.size(), std::max(length, kFrameHeader)));
    if (strips_.size() < strips) {
        strips_.resize(strips);
    }
    std::size_t at = kFrameHeader;
    int top = 0;
    for (std::size_t i = 0; i < strips; ++i) {
        if (at + kStripHeader > frame.size()) {
            throw FormatError("Cinepak: strip cut short");
        }
        const std::uint32_t id = be16(frame, at);
        const std::uint32_t size = be16(frame, at + 2);
        if (size < kStripHeader || size > frame.size() - at) {
            throw FormatError("Cinepak: bad strip size");
        }
        // A strip's place: given in full, or (when its top is 0) as a
        // height below the strip before.
        int y1 = static_cast<int>(be16(frame, at + 4));
        int x1 = static_cast<int>(be16(frame, at + 6));
        int y2 = static_cast<int>(be16(frame, at + 8));
        int x2 = static_cast<int>(be16(frame, at + 10));
        if (y1 == 0) {
            y1 = top;
            y2 = top + y2;
            x1 = 0;
            x2 = picture_.width;
        }
        y2 = std::min(y2, picture_.height);
        x2 = std::min(x2, picture_.width);
        // Unless the frame says otherwise, a strip starts from the code
        // books of the strip above.
        if (i > 0 && !(flags & 1)) {
            strips_[i] = strips_[i - 1];
        }
        decodeStrip(frame.subspan(at + kStripHeader, size - kStripHeader), strips_[i], x1, y1, x2, y2, id == 0x1000);
        top = y2;
        at += size;
    }
}

} // namespace nagus
