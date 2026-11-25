// SPDX-License-Identifier: GPL-3.0-or-later
//
// Battles between fleets and colonies, and attacks on systems: bombarding
// and invading. Resolved automatically; the way damage is worked out is our
// own (see docs/rules/combat.md).
#pragma once

#include "nagus/game/game.hpp"

namespace nagus {

// Whether two owners fight when they meet: when they are at war.
bool hostile(const Game& game, int a, int b);

// What a ship does to an enemy in one round, and what its shields stop.
int shipFirepower(const ShipClass& s, const EmpireState& empire);
int shipShields(const ShipClass& s, const EmpireState& empire);
bool isArmed(const ShipClass& s);

// Every sector where enemies meet and one of them can shoot. Destroyed
// ships are removed.
std::vector<Battle> fightBattles(Game& game);

// Fleets told to attack a system: warships bombard it, troop transports
// land and fight the people. Needs no armed defenders left in the sector.
std::vector<Assault> attackSystems(Game& game);

// Ships mend: fully at the owner's colonies, a little elsewhere.
void repairShips(Game& game);

} // namespace nagus
