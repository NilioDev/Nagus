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

// What a colony's structures add to a battle.
struct ColonyDefence {
    int firepower = 0;
    int shields = 0;
};
ColonyDefence colonyDefence(const Game& g, const Colony& c);

// One side's part in a battle: a ship, a station (ship -1, station its
// index in Game::stations), or (both -1) a colony's defences.
struct CombatUnit {
    int owner = -1;
    int fleet = -1; // index into Game::fleets
    int ship = -1;  // index into the fleet's ships
    int shipClass = -1;
    int firepower = 0;
    int shields = 0;
    int hull = 0;
    int maxHull = 0;
    int station = -1;
};
// Everyone in a sector who would take part in a battle there.
std::vector<CombatUnit> combatUnits(const Game& game, Sector at);
// Whether someone among them can shoot at an enemy.
bool battleStarts(const Game& game, const std::vector<CombatUnit>& units);
// The battle's sides and their ships, with nothing lost yet.
Battle battleSides(const std::vector<CombatUnit>& units, Sector at);
// The same mixing of numbers the battles use for chance.
std::uint32_t combatRandom(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d);

// Every sector where enemies meet and one of them can shoot, but those in
// skip (fought already). Destroyed ships are removed.
std::vector<Battle> fightBattles(Game& game, const std::vector<Sector>& skip = {});
// Removes destroyed ships, and fleets with no ships left.
void removeWrecks(Game& game);

// Fleets told to attack a system: warships bombard it, troop transports
// land and fight the people. Needs no armed defenders left in the sector.
std::vector<Assault> attackSystems(Game& game);

// Ships mend: fully at the owner's colonies and starbases, a little
// elsewhere. Stations mend a little each turn.
void repairShips(Game& game);

} // namespace nagus
