// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ships and fleets (the original's task forces): where they are, where they
// are going and what they do there. How fast and how far ships go is our
// own approximation (see docs/rules/ships.md).
#pragma once

#include "nagus/game/economy.hpp"
#include "nagus/game/galaxy.hpp"

#include <optional>
#include <vector>

namespace nagus {

// Millions of people for each point of a colony ship's capacity (our choice).
inline constexpr int kColonistsPerCapacity = 10;

struct Ship {
    int shipClass = -1; // index into EconomyData::ships
    int hull = 0;       // what is left of its hull
};

struct Fleet {
    enum class Order : std::uint8_t { None, Colonize, Attack, Raid, Scrap };
    int id = 0;
    int owner = -1;
    Sector at;
    std::optional<Sector> destination;
    Order order = Order::None;
    // Avoids battles: it fights only when an enemy that engages finds it.
    bool avoid = false;
    std::vector<Ship> ships;
};

// Sectors a turn: by the slowest ship's role, and one more for each four
// levels of propulsion.
int fleetSpeed(const Fleet& f, const EconomyData& data, const EmpireState& empire);
// How far from the empire's nearest colony ships of a range class (0
// short, 1 medium, 2 long) may go.
int rangeSectors(int rangeClass, const EmpireState& empire);
// How far from the empire's nearest colony the fleet may go.
int fleetRange(const Fleet& f, const EconomyData& data, const EmpireState& empire);
// How far a sector is from an empire's nearest colony (a large number when
// it has none).
int distanceFromColonies(Sector s, int owner, const std::vector<Colony>& colonies, const Galaxy& galaxy);
// Steps between two sectors (diagonal steps count one).
int distance(Sector a, Sector b);

// The sectors a fleet passes through on its way, one per step, without its
// own sector.
std::vector<Sector> path(Sector from, Sector to);

// Whether the fleet has a ship that can found a colony here, and the system
// can take one: a star with room for the owner's people and no colony or
// minor race yet.
bool canColonize(const Fleet& f, const Galaxy& galaxy, const std::vector<Colony>& colonies, const EconomyData& data);

// A new colony from a colony ship: its people, the best facilities the
// empire has, a farm and a factory.
Colony foundColony(int system, const StarSystem& s, int owner, int colonists, const EmpireState& empire,
                   const EconomyData& data);

} // namespace nagus
