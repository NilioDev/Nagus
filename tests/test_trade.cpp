// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/diplomacy.hpp"
#include "nagus/game/save.hpp"
#include "nagus/game/trade.hpp"

#include <algorithm>

using namespace nagus;
using namespace nagus::test;

namespace {

void setTreaty(Game& g, int a, int b, Treaty t) {
    const auto races = g.races.size();
    for (const auto& [e, r] : {std::pair{a, b}, std::pair{b, a}}) {
        if (e >= 0 && e < kEmpires && r >= 0 && static_cast<std::size_t>(r) < races) {
            g.relations[static_cast<std::size_t>(e) * races + static_cast<std::size_t>(r)].treaty = t;
        }
    }
}

} // namespace

NGS_TEST("trade routes: with friends, as many as the colony's people allow, paying both ends") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    GameSettings settings;
    settings.empire = 1;
    settings.randomEvents = false;
    Game g = newGame(settings, loadGalaxyData(res, lex), loadEconomyData(res), 3);
    const int sol = g.galaxy.homes[1];
    const int qonos = g.galaxy.homes[3];
    Colony& home = *g.colonyAt(sol);
    NGS_CHECK(tradePartner(g, sol) == 1 && tradePartner(g, qonos) == 3);

    // Not without a friendship.
    setTreaty(g, 1, 3, Treaty::Neutral);
    NGS_CHECK(!mayTrade(g, 1, 3) && !canAddRoute(g, 1, sol, qonos));
    setTreaty(g, 1, 3, Treaty::Friendship);
    NGS_CHECK(mayTrade(g, 1, 3) && mayTrade(g, 3, 1));
    NGS_CHECK(!canAddRoute(g, 1, sol, sol) && !canAddRoute(g, 3, sol, qonos)); // not to itself, not another's colony

    // As many routes as the people allow.
    home.population = 150;
    home.structures.clear();
    NGS_CHECK(routeLimit(g, home) == 1);
    NGS_CHECK(addRoute(g, 1, sol, qonos) && !canAddRoute(g, 1, sol, qonos));
    int minor = -1;
    for (std::size_t i = 0; i < g.galaxy.systems.size() && minor < 0; ++i) {
        minor = g.galaxy.systems[i].race >= kEmpires && g.colonyAt(static_cast<int>(i)) == nullptr ? static_cast<int>(i) : -1;
    }
    NGS_CHECK(minor >= 0);
    const int minorRace = g.galaxy.systems[static_cast<std::size_t>(minor)].race;
    setTreaty(g, 1, minorRace, Treaty::Friendship);
    NGS_CHECK(tradePartner(g, minor) == minorRace && !canAddRoute(g, 1, sol, minor)); // no free route
    home.population = 250;
    NGS_CHECK(routeLimit(g, home) == 2 && addRoute(g, 1, sol, minor));

    // Each route pays both ends (an empire partner); the Klingons have no
    // free route of their own.
    const TradeRoute toKlingons{sol, qonos};
    const int income = routeIncome(g, toKlingons);
    NGS_CHECK(income == (250 + g.colonyAt(qonos)->population) / 20);
    for (auto& c : g.colonies) {
        if (c.owner == 3) {
            c.population = 0; // no routes of their own
        }
    }
    g.colonyAt(qonos)->population = 1;
    const int before1 = g.empires[1].credits;
    const int before3 = g.empires[3].credits;
    std::vector<std::string> events;
    tradeTurn(g, events);
    const int paid = routeIncome(g, toKlingons) + routeIncome(g, {sol, minor});
    NGS_CHECK(g.empires[1].credits == before1 + paid);
    NGS_CHECK(g.empires[3].credits == before3 + routeIncome(g, toKlingons));

    // Saved and loaded.
    const Game back = loadGame(saveGame(g, {}), g.economy);
    NGS_CHECK(back.tradeRoutes == g.tradeRoutes);

    // A route ends with the friendship, and the player is told.
    setTreaty(g, 1, 3, Treaty::War);
    events.clear();
    tradeTurn(g, events);
    NGS_CHECK(g.tradeRoutes.size() == 1 && g.tradeRoutes[0].to == minor);
    NGS_CHECK(std::any_of(events.begin(), events.end(), [](const std::string& e) { return e.starts_with("Trade route") && e.ends_with("ended"); }));
    // Fewer people, fewer routes; ending one by hand works too.
    NGS_CHECK(removeRoute(g, sol, minor) && g.tradeRoutes.empty());
}

NGS_TEST("computer players set up trade routes with their friends") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    GameSettings settings;
    settings.empire = 1;
    settings.randomEvents = false;
    Game g = newGame(settings, loadGalaxyData(res, lex), loadEconomyData(res), 3);
    setTreaty(g, 0, 2, Treaty::Friendship);
    std::vector<std::string> events;
    tradeTurn(g, events);
    const int cardassia = g.galaxy.homes[0];
    NGS_CHECK(routesFrom(g, cardassia) == routeLimit(g, *g.colonyAt(cardassia)));
    NGS_CHECK(std::all_of(g.tradeRoutes.begin(), g.tradeRoutes.end(), [&](const TradeRoute& r) {
        return tradePartner(g, r.to) == 2 || tradePartner(g, r.to) == 0;
    }));
    // None for the player, who sets up their own.
    NGS_CHECK(routesFrom(g, g.galaxy.homes[1]) == 0);
}
