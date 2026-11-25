// SPDX-License-Identifier: GPL-3.0-or-later
//
// Trade routes between an empire's colonies and the systems of races it has
// a friendship (or better) with. The numbers are our own (see
// docs/rules/economy.md#trade-routes).
#pragma once

#include "nagus/game/game.hpp"

#include <string>
#include <vector>

namespace nagus {

// One route for each full 100 million people (at least one), plus what the
// colony's structures add.
inline constexpr int kPeoplePerRoute = 100;

// The race a system belongs to for trade: its colony's owner, or the minor
// race living there; -1 for none.
int tradePartner(const Game& game, int system);
// Whether two races may trade: a friendship, affiliation or alliance.
bool mayTrade(const Game& game, int empire, int race);
// How many routes a colony may have, and how many it has.
int routeLimit(const Game& game, const Colony& colony);
int routesFrom(const Game& game, int system);
// Whether a route may be set up from one of the empire's colonies to a
// system: a partner it may trade with, a free route at the colony, and no
// route between the two yet.
bool canAddRoute(const Game& game, int empire, int from, int to);
bool addRoute(Game& game, int empire, int from, int to);
bool removeRoute(Game& game, int from, int to);
// Credits a route makes each turn, for the empire that set it up; the
// partner, when it is an empire, gets the same.
int routeIncome(const Game& game, const TradeRoute& route);

// A turn of trade: routes whose treaty or systems are gone end (the player
// is told), and the others pay. Computer players set up routes where they
// can.
void tradeTurn(Game& game, std::vector<std::string>& events);

} // namespace nagus
