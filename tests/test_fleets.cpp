// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/combat.hpp"
#include "nagus/game/game.hpp"
#include "nagus/game/save.hpp"

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

NGS_TEST("fleet orders: redeploy, scrap, avoid and raid") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    GameSettings settings;
    settings.empire = 1;
    settings.randomEvents = false;
    Game g = newGame(settings, loadGalaxyData(res, lex), economy, 3);
    g.fleets.clear();
    int warship = -1;
    for (std::size_t i = 0; i < economy.ships.size(); ++i) {
        if (economy.ships[i].race == 1 && isArmed(economy.ships[i]) && warship < 0) {
            warship = static_cast<int>(i);
        }
    }
    NGS_CHECK(warship >= 0);
    const Sector home = g.galaxy.systems[static_cast<std::size_t>(g.galaxy.homes[1])].sector;

    // Split and merge.
    const int a = g.addFleet(1, home, warship).id;
    g.fleet(a)->ships.push_back(g.fleet(a)->ships.front());
    g.fleet(a)->ships.push_back(g.fleet(a)->ships.front());
    const int b = splitShip(g, a, 0);
    NGS_CHECK(b > 0 && g.fleet(a)->ships.size() == 2 && g.fleet(b)->ships.size() == 1 && g.fleet(b)->at == home);
    NGS_CHECK(splitShip(g, b, 0) == -1); // the last ship stays
    NGS_CHECK(mergeFleets(g, b, a));
    NGS_CHECK(g.fleet(a) == nullptr && g.fleet(b)->ships.size() == 3);
    const int other = g.addFleet(0, home, warship).id;
    NGS_CHECK(!mergeFleets(g, b, other)); // not the same owner

    // Avoiding: no battle unless the other side engages.
    for (auto& r : g.relations) {
        r.treaty = Treaty::War;
    }
    const Sector empty{0, 0};
    g.fleet(b)->at = empty;
    g.fleet(other)->at = empty;
    orderAvoid(g, b, true);
    orderAvoid(g, other, true);
    NGS_CHECK(!battleStarts(g, combatUnits(g, empty)));
    orderAvoid(g, other, false);
    NGS_CHECK(battleStarts(g, combatUnits(g, empty)));

    // Save and load keep the orders.
    orderScrap(g, other);
    const Game back = loadGame(saveGame(g, {}), economy);
    NGS_CHECK(back.fleet(b)->avoid && back.fleet(other)->order == Fleet::Order::Scrap);

    // Scrapped at the end of the turn.
    endTurn(g);
    NGS_CHECK(g.fleet(other) == nullptr && g.fleet(b) != nullptr);

    // Raiding an enemy colony takes credits.
    const Sector cardassia = g.galaxy.systems[static_cast<std::size_t>(g.galaxy.homes[0])].sector;
    g.fleets.clear();
    const int raider = g.addFleet(1, cardassia, warship).id;
    orderAvoid(g, raider, true);
    NGS_CHECK(canRaid(g, *g.fleet(raider)) && orderRaid(g, raider));
    g.empires[0].credits = 1000;
    // No defences, so the raider stays.
    g.colonyAt(g.galaxy.homes[0])->structures.clear();
    TurnSummary s = beginEndTurn(g);
    finishEndTurn(g, s, {});
    NGS_CHECK(g.fleet(raider) != nullptr && g.fleet(raider)->order == Fleet::Order::None);
    NGS_CHECK(std::any_of(s.events.begin(), s.events.end(),
                          [](const std::string& e) { return e.starts_with("Raid on") && e.ends_with(": 25 credits taken"); }));
}
