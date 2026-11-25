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

NGS_TEST("fleet orders: cloak, defend, wormholes and training") {
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
    int cloaker = -1;
    for (std::size_t i = 0; i < economy.ships.size(); ++i) {
        const ShipClass& c = economy.ships[i];
        if (c.race == 1 && isArmed(c) && warship < 0) {
            warship = static_cast<int>(i);
        }
        if (c.race == 4 && c.role == ShipRole::Destroyer && cloaker < 0) {
            cloaker = static_cast<int>(i);
        }
    }
    NGS_CHECK(warship >= 0 && cloaker >= 0);
    // The Romulans' ships cloak; the Federation's do not.
    NGS_CHECK(economy.ships[static_cast<std::size_t>(cloaker)].cloak && !economy.ships[static_cast<std::size_t>(warship)].cloak);
    for (auto& r : g.relations) {
        r.treaty = Treaty::War;
    }

    // Cloaked: unseen, and no battle.
    const Sector empty{0, 0};
    const int fed = g.addFleet(1, empty, warship).id;
    const int rom = g.addFleet(4, empty, cloaker).id;
    NGS_CHECK(!canCloak(g, *g.fleet(fed)) && !orderCloak(g, fed, true));
    NGS_CHECK(canCloak(g, *g.fleet(rom)) && orderCloak(g, rom, true));
    NGS_CHECK(!fleetSeen(g, 1, *g.fleet(rom)) && fleetSeen(g, 4, *g.fleet(rom)));
    NGS_CHECK(!battleStarts(g, combatUnits(g, empty)));
    orderCloak(g, rom, false);
    NGS_CHECK(fleetSeen(g, 1, *g.fleet(rom)) && battleStarts(g, combatUnits(g, empty)));
    // A ship without a cloak shows the fleet it joins.
    orderCloak(g, rom, true);
    const int rom2 = g.addFleet(4, empty, cloaker).id;
    g.fleet(rom2)->ships.front().shipClass = warship;
    NGS_CHECK(mergeFleets(g, rom, rom2) && !g.fleet(rom)->cloaked);

    // Defending: a fleet next to it is intercepted (at the chance shown).
    g.fleets.clear();
    const int guard = g.addFleet(1, {5, 5}, warship).id;
    NGS_CHECK(canDefend(g, *g.fleet(guard)) && orderDefend(g, guard));
    const int chance = interceptChance(g, *g.fleet(guard));
    NGS_CHECK(chance > 0 && chance < 100);
    int caught = 0;
    for (int t = 0; t < 20; ++t) {
        g.fleet(guard)->at = {5, 5};
        g.fleet(guard)->ships.front().hull = 100000;
        g.fleets.resize(1);
        g.addFleet(4, {6, 5}, cloaker);
        beginEndTurn(g);
        caught += g.fleet(guard)->at == (Sector{6, 5}) ? 1 : 0;
        NGS_CHECK(g.fleet(guard)->order == Fleet::Order::Defend);
    }
    NGS_CHECK(caught > 0 && caught < 20);
    // Not a cloaked one.
    g.fleet(guard)->at = {5, 5};
    g.fleets.resize(1);
    orderCloak(g, g.addFleet(4, {6, 5}, cloaker).id, true);
    for (int t = 0; t < 10; ++t) {
        beginEndTurn(g);
        NGS_CHECK(g.fleet(guard)->at == (Sector{5, 5}));
    }

    // Wormholes: two make a pair, and a third has no endpoint.
    int worm = -1;
    for (std::size_t i = 0; i < g.kindCodes.size(); ++i) {
        worm = g.kindCodes[i] == "worm" ? static_cast<int>(i) : worm;
    }
    NGS_CHECK(worm >= 0);
    for (auto& s : g.galaxy.systems) {
        s.kind = s.kind == worm ? 0 : s.kind;
    }
    std::vector<int> holes;
    for (std::size_t i = 0; i < g.galaxy.systems.size() && holes.size() < 3; ++i) {
        if (g.galaxy.systems[i].race < 0 && g.colonyAt(static_cast<int>(i)) == nullptr) {
            g.galaxy.systems[i].kind = worm;
            holes.push_back(static_cast<int>(i));
        }
    }
    NGS_CHECK(wormholeExit(g, holes[0]) == holes[1] && wormholeExit(g, holes[1]) == holes[0] && wormholeExit(g, holes[2]) == -1);
    g.fleets.clear();
    const Sector in = g.galaxy.systems[static_cast<std::size_t>(holes[0])].sector;
    const int traveller = g.addFleet(1, in, warship).id;
    NGS_CHECK(canEnterWormhole(g, *g.fleet(traveller)) && orderWormhole(g, traveller));
    endTurn(g);
    NGS_CHECK(g.fleet(traveller)->at == g.galaxy.systems[static_cast<std::size_t>(holes[1])].sector);
    g.fleet(traveller)->at = g.galaxy.systems[static_cast<std::size_t>(holes[2])].sector;
    NGS_CHECK(orderWormhole(g, traveller));
    const TurnSummary lost = endTurn(g);
    NGS_CHECK(g.fleet(traveller) == nullptr);
    NGS_CHECK(std::any_of(lost.events.begin(), lost.events.end(), [](const std::string& e) { return e.starts_with("1 ship lost"); }));

    // Training at a colony with a training facility, up to the limit.
    const int homeSystem = g.galaxy.homes[1];
    const Sector home = g.galaxy.systems[static_cast<std::size_t>(homeSystem)].sector;
    const int trainee = g.addFleet(1, home, warship).id;
    Colony& sol = *g.colonyAt(homeSystem);
    std::erase_if(sol.structures, [&](const Structure& st) { return economy.buildings[static_cast<std::size_t>(st.building)].bonus == bonus::kShipExperience; });
    NGS_CHECK(!canTrain(g, *g.fleet(trainee)));
    for (std::size_t i = 0; i < economy.buildings.size(); ++i) {
        if (economy.buildings[i].bonus == bonus::kShipExperience) {
            sol.structures.push_back({static_cast<int>(i), true});
            break;
        }
    }
    NGS_CHECK(canTrain(g, *g.fleet(trainee)) && orderTrain(g, trainee));
    endTurn(g);
    NGS_CHECK(g.fleet(trainee)->ships.front().experience == kTrainingExperience);
    for (int t = 0; t < 10; ++t) {
        endTurn(g);
    }
    NGS_CHECK(g.fleet(trainee)->ships.front().experience == kTrainingLimit && g.fleet(trainee)->order == Fleet::Order::None);
    NGS_CHECK(crewLevel(kTrainingLimit) == CrewLevel::Elite && crewLevel(0) == CrewLevel::Green && crewLevel(100) == CrewLevel::Legendary);

    // Saved and loaded.
    const int rom3 = g.addFleet(4, empty, cloaker).id;
    orderCloak(g, rom3, true);
    const Game back = loadGame(saveGame(g, {}), economy);
    NGS_CHECK(back.fleet(rom3)->cloaked && back.fleet(trainee)->ships.front().experience == kTrainingLimit);
}
