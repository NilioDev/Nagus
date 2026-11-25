// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/game.hpp"

#include <algorithm>

using namespace nagus;
using namespace nagus::test;

NGS_TEST("paths go one sector a step, straight to the end") {
    const auto p = path({0, 0}, {4, 2});
    NGS_CHECK(p.size() == 4);
    NGS_CHECK(p.back() == (Sector{4, 2}));
    Sector at{0, 0};
    for (const Sector s : p) {
        NGS_CHECK(distance(at, s) == 1);
        at = s;
    }
    NGS_CHECK(path({3, 3}, {3, 3}).empty());
    NGS_CHECK(distance({1, 5}, {4, 1}) == 4);
}

NGS_TEST("the game's ships: tables, the start, moving and a new colony") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    NGS_CHECK(economy.ships.size() == 125);
    const ShipClass& first = economy.ships[0];
    NGS_CHECK(first.name == "Strike Cruiser" && first.className == "Keldon" && first.race == 0);
    NGS_CHECK(first.role == ShipRole::StrikeCruiser && first.cost == 776 && first.hull == 240);
    NGS_CHECK(economy.ships[1].upgradesFrom == 0 && economy.ships[2].role == ShipRole::Colony);
    NGS_CHECK(economy.ships[22].race == 1 && economy.ships[22].tech[0] == 1);
    NGS_CHECK(economy.ships[10].range == 2 && economy.ships[10].scan == 1);

    GameSettings settings;
    settings.empire = 1;
    Game g = newGame(settings, loadGalaxyData(res, lex), economy, 11);
    // Every empire starts with a colony ship and a scout at home.
    NGS_CHECK(g.fleets.size() == 2 * kEmpires);
    const Sector home = g.galaxy.systems[static_cast<std::size_t>(g.galaxy.homes[1])].sector;
    const auto mine = g.fleetsAt(home, 1);
    NGS_CHECK(mine.size() == 2);
    // The Federation can build its colony ship at home.
    const Colony& sol = *g.colonyAt(g.galaxy.homes[1]);
    const auto options = buildOptions(sol, g.empires[1], g.galaxy.systems[static_cast<std::size_t>(sol.system)], g.economy);
    NGS_CHECK(std::any_of(options.begin(), options.end(), [](const BuildItem& i) { return i.kind == BuildItem::Kind::Ship; }));

    // The colony ship goes to the nearest free star with room, and founds a colony.
    int colonyFleet = -1;
    for (const int id : mine) {
        if (g.economy.ships[static_cast<std::size_t>(g.fleet(id)->ships[0].shipClass)].role == ShipRole::Colony) {
            colonyFleet = id;
        }
    }
    NGS_CHECK(colonyFleet > 0);
    int target = -1;
    for (std::size_t i = 0; i < g.galaxy.systems.size(); ++i) {
        const StarSystem& s = g.galaxy.systems[i];
        if (s.star && s.race < 0 && systemRoom(s, 1, g.economy) > 0 &&
            (target < 0 || distance(home, s.sector) < distance(home, g.galaxy.systems[static_cast<std::size_t>(target)].sector))) {
            target = static_cast<int>(i);
        }
    }
    NGS_CHECK(target >= 0);
    const Sector to = g.galaxy.systems[static_cast<std::size_t>(target)].sector;
    NGS_CHECK(!orderMove(g, colonyFleet, {home.x + 40, home.y}));
    NGS_CHECK(distance(home, to) <= fleetRange(*g.fleet(colonyFleet), g.economy, g.empires[1]));
    {
        NGS_CHECK(orderMove(g, colonyFleet, to));
        for (int t = 0; t < 20 && g.fleet(colonyFleet)->destination; ++t) {
            endTurn(g);
        }
        NGS_CHECK(g.fleet(colonyFleet)->at == to);
        NGS_CHECK(orderColonize(g, colonyFleet));
        const TurnSummary s = endTurn(g);
        NGS_CHECK(s.colonized.size() == 1 && g.fleet(colonyFleet) == nullptr);
        const Colony* c = g.colonyAt(target);
        NGS_CHECK(c != nullptr && c->owner == 1 && c->population > 0);
    }
}
