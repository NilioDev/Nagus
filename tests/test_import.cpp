// SPDX-License-Identifier: GPL-3.0-or-later
#include "builders.hpp"
#include "test.hpp"

#include "nagus/data/cd_stream.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game_files.hpp"

#include <filesystem>
#include <fstream>
#include <random>

using namespace nagus;
using namespace nagus::test;
namespace fs = std::filesystem;

namespace {

fs::path freshFolder(const std::string& name) {
    static std::mt19937 rng{std::random_device{}()};
    const fs::path p = fs::temp_directory_path() / ("nagus-test-" + name + "-" + std::to_string(rng()));
    fs::remove_all(p);
    fs::create_directories(p);
    return p;
}

void writeFile(const fs::path& p, const Bytes& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

// A stand-in for the game's resource archive.
Bytes fakeResources() { return makeZip({{"lexicon.dic", makeLexicon({"Accept", "Cancel"})}, {"x.tga", bytesOf("x")}}); }

Bytes fakeImage(bool raw) {
    Bytes big(5000);
    for (std::size_t i = 0; i < big.size(); ++i) {
        big[i] = static_cast<std::uint8_t>(i * 7);
    }
    return makeIso({{"MUSIC.SND", big}, {"SETUP.EXE", bytesOf("not wanted")}}, "ENGLISH",
                   {{"STBOF.RES", fakeResources()}, {"OPEN.AVI", bytesOf("movie")}}, raw);
}

Bytes fakePatch() {
    return makeZip({{"patch.lst", bytesOf("lexicon.dic\r\nminor6.tdb\r\n")},
                    {"lexicon.dic", makeLexicon({"Accept", "Cancel", "Patched"})},
                    {"minor6.tdb", bytesOf("minor")},
                    {"setup.exe", bytesOf("ignored")}});
}

} // namespace

NGS_TEST("CD stream reader finds files in 2048-byte and raw images") {
    for (const bool raw : {false, true}) {
        const Bytes image = fakeImage(raw);
        std::map<std::string, Bytes> got;
        CdStreamReader reader([](const std::string& path, std::uint32_t) { return Importer::wanted(path); },
                              [&](const std::string& path, std::uint32_t, std::uint32_t offset, ByteView piece) {
                                  Bytes& b = got[path];
                                  NGS_CHECK(b.size() == offset);
                                  b.insert(b.end(), piece.begin(), piece.end());
                              });
        // Fed in odd-sized pieces, as from a zip.
        for (std::size_t at = 0; at < image.size(); at += 1000) {
            reader.feed(ByteView(image).subspan(at, std::min<std::size_t>(1000, image.size() - at)));
        }
        reader.finish();
        NGS_CHECK(got.size() == 3);
        NGS_CHECK(got["ENGLISH/STBOF.RES"] == fakeResources());
        NGS_CHECK(got["ENGLISH/OPEN.AVI"] == bytesOf("movie"));
        NGS_CHECK(got["MUSIC.SND"].size() == 5000);
        NGS_CHECK(got.count("SETUP.EXE") == 0);
    }
}

NGS_TEST("CD stream reader refuses what is not a CD image") {
    CdStreamReader reader([](const std::string&, std::uint32_t) { return true; },
                          [](const std::string&, std::uint32_t, std::uint32_t, ByteView) {});
    bool threw = false;
    try {
        reader.feed(Bytes(40 * 2048, 0));
        reader.finish();
    } catch (const FormatError&) {
        threw = true;
    }
    NGS_CHECK(threw);
}

NGS_TEST("import from a zipped CD image and the zipped patch") {
    const fs::path lib = freshFolder("lib");
    Importer importer(lib);
    NGS_CHECK(!Importer::state(lib).game);
    const Bytes zipped = makeZip({{"Game (USA).bin", fakeImage(true)}, {"Game (USA).cue", bytesOf("FILE")}});
    NGS_CHECK(importer.importFile("game.zip", std::make_shared<MemorySource>(zipped)) == Found::Game);
    NGS_CHECK(Importer::state(lib).game);
    NGS_CHECK(!Importer::state(lib).patch);
    NGS_CHECK(fs::exists(Importer::baseDir(lib) / "music.snd"));
    NGS_CHECK(fs::exists(Importer::baseDir(lib) / "open.avi"));
    NGS_CHECK(!fs::exists(Importer::baseDir(lib) / "setup.exe"));

    NGS_CHECK(importer.importFile("botf102e.zip", std::make_shared<MemorySource>(fakePatch())) == Found::Patch);
    NGS_CHECK(Importer::state(lib).patch);
    const Resources res(lib);
    NGS_CHECK(res.file("minor6.tdb") == bytesOf("minor"));
    NGS_CHECK(res.file("LEXICON.DIC") == makeLexicon({"Accept", "Cancel", "Patched"}));
    NGS_CHECK(res.has("x.tga"));
    NGS_CHECK(!res.has("setup.exe"));
    fs::remove_all(lib);
}

NGS_TEST("import from a folder with the CD's files and the patch") {
    const fs::path data = freshFolder("data");
    writeFile(data / "BOTF" / "ENGLISH" / "STBOF.RES", fakeResources());
    writeFile(data / "BOTF" / "sfx.snd", bytesOf("sounds"));
    writeFile(data / "patch" / "PATCH.LST", bytesOf("lexicon.dic minor6.tdb"));
    writeFile(data / "patch" / "LEXICON.DIC", makeLexicon({"A"}));
    writeFile(data / "patch" / "minor6.tdb", bytesOf("m"));
    const fs::path lib = freshFolder("lib");
    NGS_CHECK(Importer(lib).importFolder(data) == Found::GameAndPatch);
    const LibraryState s = Importer::state(lib);
    NGS_CHECK(s.game && s.patch);
    NGS_CHECK(fs::exists(Importer::baseDir(lib) / "sfx.snd"));
    fs::remove_all(data);
    fs::remove_all(lib);
}

NGS_TEST("import from a plain CD image file") {
    const fs::path lib = freshFolder("lib");
    NGS_CHECK(Importer(lib).importFile("game.iso", std::make_shared<MemorySource>(fakeImage(false))) == Found::Game);
    NGS_CHECK(Importer::state(lib).game);
    fs::remove_all(lib);
}

NGS_TEST("import ignores files that are not the game") {
    const fs::path lib = freshFolder("lib");
    Importer importer(lib);
    NGS_CHECK(importer.importFile("photo.zip", std::make_shared<MemorySource>(makeZip({{"a.jpg", bytesOf("x")}}))) ==
              Found::Nothing);
    NGS_CHECK(importer.importFile("notes.txt", std::make_shared<MemorySource>(bytesOf("hello"))) == Found::Nothing);
    NGS_CHECK(!Importer::state(lib).game);
    fs::remove_all(lib);
}

NGS_TEST("a patch list cannot name files outside the library") {
    const fs::path lib = freshFolder("lib");
    const Bytes evil = makeZip({{"patch.lst", bytesOf("../evil.dic lexicon.dic")},
                                {"lexicon.dic", makeLexicon({"A"})},
                                {"../evil.dic", bytesOf("x")}});
    bool threw = false;
    try {
        Importer(lib).importFile("evil.zip", std::make_shared<MemorySource>(evil));
    } catch (const FormatError&) {
        threw = true;
    }
    NGS_CHECK(threw);
    NGS_CHECK(!fs::exists(lib.parent_path() / "evil.dic"));
    fs::remove_all(lib);
}
