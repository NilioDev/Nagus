// SPDX-License-Identifier: GPL-3.0-or-later
//
// The galaxy: a grid of sectors, some holding a star system or another
// object (nebula, black hole, ...), and the making of a new one from the
// game set-up. See docs/rules/galaxy.md.
#pragma once

#include "nagus/data/tables.hpp"
#include "nagus/game/random.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace nagus {

enum class GalaxyShape : std::uint8_t { Irregular, Elliptical, Ring, Spiral };
enum class GalaxySize : std::uint8_t { Small, Medium, Large };
enum class MinorRaces : std::uint8_t { None, Few, Some, Many };

struct Sector {
    int x = 0;
    int y = 0;
    bool operator==(const Sector&) const = default;
};

struct StarSystem {
    Sector sector;
    int kind = 0;  // index into the map object kinds (StarObjectKind)
    std::string name;
    int race = -1; // the race whose home this is: 0-4 an empire, 5 and up a minor race
    bool star = true; // a star with planets, not a nebula, black hole or the like
};

struct Galaxy {
    int width = 0;
    int height = 0;
    std::vector<StarSystem> systems;
    std::array<int, kEmpires> homes{}; // index of each empire's home system, or -1

    // The system in a sector, or -1.
    int systemAt(Sector s) const;
    bool inside(Sector s) const { return s.x >= 0 && s.y >= 0 && s.x < width && s.y < height; }
};

// What the galaxy maker needs from the game's tables.
struct GalaxyData {
    std::vector<StarObjectKind> kinds;
    std::vector<std::string> starNames;
    std::vector<RaceInfo> races;
    std::array<std::string, kEmpires> homeNames; // the empires' home systems
};

struct GalaxySetup {
    GalaxyShape shape = GalaxyShape::Irregular;
    GalaxySize size = GalaxySize::Small;
    MinorRaces minors = MinorRaces::Some;
    std::uint32_t seed = 1;
};

// Width and height of the map in sectors.
std::pair<int, int> galaxyDimensions(GalaxySize size);
int minorRaceCount(GalaxySize size, MinorRaces minors, int available);

Galaxy makeGalaxy(const GalaxySetup& setup, const GalaxyData& data);

} // namespace nagus
