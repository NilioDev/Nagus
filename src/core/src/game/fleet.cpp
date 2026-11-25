// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/fleet.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace nagus {

namespace {

// Our choice, by role: scout, destroyer, cruiser, strike cruiser, command,
// colony ship, outpost, starbase, special, transport.
constexpr int kRoleSpeed[kShipRoles] = {3, 2, 2, 2, 1, 1, 0, 0, 2, 1};
constexpr int kRangeSectors[3] = {3, 6, 10}; // short, medium, long
constexpr int kColonistsPerCapacity = 10;   // millions for each of a colony ship's capacity

const ShipClass* classOf(const Ship& s, const EconomyData& data) {
    return s.shipClass >= 0 && static_cast<std::size_t>(s.shipClass) < data.ships.size()
               ? &data.ships[static_cast<std::size_t>(s.shipClass)]
               : nullptr;
}

} // namespace

int distance(Sector a, Sector b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }

int fleetSpeed(const Fleet& f, const EconomyData& data, const EmpireState& empire) {
    int speed = std::numeric_limits<int>::max();
    for (const auto& s : f.ships) {
        const ShipClass* c = classOf(s, data);
        speed = std::min(speed, c != nullptr ? kRoleSpeed[static_cast<int>(c->role)] : 0);
    }
    if (f.ships.empty() || speed <= 0) {
        return 0;
    }
    return speed + empire.tech[static_cast<std::size_t>(TechField::Propulsion)] / 4;
}

int fleetRange(const Fleet& f, const EconomyData& data, const EmpireState& empire) {
    int range = std::numeric_limits<int>::max();
    for (const auto& s : f.ships) {
        const ShipClass* c = classOf(s, data);
        range = std::min(range, kRangeSectors[std::clamp(c != nullptr ? c->range : 0, 0, 2)]);
    }
    if (f.ships.empty()) {
        return 0;
    }
    return range + empire.tech[static_cast<std::size_t>(TechField::Propulsion)] / 3;
}

int distanceFromColonies(Sector s, int owner, const std::vector<Colony>& colonies, const Galaxy& galaxy) {
    int best = std::numeric_limits<int>::max() / 2;
    for (const auto& c : colonies) {
        if (c.owner == owner && c.system >= 0 && static_cast<std::size_t>(c.system) < galaxy.systems.size()) {
            best = std::min(best, distance(s, galaxy.systems[static_cast<std::size_t>(c.system)].sector));
        }
    }
    return best;
}

std::vector<Sector> path(Sector from, Sector to) {
    std::vector<Sector> steps;
    const int n = distance(from, to);
    for (int i = 1; i <= n; ++i) {
        // Evenly along the straight line, rounded to sectors.
        const int x = from.x + static_cast<int>(std::lround(static_cast<double>(to.x - from.x) * i / n));
        const int y = from.y + static_cast<int>(std::lround(static_cast<double>(to.y - from.y) * i / n));
        steps.push_back({x, y});
    }
    return steps;
}

bool canColonize(const Fleet& f, const Galaxy& galaxy, const std::vector<Colony>& colonies, const EconomyData& data) {
    const bool hasColonyShip = std::any_of(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
        const ShipClass* c = classOf(s, data);
        return c != nullptr && c->role == ShipRole::Colony;
    });
    const int at = galaxy.systemAt(f.at);
    if (!hasColonyShip || at < 0) {
        return false;
    }
    const StarSystem& s = galaxy.systems[static_cast<std::size_t>(at)];
    if (!s.star || s.race >= 0 ||
        std::any_of(colonies.begin(), colonies.end(), [&](const Colony& c) { return c.system == at; })) {
        return false;
    }
    return systemRoom(s, f.owner, data) > 0;
}

Colony foundColony(int system, const StarSystem& s, int owner, int colonists, const EmpireState& empire,
                   const EconomyData& data) {
    Colony c;
    c.system = system;
    c.owner = owner;
    c.maxPopulation = systemRoom(s, owner, data);
    c.population = std::clamp(colonists * kColonistsPerCapacity, 1, std::max(1, c.maxPopulation));
    for (int o = 0; o < kOutputs; ++o) {
        c.facility[static_cast<std::size_t>(o)] = bestFacility(static_cast<Output>(o), empire, owner, data);
    }
    for (const Output o : {Output::Food, Output::Industry}) {
        const auto i = static_cast<std::size_t>(o);
        c.facilities[i] = c.facility[i] >= 0 ? 1 : 0;
    }
    assignWorkers(c, data);
    c.food = c.population;
    return c;
}

} // namespace nagus
