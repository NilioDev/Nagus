// SPDX-License-Identifier: GPL-3.0-or-later
//
// A game in progress: the galaxy, the player's empire and the turn.
#pragma once

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/economy.hpp"
#include "nagus/game/fleet.hpp"
#include "nagus/game/galaxy.hpp"
#include "nagus/settings.hpp"

namespace nagus {

// Treaties between an empire and another race, from war to membership (a
// minor race joining the empire). None: they have not met.
enum class Treaty : std::uint8_t { None, War, Neutral, NonAggression, Friendship, Affiliation, Alliance, Membership };
inline constexpr int kTreaties = 8;

struct Relation {
    Treaty treaty = Treaty::None;
    int regard = 50; // how the race thinks of the empire
    int since = 0;   // the turn the treaty began
};

// A proposal from a computer player, waiting for the player's answer.
struct Offer {
    int from = -1;
    Treaty treaty = Treaty::None;
    int turn = 0;
};

// An intelligence operation, for the reports.
struct IntelReport {
    int turn = 0;
    int attacker = -1;
    int target = -1;
    bool sabotage = false;
    IntelFocus focus = IntelFocus::General;
    bool success = false;
    int value = 0;   // credits, research points or people it took
    int system = -1; // where, if at a system
};

struct Game {
    GameSettings settings;
    Galaxy galaxy;
    int player = 1; // the player's empire (see Empire)
    int turn = 1;
    // From the game's tables, for showing the galaxy.
    std::vector<std::string> kindCodes; // map object kinds (see StarObjectKind)
    std::vector<std::string> raceNames;
    std::vector<RaceInfo> races; // pictures and character, by race


    EconomyData economy;
    std::vector<Colony> colonies;
    std::array<EmpireState, kEmpires> empires{};
    std::vector<Fleet> fleets;
    int nextFleetId = 1;
    // For each sector (y * width + x), a bit for each empire that has seen it.
    std::vector<std::uint8_t> explored;
    // For each empire and race (empire * races + race): see Relation. The
    // treaty between two empires is kept both ways.
    std::vector<Relation> relations;
    std::vector<Offer> offers; // to the player
    std::vector<IntelReport> intelReports; // the latest, oldest first

    // The colony on a system, or null.
    Colony* colonyAt(int system);
    const Colony* colonyAt(int system) const;
    Fleet* fleet(int id);
    const Fleet* fleet(int id) const;
    // The fleets in a sector, of an owner or (-1) of anyone.
    std::vector<int> fleetsAt(Sector s, int owner = -1) const;
    // A new fleet of one ship.
    Fleet& addFleet(int owner, Sector at, int shipClass);
};

// Whether an empire sees what is in a sector: near its colonies (two
// sectors) and its fleets (one sector and their scan).
bool visibleTo(const Game& game, int empire, Sector s);
// Whether an empire has ever seen a sector.
bool exploredBy(const Game& game, int empire, Sector s);
// Marks what each empire sees now as explored.
void explore(Game& game);

// Sends a fleet towards a sector, if it is in range. Returns whether it goes.
bool orderMove(Game& game, int fleet, Sector to);
// Tells a fleet to found a colony where it is (on the next turn).
bool orderColonize(Game& game, int fleet);
// Whether a fleet can attack the system where it is: an enemy colony, and
// warships to bombard it or troops to invade it.
bool canAttack(const Game& game, const Fleet& fleet);
// Tells a fleet to attack the system where it is, each turn until it is
// taken or the order is changed.
bool orderAttack(Game& game, int fleet);

// A battle in a sector (see combat.hpp).
struct Battle {
    Sector at;
    std::vector<int> owners; // the sides
    std::vector<int> ships;  // ships each side had
    std::vector<int> lost;   // ships each side lost
};

// What happened when a fleet attacked a system.
struct Assault {
    int system = -1;
    int attacker = -1;
    int defender = -1;
    int killed = 0; // millions of people
    bool captured = false;
};

// Something that happened between two races, for the turn report: a
// lexicon text with %1 (race a) and %2 (race b), or one of our own (see
// diplomacy.hpp).
struct DiplomacyEvent {
    std::size_t text = 0;
    int a = -1;
    int b = -1;
};

// Ends the turn: every colony makes, builds and grows.
struct TurnSummary {
    std::vector<std::string> events;     // for the player: what the colonies finished
    std::vector<std::string> researched; // the player's new technologies
    std::vector<std::string> colonized;  // systems the player colonized
    std::vector<Battle> battles;         // those the player fought in
    std::vector<Assault> assaults;       // on or by the player
    std::vector<DiplomacyEvent> diplomacy;
    std::vector<IntelReport> intel;      // the player's operations, and those against them
};
TurnSummary endTurn(Game& game);

// Reads what the galaxy maker needs from the game's files.
GalaxyData loadGalaxyData(const Resources& resources, const Lexicon& lexicon);

EconomyData loadEconomyData(const Resources& resources);

Game newGame(const GameSettings& settings, const GalaxyData& data, const EconomyData& economy, std::uint32_t seed);

// The technology level every field starts at, by the civilization setting
// (beginning ... advanced; our choice).
inline constexpr int kStartTech[GameSettings::kCivilizations] = {1, 2, 3, 4, 5};

// The animation that shows a planet, from the names of the game's animations
// (with ".ani"). The names spell type, atmosphere, a variant and the size:
// "trol.ani" is a large terran planet with an oxygen-rich atmosphere. The
// planets of the Sol system have their own ("eart.ani").
std::string planetAnimation(const Planet& planet, const std::vector<std::string>& names);

// Lexicon numbers of the empires' home system names and of the names of the
// map object kinds (in the order of objstruc.smt).
namespace text {
inline constexpr std::size_t kHomeSystems[kEmpires] = {121, 1160, 357, 958, 1092};
inline constexpr std::size_t kObjectKinds[] = {94, 1450, 802, 810, 987, 889, 1452, 1446, 420, 96, 0, 968};
inline constexpr std::size_t kTurn = 1370;
inline constexpr std::size_t kSector = 1107; // "Sector %s"
inline constexpr std::size_t kBuild = 108;
inline constexpr std::size_t kBuildQueue = 112;   // "BUILD QUEUE"
inline constexpr std::size_t kCredits = 193;
inline constexpr std::size_t kEnergy = 279;
inline constexpr std::size_t kFood = 365;
inline constexpr std::size_t kIndustry = 528;
inline constexpr std::size_t kIntelligence = 548;
inline constexpr std::size_t kLabor = 594;
inline constexpr std::size_t kPlanets = 623;
inline constexpr std::size_t kTurns = 624;        // "%d turns"
inline constexpr std::size_t kOneTurn = 886;      // "1 turn"
inline constexpr std::size_t kMaxPop = 626;
inline constexpr std::size_t kPopulation = 919;
inline constexpr std::size_t kProduction = 926;
inline constexpr std::size_t kResearch = 996;
inline constexpr std::size_t kTradeGoods = 1364;
inline constexpr std::size_t kUnassigned = 1381;
inline constexpr std::size_t kBuildCost = 109;
inline constexpr std::size_t kMaintenance = 282;  // "Energy Maintenance Cost"
inline constexpr std::size_t kStructures = 1185;  // "STRUCTURES"
inline constexpr std::size_t kCurrentStructures = 209;
inline constexpr std::size_t kEmpty = 276;        // "empty"
inline constexpr std::size_t kUpgradeTo = 1400;   // "Upgrade to %s"
inline constexpr std::size_t kGrowthRate = 425;
inline constexpr std::size_t kColonized = 1043; // "System %s colonized"
inline constexpr std::size_t kGalSpeed = 405;   // "Gal. Speed"
inline constexpr std::size_t kGalRange = 404;   // "Gal. Range"
inline constexpr std::size_t kHullShields = 435; // "Hit Points / Shields"
inline constexpr std::size_t kRangeNames[3] = {1147, 1156, 611}; // Short, Medium, Long
inline constexpr std::size_t kShips = 1139;
inline constexpr std::size_t kCrew = 197;
inline constexpr std::size_t kAttackSystem = 651; // "ATTACK SYSTEM"
inline constexpr std::size_t kCombatIn = 167;     // "Combat occurring in sector %1"
} // namespace text

} // namespace nagus
