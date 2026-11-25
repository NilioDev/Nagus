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

// A proposal waiting for a human player's answer.
struct Offer {
    int from = -1;
    int to = -1; // the human player it waits for
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

// How a game ended for the player (see victory.hpp). Retired: the player
// gave up from the main menu.
enum class Ending : std::uint8_t { None, Annihilation, Domination, Alliance, Loss, Retired };
inline constexpr int kEndings = 6;

// A trade route from one of an empire's colonies to another race's system
// (see trade.hpp).
struct TradeRoute {
    int from = -1; // system of the empire's colony
    int to = -1;   // system of the other race
    bool operator==(const TradeRoute&) const = default;
};

// An outpost or a starbase: a station in a sector (see docs/rules/ships.md).
struct Station {
    int owner = -1;
    Sector at;
    int shipClass = -1; // its class in EconomyData::ships (role outpost or starbase)
    int hull = 0;
};

struct Game {
    GameSettings settings;
    Galaxy galaxy;
    int player = 1; // the player's empire (see Empire)
    // In a multiplayer game, the empires other people play; the computer
    // plays the rest. The player is always human.
    std::array<bool, kEmpires> humans{};
    int turn = 1;
    // From the game's tables, for showing the galaxy.
    std::vector<std::string> kindCodes; // map object kinds (see StarObjectKind)
    std::vector<std::string> raceNames;
    std::vector<RaceInfo> races; // pictures and character, by race


    EconomyData economy;
    std::vector<Colony> colonies;
    std::array<EmpireState, kEmpires> empires{};
    std::vector<Fleet> fleets;
    std::vector<Station> stations;
    std::vector<TradeRoute> tradeRoutes;
    int nextFleetId = 1;
    // For each sector (y * width + x), a bit for each empire that has seen it.
    std::vector<std::uint8_t> explored;
    // For each empire and race (empire * races + race): see Relation. The
    // treaty between two empires is kept both ways.
    std::vector<Relation> relations;
    std::vector<Offer> offers; // to human players
    std::vector<IntelReport> intelReports; // the latest, oldest first
    std::array<bool, kEmpires> out{}; // empires that have lost every colony
    Ending ending = Ending::None;

    // Whether a person plays the empire (the player, or another in a
    // multiplayer game).
    bool isHuman(int empire) const;
    // Whether other people play in this game too.
    bool multiplayer() const;

    // The colony on a system, or null.
    Colony* colonyAt(int system);
    const Colony* colonyAt(int system) const;
    Fleet* fleet(int id);
    const Fleet* fleet(int id) const;
    // The fleets in a sector, of an owner or (-1) of anyone.
    std::vector<int> fleetsAt(Sector s, int owner = -1) const;
    // The station in a sector, or null.
    Station* stationAt(Sector s);
    const Station* stationAt(Sector s) const;
    // A new fleet of one ship.
    Fleet& addFleet(int owner, Sector at, int shipClass);
};

// How far a sector is from an empire's nearest colony or station.
int distanceFromBases(const Game& game, Sector s, int owner);
// The best class of outpost or starbase an empire can build, or -1.
int stationClass(const Game& game, int empire, ShipRole role);
// Whether a fleet with troop transports can build an outpost where it is
// (no station there yet), turn an outpost of its empire there into a
// starbase, or upgrade its empire's station there to a better class.
bool canBuildOutpost(const Game& game, const Fleet& fleet);
bool canBuildStarbase(const Game& game, const Fleet& fleet);
bool canUpgradeStation(const Game& game, const Fleet& fleet);
// Gives one of those orders; the work goes on each turn until it is done.
bool orderBuildStation(Game& game, int fleet, Fleet::Order order);
// The class the fleet's station order builds, and the industry it takes.
int stationTarget(const Game& game, const Fleet& fleet);
int stationWork(const Game& game, const Fleet& fleet);
// Industry each troop transport puts into a station a turn (our choice).
inline constexpr int kStationWorkPerTransport = 150;

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
// Whether a fleet can raid the system where it is: an enemy empire's
// colony, and a warship to raid it with.
bool canRaid(const Game& game, const Fleet& fleet);
// Tells a fleet to raid the system where it is at the end of the turn: it
// takes credits from the system's owner, if it is still there after the
// battles.
bool orderRaid(Game& game, int fleet);
// Credits a raid takes: per warship, up to what the owner has.
inline constexpr int kRaidCredits = 25;
// Tells a fleet to be dismantled at the end of the turn.
bool orderScrap(Game& game, int fleet);
// Tells a fleet to avoid battles, or to engage the enemy (the default).
bool orderAvoid(Game& game, int fleet, bool avoid);
// Whether an empire sees a fleet: one of its own, or one in a sector it sees
// that is not cloaked.
bool fleetSeen(const Game& game, int empire, const Fleet& fleet);
// Whether every ship of the fleet has a cloaking device.
bool canCloak(const Game& game, const Fleet& fleet);
// Cloaks or decloaks a fleet. A cloaked fleet is hidden from other empires
// and takes no part in battles; attacking, raiding or defending decloaks it.
bool orderCloak(Game& game, int fleet, bool cloak);
// Whether the fleet has a warship to defend its sector with.
bool canDefend(const Game& game, const Fleet& fleet);
// Tells a fleet to stay and defend its sector: after the fleets move, it
// may intercept an enemy fleet that stops next to it.
bool orderDefend(Game& game, int fleet);
// The chance (percent) that a defending fleet intercepts an enemy fleet.
int interceptChance(const Game& game, const Fleet& fleet);
// The wormhole a wormhole leads to (system indices), or -1 when it has no
// endpoint.
int wormholeExit(const Game& game, int system);
// Whether the fleet is at a wormhole.
bool canEnterWormhole(const Game& game, const Fleet& fleet);
// Tells a fleet to go through the wormhole where it is, at the end of the
// turn. Without an endpoint, the fleet is lost.
bool orderWormhole(Game& game, int fleet);
// The planet a fleet's colony ships would terraform where it is: the one
// not yet terraformed that would hold the most of its owner's people, in a
// system that is free or the owner's colony. -1 for none.
int terraformTarget(const Game& game, const Fleet& fleet);
// Tells a fleet to terraform that planet: each colony ship adds
// kTerraformPerShip a turn until the planet's terraformCost is reached.
bool orderTerraform(Game& game, int fleet);
int terraformCost(const Planet& planet);
inline constexpr int kTerraformPerShip = 20;
// Whether the fleet is at one of its empire's colonies with a working
// training facility, and a crew there can still learn.
bool canTrain(const Game& game, const Fleet& fleet);
// Tells a fleet to train its crews, each turn until it moves or they are
// as good as training makes them.
bool orderTrain(Game& game, int fleet);
// Experience a crew gains: a turn of training, and a battle it lives
// through. Training goes up to kTrainingLimit; battles beyond.
inline constexpr int kTrainingExperience = 10;
inline constexpr int kTrainingLimit = 60;
inline constexpr int kBattleExperience = 5;
// Takes one ship out of a fleet into a fleet of its own, in the same
// sector. Returns the new fleet's id, or -1.
int splitShip(Game& game, int fleet, int ship);
// Puts another fleet's ships into a fleet: both the same owner's and in
// the same sector. The other fleet is gone; the fleet keeps its orders.
bool mergeFleets(Game& game, int into, int from);

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

// A random event, for the turn report: the lexicon text with its values
// (see events.hpp).
struct GameEvent {
    std::size_t text = 0;
    std::vector<std::string> values;
    int empire = -1;
    int system = -1;
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
    std::vector<int> fallen;             // empires that lost their last colony
    std::vector<GameEvent> happenings;   // random events that struck the player
    Ending ending = Ending::None;
};
TurnSummary endTurn(Game& game);

// Sets each colony's bonuses from its system: the food and energy bonuses
// of its terraformed planets and the system's dilithium. Done at the start
// and the end of each turn, and when a game is made or loaded.
void refreshBonuses(Game& game);
// endTurn in two halves, for battles the player fights themselves between
// them: the first gives the orders, builds and moves the fleets; the second
// fights the other battles (fought: those the player fought, already
// written into the game by TacticalBattle::finish) and does the rest.
TurnSummary beginEndTurn(Game& game);
void finishEndTurn(Game& game, TurnSummary& summary, const std::vector<Battle>& fought);

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
