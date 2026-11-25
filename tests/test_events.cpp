// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/events.hpp"

#include <set>

using namespace nagus;
using namespace nagus::test;

NGS_TEST("texts: filling in their places") {
    NGS_CHECK(fillText("Immigrants flock to %s; +%d million citizens.", {"Sol", "12"}) == "Immigrants flock to Sol; +12 million citizens.");
    NGS_CHECK(fillText("%s1 Ships in sector %2 infected", {"3", "4.5"}) == "3 Ships in sector 4.5 infected");
    NGS_CHECK(fillText("A comet has struck %1 of %2, %d3 dead", {"Mars", "Sol", "7"}) == "A comet has struck Mars of Sol, 7 dead");
    NGS_CHECK(fillText(" +%d%% Empire Bonus", {"5"}) == " +5% Empire Bonus");
    NGS_CHECK(fillText("100% sure, %9 missing", {}) == "100% sure,  missing");
}

NGS_TEST("random events strike now and then, and only when chosen") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    const GalaxyData data = loadGalaxyData(res, lex);
    GameSettings settings;
    settings.randomEvents = false;
    Game quiet = newGame(settings, data, economy, 2);
    for (int t = 0; t < 200; ++t) {
        quiet.turn = t;
        NGS_CHECK(randomEvents(quiet).empty());
    }
    settings.randomEvents = true;
    Game g = newGame(settings, data, economy, 2);
    std::set<std::size_t> kinds;
    int heard = 0;
    for (int t = 0; t < 2000; ++t) {
        g.turn = t;
        for (const auto& e : randomEvents(g)) {
            NGS_CHECK(e.empire == g.player && !fillText(lex[e.text], e.values).empty());
            kinds.insert(e.text);
            ++heard;
        }
        // People come back, so the plagues have someone to strike.
        for (auto& c : g.colonies) {
            c.population = std::max(c.population, 10);
        }
    }
    // About one turn in 25 for the player.
    NGS_CHECK(heard > 30 && heard < 150);
    NGS_CHECK(kinds.size() >= 3);
}
