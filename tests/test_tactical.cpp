// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/fleet.hpp"
#include "nagus/game/tactical.hpp"

#include <algorithm>

using namespace nagus;
using namespace nagus::test;

namespace {

int shipNamed(const EconomyData& d, int race, const char* name) {
    for (std::size_t i = 0; i < d.ships.size(); ++i) {
        if (d.ships[i].race == race && d.ships[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Sector emptySector(const Game& g) {
    for (int y = 1; y + 1 < g.galaxy.height; ++y) {
        for (int x = 1; x + 1 < g.galaxy.width; ++x) {
            if (g.galaxy.systemAt({x, y}) < 0) {
                return {x, y};
            }
        }
    }
    return {-1, -1};
}

int playerDamage(const TacticalBattle& b, int player) {
    int total = 0;
    for (std::size_t i = 0; i < b.units().size(); ++i) {
        total += b.units()[i].owner == player ? b.lastDamage()[i] : 0;
    }
    return total;
}

} // namespace

NGS_TEST("tactical combat: maneuvers, retreats, hails and the turn around it") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    // The maneuvers' names and help come from the lexicon.
    NGS_CHECK(lex[maneuverName(Maneuver::Charge)] == "charge");
    NGS_CHECK(lex[maneuverName(Maneuver::Retreat)] == "retreat");
    NGS_CHECK(lex[maneuverHelp(Maneuver::Harry)].find("close range") != std::string::npos);
    NGS_CHECK(lex[maneuverHelp(Maneuver::Hail)].find("hailing frequencies") != std::string::npos);

    GameSettings settings;
    settings.empire = 1;
    const Game start = newGame(settings, loadGalaxyData(res, lex), economy, 3);
    const int cruiser = shipNamed(economy, 3, "Attack Cruiser");
    NGS_CHECK(cruiser >= 0);
    auto setUp = [&](int mine, int theirs) {
        Game g = start;
        g.fleets.clear();
        for (auto& r : g.relations) {
            r.treaty = Treaty::War;
        }
        const Sector at = emptySector(g);
        for (int i = 0; i < mine; ++i) {
            g.addFleet(1, at, cruiser);
        }
        for (int i = 0; i < theirs; ++i) {
            g.addFleet(3, at, cruiser);
        }
        return g;
    };

    // A fight, round by round.
    {
        Game g = setUp(2, 2);
        const Sector at = emptySector(g);
        NGS_CHECK(playerBattles(g) == std::vector<Sector>{at});
        TacticalBattle b(g, at);
        NGS_CHECK(b.units().size() == 4 && b.sides().size() == 2 && b.sides()[0] == 1);
        NGS_CHECK(!b.over() && b.round() == 0);
        // Charging takes more fire than evading.
        TacticalBattle evading = b;
        b.fight(g, Maneuver::Charge);
        evading.fight(g, Maneuver::Evade);
        NGS_CHECK(b.round() == 1 && b.lastManeuver(1) == Maneuver::Charge);
        NGS_CHECK(!b.lastShots().empty());
        NGS_CHECK(playerDamage(b, 1) > playerDamage(evading, 1));
        // To the end: at most kTacticalRounds, and what was lost is written back.
        b.fightToEnd(g);
        NGS_CHECK(b.over() && b.round() <= kTacticalRounds);
        const Battle result = b.finish(g);
        int destroyed = 0;
        for (const auto& u : b.units()) {
            destroyed += u.hull <= 0 ? 1 : 0;
        }
        NGS_CHECK(result.lost[0] + result.lost[1] == destroyed);
        removeWrecks(g);
        int ships = 0;
        for (const auto& f : g.fleets) {
            ships += static_cast<int>(f.ships.size());
        }
        NGS_CHECK(ships == 4 - destroyed);
    }

    // A retreat: the ships leave for a neighbouring sector.
    {
        Game g = setUp(1, 3);
        const Sector at = emptySector(g);
        TacticalBattle b(g, at);
        b.fight(g, Maneuver::Retreat);
        NGS_CHECK(b.over());
        b.finish(g);
        removeWrecks(g);
        for (const auto& f : g.fleets) {
            if (f.owner == 1) {
                NGS_CHECK(distance(f.at, at) == 1);
            }
        }
    }

    // The computer retreats when far weaker, and hails when weaker.
    {
        Game g = setUp(4, 1);
        NGS_CHECK(TacticalBattle(g, emptySector(g)).choose(g, 3) == Maneuver::Retreat);
        g = setUp(2, 1);
        TacticalBattle b(g, emptySector(g));
        NGS_CHECK(b.choose(g, 3) == Maneuver::Hail);
        // Both hail: no battle.
        b.fight(g, Maneuver::Hail);
        NGS_CHECK(b.over() && b.avoided());
        const Battle result = b.finish(g);
        NGS_CHECK(result.lost[0] == 0 && result.lost[1] == 0);
    }

    // The turn in two halves, with the player's battle fought between them.
    {
        Game g = setUp(2, 2);
        const int turn = g.turn;
        TurnSummary summary = beginEndTurn(g);
        const auto waiting = playerBattles(g);
        NGS_CHECK(waiting.size() == 1);
        TacticalBattle b(g, waiting[0]);
        b.fightToEnd(g);
        const Battle fought = b.finish(g);
        finishEndTurn(g, summary, {fought});
        NGS_CHECK(g.turn == turn + 1);
        NGS_CHECK(summary.battles.size() == 1);
        for (const auto& f : g.fleets) {
            for (const auto& s : f.ships) {
                NGS_CHECK(s.hull > 0);
            }
        }
    }
}
