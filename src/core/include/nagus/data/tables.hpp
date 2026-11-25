// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game's tables of races, stars, home systems and technologies. See
// docs/formats/tables.md.
#pragma once

#include "nagus/util/bytes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nagus {

// The five major empires, in the order the tables use.
enum class Empire : std::uint8_t { Cardassian, Federation, Ferengi, Klingon, Romulan };
inline constexpr int kEmpires = 5;

// A race (race.rst): the five empires first, then the minor races.
struct RaceInfo {
    std::string name;
    std::string picture;
};
std::vector<RaceInfo> readRaces(ByteView data);

// The kinds of objects on the galaxy map (objstruc.smt), with the share of
// map objects that are of that kind. The code names the map pictures:
// "<code>map.tga" and "<code>maps.tga".
struct StarObjectKind {
    std::string code;
    float share = 0;
};
std::vector<StarObjectKind> readStarObjectKinds(ByteView data);

// Names for star systems (starname.bin).
std::vector<std::string> readStarNames(ByteView data);

// The planets of the empires' home systems (meplanet.bin).
struct HomePlanet {
    std::string name;
    std::string race;
    std::uint32_t kind = 0; // not yet understood: 0, 1 or 10
    std::uint32_t star = 0; // not yet understood: the same for a whole system
};
std::vector<HomePlanet> readHomePlanets(ByteView data);

// The technologies (techtree.tec), in fields of levels 0 to 10.
enum class TechField : std::uint8_t { Biotech, Computers, Construction, Energy, Propulsion, Weapons, Sociology };
inline constexpr int kTechFields = 7;
struct Tech {
    std::string name;
    TechField field = TechField::Biotech;
    int level = 0;
    std::uint32_t cost = 0;        // research points
    std::uint32_t description = 0; // offset of its text in techdesc.tec
};
std::vector<Tech> readTechTree(ByteView data);

} // namespace nagus
