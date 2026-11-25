// SPDX-License-Identifier: GPL-3.0-or-later
#include "builders.hpp"
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/resources.hpp"
#include "nagus/sound/jukebox.hpp"

#include <algorithm>

using namespace nagus;
using namespace nagus::test;

namespace {

template <class F> bool throwsFormatError(F f) {
    try {
        f();
    } catch (const FormatError&) {
        return true;
    }
    return false;
}
#define NGS_CHECK_THROWS(...) NGS_CHECK(throwsFormatError([&] { (void)(__VA_ARGS__); }))

// A mono IMA ADPCM block of 8 bytes: header, then four bytes of samples.
Bytes adpcmBlock(std::int16_t predictor, std::uint8_t index, std::array<std::uint8_t, 4> data) {
    Bytes b;
    put16(b, static_cast<std::uint16_t>(predictor));
    b.push_back(index);
    b.push_back(0);
    b.insert(b.end(), data.begin(), data.end());
    return b;
}

Bytes wav16(int channels, int rate, const std::vector<std::int16_t>& samples) {
    Bytes b;
    putText(b, "RIFF");
    put32(b, static_cast<std::uint32_t>(36 + samples.size() * 2));
    putText(b, "WAVEfmt ");
    put32(b, 16);
    put16(b, 1);
    put16(b, static_cast<std::uint32_t>(channels));
    put32(b, static_cast<std::uint32_t>(rate));
    put32(b, static_cast<std::uint32_t>(rate * channels * 2));
    put16(b, static_cast<std::uint32_t>(channels * 2));
    put16(b, 16);
    putText(b, "data");
    put32(b, static_cast<std::uint32_t>(samples.size() * 2));
    for (const auto s : samples) {
        put16(b, static_cast<std::uint16_t>(s));
    }
    return b;
}

// An archive of sfx.snd's shape: the table's place, the sounds, the table.
Bytes soundArchive(const std::vector<Bytes>& sounds, const std::vector<std::uint32_t>& lengths) {
    Bytes b(4, 0);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> at;
    for (const auto& s : sounds) {
        at.emplace_back(static_cast<std::uint32_t>(b.size()), static_cast<std::uint32_t>(s.size()));
        b.insert(b.end(), s.begin(), s.end());
    }
    const auto table = static_cast<std::uint32_t>(b.size());
    for (std::size_t i = 0; i < sounds.size(); ++i) {
        put32(b, at[i].first);
        put32(b, at[i].second);
        put32(b, lengths[i]);
        put32(b, 0);
    }
    b[0] = static_cast<std::uint8_t>(table);
    b[1] = static_cast<std::uint8_t>(table >> 8);
    return b;
}

} // namespace

NGS_TEST("sound: IMA ADPCM and WAV files") {
    // Predictor 0, step 7: nibble 4 adds 7; then step 9: nibble 12 takes 10.
    const Bytes block = adpcmBlock(0, 0, {0xc4, 0x00, 0x00, 0x00});
    const auto s = decodeImaAdpcm(block, 1, 8, 100);
    NGS_CHECK(s.size() == 9);
    NGS_CHECK(s[0] == 0 && s[1] == 7 && s[2] == -3);
    NGS_CHECK(decodeImaAdpcm(block, 1, 8, 2).size() == 2);
    // A bad step index or block size is refused.
    NGS_CHECK_THROWS(decodeImaAdpcm(adpcmBlock(0, 89, {0, 0, 0, 0}), 1, 8, 100));
    NGS_CHECK_THROWS(decodeImaAdpcm(block, 1, 6, 100));
    NGS_CHECK_THROWS(decodeImaAdpcm(block, 3, 8, 100));
    // Two channels: each block holds the left and right headers, then four
    // bytes for each in turn.
    Bytes stereo;
    put16(stereo, 100);
    put16(stereo, 0);
    put16(stereo, static_cast<std::uint16_t>(-100));
    put16(stereo, 0);
    stereo.resize(16, 0);
    const auto st = decodeImaAdpcm(stereo, 2, 16, 100);
    NGS_CHECK(st.size() == 18 && st[0] == 100 && st[1] == -100);

    const Pcm w = readWav(wav16(2, 11025, {1, -1, 300, -300}));
    NGS_CHECK(w.rate == 11025 && w.channels == 2 && w.frames() == 2 && w.samples[2] == 300);
    NGS_CHECK_THROWS(readWav(bytesOf("RIFF")));
    NGS_CHECK_THROWS(readWav(bytesOf("RIFF\x04\x00\x00\x00WAVE")));
    Bytes cut = wav16(1, 22050, {5, 6, 7, 8});
    cut.resize(cut.size() - 3); // the data runs past the end: what is there is read
    NGS_CHECK(readWav(cut).frames() == 2);
}

NGS_TEST("sound: archives and the jukebox") {
    const Bytes effect = wav16(1, kSoundRate, std::vector<std::int16_t>(100, 1000));
    const Bytes speech = adpcmBlock(500, 0, {0, 0, 0, 0});
    const Bytes archive = soundArchive({effect, speech}, {0, 18});
    auto effects = std::make_unique<SoundArchive>(std::make_shared<MemorySource>(archive));
    NGS_CHECK(effects->size() == 2);
    NGS_CHECK(effects->sound(0).frames() == 100);
    NGS_CHECK(effects->sound(1).frames() == 9 && effects->sound(1).samples[0] == 500);
    NGS_CHECK_THROWS(effects->sound(2));
    // A table pointing outside the file is refused.
    Bytes bad = archive;
    bad[0] = 0xff;
    bad[1] = 0xff;
    NGS_CHECK_THROWS(SoundArchive(std::make_shared<MemorySource>(bad)));

    // The jukebox mixes the effect over silence.
    Jukebox box(nullptr, std::move(effects));
    box.setVolumes(100, 50);
    box.playEffect(0);
    box.playEffect(99); // no such sound: nothing
    NGS_CHECK(box.soundsPlaying());
    std::vector<std::int16_t> out(2 * 150);
    box.mix(out);
    NGS_CHECK(out[0] == 500 && out[1] == 500 && out[2 * 99] == 500 && out[2 * 100] == 0);
    NGS_CHECK(!box.soundsPlaying());
    box.playMusic(3); // no music archive: silence
    box.mix(out);
    NGS_CHECK(std::all_of(out.begin(), out.end(), [](std::int16_t v) { return v == 0; }));
}

NGS_TEST("sound: the game's music, sounds and speech") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    MusicArchive music(std::make_shared<FileSource>(res.loose("music.snd")));
    NGS_CHECK(music.tracks() == 16);
    NGS_CHECK(music.pieces(0) == 5 && music.pieces(1) == 15 && music.pieces(16) == 0);
    const Pcm piece = music.piece(0, 0);
    NGS_CHECK(piece.channels == 2 && piece.rate == 22050);
    NGS_CHECK(piece.frames() > 22050 * 20 && piece.frames() < 22050 * 25);
    SoundArchive sfx(std::make_shared<FileSource>(res.loose("sfx.snd")));
    NGS_CHECK(sfx.size() == 163);
    NGS_CHECK(sfx.sound(76).channels == 1 && sfx.sound(76).frames() > 1000);
    SoundArchive speech(std::make_shared<FileSource>(res.loose("english.snd")));
    NGS_CHECK(speech.size() == 1364 && speech.sound(0).frames() == 21056);

    // The jukebox plays the main theme, one piece after the other.
    Jukebox box(res);
    box.playMusic(0);
    std::vector<std::int16_t> out(2 * 4096);
    long long energy = 0;
    for (int i = 0; i < 20; ++i) {
        box.mix(out);
        for (const auto v : out) {
            energy += std::abs(v);
        }
    }
    NGS_CHECK(energy > 0);
}
