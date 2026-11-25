// SPDX-License-Identifier: GPL-3.0-or-later
#include "builders.hpp"
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/diplomacy.hpp"
#include "nagus/game/save.hpp"
#include "nagus/game/victory.hpp"
#include "nagus/ui/ending_screen.hpp"
#include "nagus/ui/fame_screen.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>

using namespace nagus;
using namespace nagus::test;

NGS_TEST("LZSS: bytes as they are and copies from the window") {
    // Flags 0b011: two bytes as they are, then a copy of 4 bytes from
    // window place 1 (where the first byte went).
    const Bytes packed{0x03, 'a', 'b', 0x20, 0x01};
    const Bytes out = unpackLzss(packed, 6);
    NGS_CHECK(std::string(out.begin(), out.end()) == "ababab");
    bool threw = false;
    try {
        (void)unpackLzss(packed, 20);
    } catch (const FormatError&) {
        threw = true;
    }
    NGS_CHECK(threw);

    // An archive of one packed and one stored file.
    Bytes a(16, 0);
    std::copy_n("alt.res", 7, a.begin());
    put32(a, 0); // list, filled in below
    put32(a, 2);
    const auto first = static_cast<std::uint32_t>(a.size());
    a.insert(a.end(), packed.begin(), packed.end());
    const auto second = static_cast<std::uint32_t>(a.size());
    putText(a, "plain");
    const auto list = static_cast<std::uint32_t>(a.size());
    auto entry = [&](const char* name, std::uint32_t at, std::uint32_t size, std::uint32_t packedSize, std::uint32_t method) {
        Bytes n(16, 0);
        std::copy_n(name, std::strlen(name), n.begin());
        a.insert(a.end(), n.begin(), n.end());
        put32(a, at);
        put32(a, 0);
        put32(a, size);
        put32(a, packedSize);
        put32(a, method);
        put32(a, 0);
    };
    entry("Packed.txt", first, 6, 5, 1);
    entry("plain.txt", second, 5, 5, 0);
    a[16] = static_cast<std::uint8_t>(list);
    const LzArchive archive(std::make_shared<MemorySource>(a));
    NGS_CHECK(archive.entries().size() == 2 && archive.find("PACKED.TXT") != nullptr);
    NGS_CHECK(archive.read(*archive.find("packed.txt")) == bytesOf("ababab"));
    NGS_CHECK(archive.read(*archive.find("plain.txt")) == bytesOf("plain"));
    Bytes bad = a;
    bad[20] = 200; // more entries than the file holds
    threw = false;
    try {
        LzArchive x(std::make_shared<MemorySource>(bad));
    } catch (const FormatError&) {
        threw = true;
    }
    NGS_CHECK(threw);
}

NGS_TEST("the end of a game: scripts and the hall of fame") {
    const auto steps = readEndingScript("VOICEOVER heogalli.wav\r\nSHOW hw.tga\r\nWAIT 520\r\nNONSENSE\r\nWAIT x\r\nVOICEWAIT\r\nEND FAMEHALL\r\n");
    NGS_CHECK(steps.size() == 5);
    NGS_CHECK(steps[0].kind == ScriptStep::Kind::Voice && steps[0].name == "heogalli.wav");
    NGS_CHECK(steps[1].kind == ScriptStep::Kind::Show && steps[2].time == 5200);
    NGS_CHECK(steps[3].kind == ScriptStep::Kind::WaitForVoice && steps[4].kind == ScriptStep::Kind::End);

    std::vector<FameEntry> fame;
    NGS_CHECK(addFame(fame, {500, "Federation", "Victory"}) == 0);
    NGS_CHECK(addFame(fame, {900, "Klingons|x", "Defeat\n"}) == 0);
    NGS_CHECK(addFame(fame, {100, "Romulans", "Retired"}) == 2);
    for (int i = 0; i < 10; ++i) {
        addFame(fame, {1000 + i, "Ferengi", ""});
    }
    NGS_CHECK(fame.size() == kFameEntries && fame.front().score == 1009);
    NGS_CHECK(addFame(fame, {1, "Cardassians", ""}) == -1);
    const auto file = std::filesystem::temp_directory_path() / "nagus-test-fame.txt";
    saveFame(file, fame);
    const auto back = loadFame(file);
    NGS_CHECK(back.size() == kFameEntries && back[9].score == fame[9].score && back[9].name == fame[9].name);
    std::filesystem::remove(file);
    NGS_CHECK(loadFame(file).empty());
}

NGS_TEST("victory: conquest, domination, allies and defeat") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    const GalaxyData data = loadGalaxyData(res, lex);
    GameSettings settings;
    settings.empire = 1;
    const Game start = newGame(settings, data, economy, 5);
    NGS_CHECK(checkEnding(start) == Ending::None && !outOfGame(start, 0));

    // The Federation takes every empire's systems.
    Game g = start;
    for (auto& c : g.colonies) {
        if (c.owner < kEmpires) {
            c.owner = 1;
        }
    }
    const auto fallen = removeFallenEmpires(g);
    NGS_CHECK(fallen.size() == 4 && g.out[0] && !g.out[1]);
    NGS_CHECK(std::none_of(g.fleets.begin(), g.fleets.end(), [](const Fleet& f) { return f.owner != 1; }));
    NGS_CHECK(checkEnding(g) == Ending::Annihilation);
    g.ending = Ending::Annihilation;
    NGS_CHECK(score(g) > score(start));
    // The ending is saved.
    const Game loaded = loadGame(saveGame(g, {}), economy);
    NGS_CHECK(loaded.ending == Ending::Annihilation && loaded.out[3]);

    // Two thirds of all systems is domination, but not in a vendetta.
    Game d = start;
    const Colony home = *d.colonyAt(d.galaxy.homes[1]);
    for (int sys = 0; sys < static_cast<int>(d.galaxy.systems.size()) && colonyShare(d, 1) < kDominationShare; ++sys) {
        bool minorsHome = false;
        for (int race = kEmpires; race < static_cast<int>(d.races.size()); ++race) {
            minorsHome = minorsHome || minorHome(d, race) == sys;
        }
        if (d.colonyAt(sys) == nullptr && !minorsHome) {
            Colony c = home;
            c.system = sys;
            c.home = false;
            d.colonies.push_back(c);
        }
    }
    NGS_CHECK(colonyShare(d, 1) >= kDominationShare && !d.out[0]);
    NGS_CHECK(checkEnding(d) == Ending::Domination);
    d.settings.victory = 2;
    NGS_CHECK(checkEnding(d) == Ending::None);

    // Team play: allied with every empire left.
    Game t = start;
    t.settings.victory = 1;
    std::fill(t.explored.begin(), t.explored.end(), std::uint8_t{0x1f});
    (void)diplomacyTurn(t, {}, {});
    for (int e = 0; e < kEmpires; ++e) {
        if (e != 1) {
            const auto races = t.races.size();
            t.relations[static_cast<std::size_t>(1) * races + static_cast<std::size_t>(e)].treaty = Treaty::Alliance;
            t.relations[static_cast<std::size_t>(e) * races + 1].treaty = Treaty::Alliance;
        }
    }
    NGS_CHECK(checkEnding(t) == Ending::Alliance);
    t.settings.victory = 0;
    NGS_CHECK(checkEnding(t) == Ending::None);

    // Losing every system is defeat.
    Game l = start;
    std::erase_if(l.colonies, [](const Colony& c) { return c.owner == 1; });
    (void)removeFallenEmpires(l);
    NGS_CHECK(checkEnding(l) == Ending::Loss);
    // The turn finds the ending.
    Game e = start;
    std::erase_if(e.colonies, [](const Colony& c) { return c.owner == 1; });
    const TurnSummary s = endTurn(e);
    NGS_CHECK(s.ending == Ending::Loss && e.ending == Ending::Loss);
    NGS_CHECK(std::find(s.fallen.begin(), s.fallen.end(), 1) != s.fallen.end());
}
