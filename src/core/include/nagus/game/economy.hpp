// SPDX-License-Identifier: GPL-3.0-or-later
//
// Colonies and what they make: people working facilities for food,
// industry, energy, research and intelligence; structures that add to
// these; the build queue that industry pays for; growth of population.
// The numbers come from the game's tables; the way they are combined is our
// own approximation (see docs/rules/economy.md).
#pragma once

#include "nagus/data/tables.hpp"
#include "nagus/game/galaxy.hpp"

#include <array>
#include <string>
#include <vector>

namespace nagus {

// What a building's bonus does (the texts of edifbnft.bst, by number).
namespace bonus {
inline constexpr int kFood = 0;
inline constexpr int kFoodPercent = 1;
inline constexpr int kEnergy = 2;
inline constexpr int kEnergyPercent = 3;
inline constexpr int kCredits = 4;
inline constexpr int kCreditsPercent = 5;
inline constexpr int kTotalCreditsPercent = 6;
inline constexpr int kResearch = 7;
inline constexpr int kResearchPercent = 8;
inline constexpr int kResearchEmpirePercent = 9;
inline constexpr int kIntel = 10;
inline constexpr int kIntelPercent = 11;
inline constexpr int kIndustry = 13;
inline constexpr int kIndustryPercent = 14;
inline constexpr int kShipExperience = 16;
inline constexpr int kDefense = 17;
inline constexpr int kShipyard = 18;
inline constexpr int kShieldPerEnergyTech = 19;
inline constexpr int kMorale = 20;
inline constexpr int kScanRange = 29;
inline constexpr int kResearchPoints = 30;
inline constexpr int kIntelPoints = 32;
inline constexpr int kMoraleToo = 36;
inline constexpr int kGroundDefense = 37;
inline constexpr int kEnergyToo = 38;
inline constexpr int kFoodToo = 42;
inline constexpr int kSpecialShipyard = 43;
inline constexpr int kGroundCombat = 44;
} // namespace bonus

struct EconomyData {
    std::vector<Tech> techs;
    std::vector<Building> buildings;
    std::vector<ShipClass> ships;
    std::vector<GrowthBracket> growth;
    std::vector<Habitat> habitats;
};

struct BuildItem {
    enum class Kind : std::uint8_t { Facility, Upgrade, Structure, Ship };
    Kind kind = Kind::Facility;
    int building = -1; // index into EconomyData::buildings, or EconomyData::ships for a ship
    bool operator==(const BuildItem&) const = default;
};

struct Structure {
    int building = -1;
    bool on = true; // switched on (uses energy)
};

struct Colony {
    int system = -1; // index into Galaxy::systems
    int owner = -1;  // the empire (0-4) or minor race
    bool home = false; // the owner's home system
    int population = 0;    // millions
    int maxPopulation = 0; // millions
    int food = 0;          // food in store
    // For each Output: the facility built there (a building index, or -1),
    // how many and how many have people working them.
    std::array<int, kOutputs> facility{-1, -1, -1, -1, -1};
    std::array<int, kOutputs> facilities{};
    std::array<int, kOutputs> working{};
    std::vector<Structure> structures;
    std::vector<BuildItem> queue;
    int progress = 0; // industry put into the first item of the queue
};

// What a colony makes in one turn.
struct ColonyOutput {
    std::array<int, kOutputs> made{};
    int foodNeeded = 0;
    int energyNeeded = 0; // by the structures switched on
    int credits = 0;
    int scanRange = 1;
    bool shipyard = false;
};

// The fields the player puts research into; sociology follows them.
inline constexpr int kResearchFields = 6;
inline constexpr int kMaxTechLevel = 10;

// What an empire's agents do against another empire: shares (percent) of
// the intelligence for operations, and what they aim at.
enum class IntelFocus : std::uint8_t { General, Science, Economy, Military };
inline constexpr int kIntelFocuses = 4;
struct IntelPlan {
    int espionage = 0;
    int sabotage = 0;
    IntelFocus focus = IntelFocus::General;
};

struct EmpireState {
    int credits = 0;
    std::array<int, kTechFields> tech{};     // the level reached in each field
    std::array<int, kTechFields> progress{}; // research points towards the next level
    std::array<int, kResearchFields> share{17, 17, 17, 17, 16, 16}; // percent of research for each field
    std::array<bool, kResearchFields> locked{};                     // shares kept when others change
    int researchPoints = 0;              // gathered this turn
    int intelPoints = 0;
    // Intelligence: the share kept for security, the plan against each
    // other empire, and the points gathered towards the next operation.
    int security = 100;
    std::array<IntelPlan, 5> intel{};
    std::array<int, 5> espionagePool{};
    std::array<int, 5> sabotagePool{};
    int lastIntel = 0;    // intelligence last turn, for showing
    int lastSecurity = 0; // of which for security
};

// How many people (millions) a system can hold for an empire or minor race.
int systemRoom(const StarSystem& s, int race, const EconomyData& data);
// The room a system would have with all its planets terraformed.
int systemRoomTerraformed(const StarSystem& s, int race, const EconomyData& data);
// What a planet holds of a race's people (millions), when terraformed.
int planetRoom(const Planet& p, int race, const EconomyData& data);

// The best facility an empire can build for an output (a building index, or -1).
int bestFacility(Output o, const EmpireState& empire, int race, const EconomyData& data);

// How many facilities a colony can have in all.
int facilityLimit(const Colony& c);

ColonyOutput colonyOutput(const Colony& c, const EconomyData& data);

// What the colony can put in its build queue now.
std::vector<BuildItem> buildOptions(const Colony& c, const EmpireState& empire, const StarSystem& s,
                                    const EconomyData& data);
std::uint32_t itemCost(const BuildItem& item, const Colony& c, const EconomyData& data);
std::string itemName(const BuildItem& item, const EconomyData& data);
// Whether an empire can build a ship class (at a colony with a shipyard).
bool canBuildShip(const ShipClass& s, int race, const EmpireState& empire);

// Credits to finish the first item of the queue at once.
int buyCost(const Colony& c, const EconomyData& data);

// Puts people to work: up to `count` facilities of an output.
void setWorking(Colony& c, Output o, int count);

// Puts people without work on the facilities that need them most: farms
// while food is short, power plants while energy is short, then industry,
// research and intelligence.
void assignWorkers(Colony& c, const EconomyData& data);

// A colony on an empire's home system at the start of the game.
Colony homeColony(int system, const StarSystem& s, int empire, const EmpireState& state, const EconomyData& data);

// One turn of a colony: making things, building, growing. Returns the names
// of what was finished.
struct TurnReport {
    std::vector<std::string> finished;
    std::vector<int> ships; // ship classes built
    int credits = 0;
    int research = 0;
    int intel = 0;
    bool starving = false;
};
TurnReport colonyTurn(Colony& c, const EconomyData& data);

} // namespace nagus
