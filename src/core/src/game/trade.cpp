// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/trade.hpp"

#include "nagus/game/diplomacy.hpp"

#include <algorithm>

namespace nagus {

namespace {

bool validSystem(const Game& game, int system) {
    return system >= 0 && static_cast<std::size_t>(system) < game.galaxy.systems.size();
}

int bonusOf(const Game& game, const Colony& c, int which) {
    int total = 0;
    for (const auto& st : c.structures) {
        if (st.on && st.building >= 0 && static_cast<std::size_t>(st.building) < game.economy.buildings.size()) {
            const Building& b = game.economy.buildings[static_cast<std::size_t>(st.building)];
            total += b.bonus == which ? b.amount : 0;
        }
    }
    return total;
}

// The people at a system, for trade: a colony's, or the room a minor race
// has on its home.
int people(const Game& game, int system) {
    if (const Colony* c = game.colonyAt(system)) {
        return c->population;
    }
    const StarSystem& s = game.galaxy.systems[static_cast<std::size_t>(system)];
    return s.race >= 0 ? systemRoom(s, s.race, game.economy) / 2 : 0;
}

} // namespace

int tradePartner(const Game& game, int system) {
    if (!validSystem(game, system)) {
        return -1;
    }
    if (const Colony* c = game.colonyAt(system)) {
        return c->owner;
    }
    const int race = game.galaxy.systems[static_cast<std::size_t>(system)].race;
    return race >= kEmpires ? race : -1;
}

bool mayTrade(const Game& game, int empire, int race) {
    if (empire < 0 || empire >= kEmpires || race < 0 || race == empire) {
        return false;
    }
    const Treaty t = treaty(game, empire, race);
    return t == Treaty::Friendship || t == Treaty::Affiliation || t == Treaty::Alliance;
}

int routeLimit(const Game& game, const Colony& c) {
    return std::max(1, c.population / kPeoplePerRoute) + bonusOf(game, c, bonus::kTradeRoutes);
}

int routesFrom(const Game& game, int system) {
    return static_cast<int>(
        std::count_if(game.tradeRoutes.begin(), game.tradeRoutes.end(), [&](const TradeRoute& r) { return r.from == system; }));
}

bool canAddRoute(const Game& game, int empire, int from, int to) {
    const Colony* c = validSystem(game, from) ? game.colonyAt(from) : nullptr;
    if (c == nullptr || c->owner != empire || from == to || !mayTrade(game, empire, tradePartner(game, to))) {
        return false;
    }
    const bool exists = std::any_of(game.tradeRoutes.begin(), game.tradeRoutes.end(), [&](const TradeRoute& r) {
        return (r.from == from && r.to == to) || (r.from == to && r.to == from);
    });
    return !exists && routesFrom(game, from) < routeLimit(game, *c);
}

bool addRoute(Game& game, int empire, int from, int to) {
    if (!canAddRoute(game, empire, from, to)) {
        return false;
    }
    game.tradeRoutes.push_back({from, to});
    return true;
}

bool removeRoute(Game& game, int from, int to) {
    const auto n = std::erase(game.tradeRoutes, TradeRoute{from, to});
    return n > 0;
}

int routeIncome(const Game& game, const TradeRoute& r) {
    const Colony* c = validSystem(game, r.from) && validSystem(game, r.to) ? game.colonyAt(r.from) : nullptr;
    if (c == nullptr) {
        return 0;
    }
    const int base = (people(game, r.from) + people(game, r.to)) / 20;
    return base * (100 + bonusOf(game, *c, bonus::kTradeIncomePercent)) / 100;
}

void tradeTurn(Game& game, std::vector<std::string>& events) {
    // Routes end when their colony or treaty is gone.
    std::erase_if(game.tradeRoutes, [&](const TradeRoute& r) {
        const Colony* c = validSystem(game, r.from) ? game.colonyAt(r.from) : nullptr;
        const int owner = c != nullptr ? c->owner : -1;
        const bool ok = c != nullptr && validSystem(game, r.to) && mayTrade(game, owner, tradePartner(game, r.to)) &&
                        owner >= 0 && owner < kEmpires;
        if (!ok && (owner == game.player || tradePartner(game, r.to) == game.player) && validSystem(game, r.from) &&
            validSystem(game, r.to)) {
            events.push_back("Trade route " + game.galaxy.systems[static_cast<std::size_t>(r.from)].name + " - " +
                             game.galaxy.systems[static_cast<std::size_t>(r.to)].name + " ended");
        }
        return !ok;
    });
    // Too many for a colony that shrank: the newest go.
    for (const auto& c : game.colonies) {
        while (routesFrom(game, c.system) > routeLimit(game, c)) {
            const auto it = std::find_if(game.tradeRoutes.rbegin(), game.tradeRoutes.rend(),
                                         [&](const TradeRoute& r) { return r.from == c.system; });
            game.tradeRoutes.erase(std::next(it).base());
        }
    }
    // Computer players fill their free routes with the best partners.
    for (const auto& c : game.colonies) {
        if (c.owner < 0 || c.owner >= kEmpires || game.isHuman(c.owner)) {
            continue;
        }
        while (routesFrom(game, c.system) < routeLimit(game, c)) {
            int best = -1;
            int bestIncome = 0;
            for (int s = 0; s < static_cast<int>(game.galaxy.systems.size()); ++s) {
                if (!canAddRoute(game, c.owner, c.system, s)) {
                    continue;
                }
                const int income = routeIncome(game, {c.system, s});
                if (best < 0 || income > bestIncome) {
                    best = s;
                    bestIncome = income;
                }
            }
            if (best < 0) {
                break;
            }
            addRoute(game, c.owner, c.system, best);
        }
    }
    // The routes pay both ends.
    for (const auto& r : game.tradeRoutes) {
        const int income = routeIncome(game, r);
        const int owner = game.colonyAt(r.from)->owner;
        game.empires[static_cast<std::size_t>(owner)].credits += income;
        if (const int partner = tradePartner(game, r.to); partner >= 0 && partner < kEmpires) {
            game.empires[static_cast<std::size_t>(partner)].credits += income;
        }
    }
}

} // namespace nagus
