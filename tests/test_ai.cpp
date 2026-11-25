// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/ai.hpp"
#include "nagus/game/save.hpp"

#include <cstdio>

using namespace nagus;
using namespace nagus::test;

NGS_TEST("the computer players settle new systems and build ships") {
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
    Game g = newGame(settings, data, economy, 21);
    std::array<int, kEmpires> start{};
    for (const auto& c : g.colonies) {
        if (c.owner >= 0 && c.owner < kEmpires) {
            start[static_cast<std::size_t>(c.owner)] += 1;
        }
    }
    // At the start each empire knows only the space around its home.
    for (int e = 0; e < kEmpires; ++e) {
        const Sector home = g.galaxy.systems[static_cast<std::size_t>(g.galaxy.homes[static_cast<std::size_t>(e)])].sector;
        NGS_CHECK(exploredBy(g, e, home));
        for (int other = 0; other < kEmpires; ++other) {
            const Sector far = g.galaxy.systems[static_cast<std::size_t>(g.galaxy.homes[static_cast<std::size_t>(other)])].sector;
            NGS_CHECK(other == e || !exploredBy(g, e, far));
        }
    }
    for (int t = 0; t < 100; ++t) {
        (void)endTurn(g);
    }
    for (int e = 0; e < kEmpires; ++e) {
        int colonies = 0;
        int people = 0;
        for (const auto& c : g.colonies) {
            if (c.owner == e) {
                colonies += 1;
                people += c.population;
            }
        }
        int ships = 0;
        for (const auto& f : g.fleets) {
            ships += f.owner == e ? static_cast<int>(f.ships.size()) : 0;
        }
        const auto& st = g.empires[static_cast<std::size_t>(e)];
        std::printf("  empire %d: %d colonies (from %d), %d people, %d ships, %d credits, tech %d %d %d %d %d %d\n", e,
                    colonies, start[static_cast<std::size_t>(e)], people, ships, st.credits, st.tech[0], st.tech[1],
                    st.tech[2], st.tech[3], st.tech[4], st.tech[5]);
        if (e != g.player) {
            NGS_CHECK(colonies > start[static_cast<std::size_t>(e)]);
            NGS_CHECK(ships > 0);
        }
    }
    // The same game gives the same moves.
    Game again = newGame(settings, data, economy, 21);
    for (int t = 0; t < 100; ++t) {
        (void)endTurn(again);
    }
    NGS_CHECK(saveGame(again, {}) == saveGame(g, {}));
}
