// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game's tables of races, stars, home systems and technologies. See
// docs/formats/tables.md.
#pragma once

#include "nagus/util/bytes.hpp"

#include <array>
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
    // Our reading: how the race starts out thinking of each empire (100 for
    // itself), and how warlike it is in percent (80 to 120).
    std::array<int, 5> attitude{50, 50, 50, 50, 50};
    int aggression = 100;
    std::uint32_t description = 0; // offset of its text in racedesc.rst
};
std::vector<RaceInfo> readRaces(ByteView data);
// The text at an offset in a file of texts each ended by a zero byte, or
// nothing when the offset is outside the file.
std::string textAt(ByteView data, std::uint32_t offset);

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

// Planet types and atmospheres (environ.est): eight types (arctic, barren,
// desert, gas giant, jungle, oceanic, terran, volcanic) with the share of
// planets of each type, then five atmospheres (methane, none, oxygen rich,
// sulfuric, thin oxygen). Atmospheres are numbered on from the types: 8-12.
inline constexpr int kPlanetTypes = 8;
inline constexpr int kAtmospheres = 5;
struct PlanetKinds {
    std::vector<std::string> names; // 13: the types, then the atmospheres
    std::vector<float> shares;      // 8, one per type
};
PlanetKinds readPlanetKinds(ByteView data);

// Which atmospheres each planet type can have (habit.bin), with six numbers
// per pair that are not understood yet.
struct Habitat {
    int type = 0;
    int atmosphere = 0; // 8-12
    std::array<std::uint8_t, 6> values{};
};
std::vector<Habitat> readHabitats(ByteView data);

// Room for population on a planet by type and size (planspac.bin): nine
// steps for each pair (not yet known what picks the step). Gas giants have
// none.
struct PlanetRoom {
    int type = 0;
    int size = 0; // 0 small, 1 medium, 2 large
    std::array<std::uint32_t, 9> population{};
};
std::vector<PlanetRoom> readPlanetRoom(ByteView data);

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

// Buildings (edifice.bst): the facilities that make food, industry, energy,
// research and intelligence, and the structures a system can have.
enum class Output : std::uint8_t { Research, Industry, Food, Energy, Intelligence };
inline constexpr int kOutputs = 5;
struct Building {
    enum class Class : std::uint8_t { Facility, Structure, Unique };
    std::string name;
    std::string plural;
    int id = 0;
    int upgradesFrom = -1; // the id of the building this one replaces
    std::string picture;
    TechField field = TechField::Biotech;
    int level = 0;         // the technology level it needs
    std::uint8_t excluded = 0; // bit e set: empire e can not build it
    std::uint32_t minorRaces = 0xffffffffu; // minor races whose systems it needs (all bits: none)
    int systemKind = 9;    // the system it needs: 0 the home system, 1 the race's own, 9 any (others not known)
    int planetType = 8;    // a planet type it needs; 8 for none, above for special planets
    int bonus = 0;         // what it does (edifbnft.bst), for structures
    Class kind = Class::Facility;
    Output output = Output::Industry; // what it adds to
    std::uint32_t cost = 0;   // industry needed to build it
    std::uint32_t energy = 0; // energy it needs to work
    int workers = 0;          // people (millions) needed to work it
    int amount = 0;           // what it makes, or the size of its bonus
    std::uint32_t description = 0; // offset of its text in edifdesc.bst

    bool buildableBy(int empire) const { return empire >= 0 && empire < 8 && (excluded & (1u << empire)) == 0; }
};
std::vector<Building> readBuildings(ByteView data);

// Ship classes (shiplist.sst, with the technologies they need from
// shiptech.sst).
enum class ShipRole : std::uint8_t {
    Scout, Destroyer, Cruiser, StrikeCruiser, Command, Colony, Outpost, Starbase, Special, Transport
};
inline constexpr int kShipRoles = 10;
struct ShipClass {
    std::string name;
    std::string className;
    std::string picture;  // i_<picture>30.tga, ...60, ...120, ...170
    ShipRole role = ShipRole::Scout;
    int race = 0;         // 0-4 an empire, 5 and up a minor race, 36 monsters
    int hull = 0;
    int shields = 0;
    int beamCount = 0;    // our reading: beams fired a round
    int beamDamage = 0;   // our reading: damage of one beam
    int torpedoCount = 0;
    int torpedoDamage = 0;
    int cost = 0;         // industry
    int range = 0;        // 0 short, 1 medium, 2 long
    int scan = 0;         // added to the scan range
    int capacity = 0;     // colonists or troops
    bool cloak = false;   // has a cloaking device
    std::uint32_t description = 0; // offset of its text in shipdesc.sst
    int upgradesFrom = -1; // the class this one improves on (an index)
    std::array<int, 6> tech{}; // levels needed: biotech, computers, construction, energy, propulsion, weapons
};
std::vector<ShipClass> readShipClasses(ByteView list, ByteView tech);

// How fast population grows (ppgrowth.bin): for each bracket of population,
// a rate in ten-millionths per turn.
struct GrowthBracket {
    int from = 0;
    int to = 0;
    std::uint32_t rate = 0;
};
std::vector<GrowthBracket> readGrowth(ByteView data);

} // namespace nagus
