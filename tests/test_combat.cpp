// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/combat.hpp"

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

int shipsOf(const Game& g, int owner) {
    int n = 0;
    for (const auto& f : g.fleets) {
        n += f.owner == owner ? static_cast<int>(f.ships.size()) : 0;
    }
    return n;
}

} // namespace

NGS_TEST("battles, attacks on systems and invasions") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    // The table's weapons: a battleship hits harder than a scout.
    const ShipClass& battleship = economy.ships[static_cast<std::size_t>(shipNamed(economy, 0, "Battleship"))];
    const ShipClass& scout = economy.ships[static_cast<std::size_t>(shipNamed(economy, 0, "Scout"))];
    NGS_CHECK(battleship.beamCount == 14 && battleship.beamDamage == 49);
    NGS_CHECK(scout.beamCount == 2 && scout.beamDamage == 16);
    NGS_CHECK(isArmed(scout) && !isArmed(economy.ships[static_cast<std::size_t>(shipNamed(economy, 0, "Colony Ship"))]));

    GameSettings settings;
    settings.empire = 1;
    Game g = newGame(settings, loadGalaxyData(res, lex), economy, 3);
    g.fleets.clear();
    NGS_CHECK(hostile(g, 0, 1) && !hostile(g, 1, 1) && !hostile(g, 1, 7));

    // Two Klingon cruisers meet a Federation scout in empty space.
    const int home = g.galaxy.homes[1];
    const Sector sol = g.galaxy.systems[static_cast<std::size_t>(home)].sector;
    Sector empty{-1, -1};
    for (int y = 0; y < g.galaxy.height && empty.x < 0; ++y) {
        for (int x = 0; x < g.galaxy.width && empty.x < 0; ++x) {
            if (g.galaxy.systemAt({x, y}) < 0) {
                empty = {x, y};
            }
        }
    }
    g.addFleet(1, empty, shipNamed(economy, 1, "Science Vessel"));
    g.addFleet(3, empty, shipNamed(economy, 3, "Attack Cruiser"));
    g.addFleet(3, empty, shipNamed(economy, 3, "Attack Cruiser"));
    // Ships of one empire alone do not fight.
    g.addFleet(2, {empty.x, empty.y}, shipNamed(economy, 2, "Colony Ship"));
    auto battles = fightBattles(g);
    NGS_CHECK(battles.size() == 1);
    NGS_CHECK(shipsOf(g, 1) == 0 && shipsOf(g, 3) == 2);
    NGS_CHECK(battles[0].owners.size() == 3);
    // The Ferengi colony ship, unarmed, was a target too, or is still there.
    g.fleets.clear();

    // Unarmed ships of two empires do not fight.
    g.addFleet(1, empty, shipNamed(economy, 1, "Colony Ship"));
    g.addFleet(2, empty, shipNamed(economy, 2, "Colony Ship"));
    NGS_CHECK(fightBattles(g).empty() && g.fleets.size() == 2);
    g.fleets.clear();

    // Klingon warships and transports at Sol: bombard, then invade.
    Colony& earth = *g.colonyAt(home);
    const int people = earth.population;
    const int attackers = g.addFleet(3, sol, shipNamed(economy, 3, "Attack Cruiser")).id;
    NGS_CHECK(canAttack(g, *g.fleet(attackers)));
    NGS_CHECK(orderAttack(g, attackers));
    auto assaults = attackSystems(g);
    NGS_CHECK(assaults.size() == 1 && !assaults[0].captured);
    NGS_CHECK(earth.population < people && assaults[0].killed == people - earth.population);
    // Too few troops are lost against the people.
    Fleet& troops = g.addFleet(3, sol, shipNamed(economy, 3, "Assault Transport"));
    troops.order = Fleet::Order::Attack;
    assaults = attackSystems(g);
    NGS_CHECK(g.colonyAt(home)->owner == 1);
    NGS_CHECK(std::none_of(g.fleets.begin(), g.fleets.end(), [&](const Fleet& f) {
        return std::any_of(f.ships.begin(), f.ships.end(), [&](const Ship& s) { return economy.ships[static_cast<std::size_t>(s.shipClass)].role == ShipRole::Transport; });
    }));
    // Enough of them take the system.
    g.colonyAt(home)->population = 20;
    for (int i = 0; i < 3; ++i) {
        Fleet& more = g.addFleet(3, sol, shipNamed(economy, 3, "Assault Transport"));
        more.order = Fleet::Order::Attack;
    }
    assaults = attackSystems(g);
    const Colony* taken = g.colonyAt(home);
    NGS_CHECK(taken != nullptr && taken->owner == 3 && !taken->home);
    NGS_CHECK(std::any_of(assaults.begin(), assaults.end(), [](const Assault& a) { return a.captured; }));
    // An attack needs an enemy colony.
    NGS_CHECK(!canAttack(g, *g.fleet(attackers)));

    // Damaged ships mend at their own colonies.
    Fleet& hurt = g.addFleet(3, sol, shipNamed(economy, 3, "Attack Cruiser"));
    hurt.ships[0].hull = 1;
    repairShips(g);
    NGS_CHECK(g.fleets.back().ships[0].hull == economy.ships[static_cast<std::size_t>(g.fleets.back().ships[0].shipClass)].hull);
}
