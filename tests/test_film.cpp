// SPDX-License-Identifier: GPL-3.0-or-later
#include "builders.hpp"
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/resources.hpp"
#include "nagus/video/avi.hpp"

using namespace nagus;
using namespace nagus::test;

namespace {

void be16(Bytes& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v >> 8));
    b.push_back(static_cast<std::uint8_t>(v));
}

void chunk(Bytes& b, std::uint32_t id, const Bytes& body) {
    be16(b, id);
    be16(b, static_cast<std::uint32_t>(body.size() + 4));
    b.insert(b.end(), body.begin(), body.end());
}

// A Cinepak frame of one 8×4 strip: a v1 and a v4 code book, then two
// blocks, the first v1, the second v4.
Bytes cinepakFrame() {
    Bytes strip;
    // v1: entry 0 grey 100; entry 1 with colour.
    chunk(strip, 0x2200, Bytes{100, 100, 100, 100, 0, 0, 100, 100, 100, 100, 10, 0xf6});
    // v4: entry 0 has four different greys.
    chunk(strip, 0x2000, Bytes{10, 20, 30, 40, 0, 0});
    Bytes blocks;
    put32(blocks, 0); // flags, read high bit first: v1, then v4
    blocks[3] = 0;
    blocks[0] = 0x40;
    blocks.push_back(1);                       // the v1 block: entry 1
    blocks.insert(blocks.end(), {0, 0, 0, 0}); // the v4 block: entry 0 four times
    chunk(strip, 0x3000, blocks);
    Bytes frame;
    frame.push_back(0);
    const std::size_t size = 10 + 12 + strip.size();
    frame.push_back(static_cast<std::uint8_t>(size >> 16));
    be16(frame, static_cast<std::uint32_t>(size));
    be16(frame, 8);
    be16(frame, 4);
    be16(frame, 1);
    be16(frame, 0x1000);
    be16(frame, static_cast<std::uint32_t>(12 + strip.size()));
    be16(frame, 0);
    be16(frame, 0);
    be16(frame, 4);
    be16(frame, 8);
    frame.insert(frame.end(), strip.begin(), strip.end());
    return frame;
}

Bytes riffChunk(const std::string& id, const Bytes& body) {
    Bytes b;
    putText(b, id);
    put32(b, static_cast<std::uint32_t>(body.size()));
    b.insert(b.end(), body.begin(), body.end());
    if (body.size() & 1) {
        b.push_back(0);
    }
    return b;
}

Bytes list(const std::string& type, const std::vector<Bytes>& parts) {
    Bytes body;
    putText(body, type);
    for (const auto& p : parts) {
        body.insert(body.end(), p.begin(), p.end());
    }
    return riffChunk("LIST", body);
}

Bytes avi(const Bytes& frame) {
    Bytes avih(56, 0);
    avih[0] = 0x40; // 0x9c40: 40000 microseconds a frame
    avih[1] = 0x9c;
    Bytes vstrh(56, 0);
    std::copy_n("vidscvid", 8, vstrh.begin());
    Bytes vstrf;
    put32(vstrf, 40);
    put32(vstrf, 8);
    put32(vstrf, 4);
    put16(vstrf, 1);
    put16(vstrf, 24);
    putText(vstrf, "cvid");
    vstrf.resize(40, 0);
    Bytes astrh(56, 0);
    std::copy_n("auds", 4, astrh.begin());
    Bytes astrf;
    put16(astrf, 1);
    put16(astrf, 1);
    put32(astrf, 22050);
    put32(astrf, 44100);
    put16(astrf, 2);
    put16(astrf, 16);
    Bytes sound;
    put16(sound, 1234);
    put16(sound, 4321);
    const Bytes hdrl = list("hdrl", {riffChunk("avih", avih), list("strl", {riffChunk("strh", vstrh), riffChunk("strf", vstrf)}),
                                     list("strl", {riffChunk("strh", astrh), riffChunk("strf", astrf)})});
    const Bytes movi = list("movi", {riffChunk("00dc", frame), riffChunk("01wb", sound), riffChunk("00dc", frame)});
    Bytes body;
    putText(body, "AVI ");
    body.insert(body.end(), hdrl.begin(), hdrl.end());
    body.insert(body.end(), movi.begin(), movi.end());
    return riffChunk("RIFF", body);
}

} // namespace

NGS_TEST("films: AVI files and Cinepak pictures") {
    const Bytes frame = cinepakFrame();
    const AviFile file(std::make_shared<MemorySource>(avi(frame)));
    NGS_CHECK(file.width() == 8 && file.height() == 4 && file.frameTime() == 40000 && file.frames() == 2);
    NGS_CHECK(file.frame(1) == frame);
    NGS_CHECK(file.sound().channels == 1 && file.sound().samples.size() == 2 && file.sound().samples[1] == 4321);

    CinepakDecoder d(8, 4);
    d.decode(file.frame(0));
    const Image& p = d.picture();
    // The v1 block: Y 100, U 10, V -10 gives red 80, green 105, blue 120.
    NGS_CHECK(p.at(0, 0) == 0xff506978u && p.at(3, 3) == 0xff506978u);
    // The v4 block: each 2×2 part has its four greys.
    NGS_CHECK(p.at(4, 0) == 0xff0a0a0au && p.at(5, 0) == 0xff141414u && p.at(4, 1) == 0xff1e1e1eu &&
              p.at(5, 1) == 0xff282828u && p.at(7, 3) == 0xff282828u);

    // Cut-off and wrong data is refused.
    bool threw = false;
    try {
        d.decode(ByteView(frame).first(20));
    } catch (const FormatError&) {
        threw = true;
    }
    NGS_CHECK(threw);
    threw = false;
    try {
        AviFile bad(std::make_shared<MemorySource>(bytesOf("RIFF\x04\x00\x00\x00AVI ")));
    } catch (const FormatError&) {
        threw = true;
    }
    NGS_CHECK(threw);
}

NGS_TEST("films: the game's films") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const AviFile opening(std::make_shared<FileSource>(res.loose("open.avi")));
    NGS_CHECK(opening.width() == 800 && opening.height() == 300 && opening.frames() == 1804);
    NGS_CHECK(opening.sound().frames() == 0);
    const AviFile fed(std::make_shared<FileSource>(res.loose("h.avi")));
    NGS_CHECK(fed.frames() == 576 && fed.sound().channels == 2 && fed.sound().rate == 22050);
    CinepakDecoder d(fed.width(), fed.height());
    int failed = 0;
    for (std::size_t i = 0; i < fed.frames(); ++i) {
        try {
            d.decode(fed.frame(i));
        } catch (const FormatError&) {
            ++failed;
        }
    }
    NGS_CHECK(failed == 0);
}
