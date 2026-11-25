// SPDX-License-Identifier: GPL-3.0-or-later
//
// A game in progress: the galaxy, the player's empire and the turn.
#pragma once

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/economy.hpp"
#include "nagus/game/galaxy.hpp"
#include "nagus/settings.hpp"

namespace nagus {

struct Game {
    GameSettings settings;
    Galaxy galaxy;
    int player = 1; // the player's empire (see Empire)
    int turn = 1;
    // From the game's tables, for showing the galaxy.
    std::vector<std::string> kindCodes; // map object kinds (see StarObjectKind)
    std::vector<std::string> raceNames;

    EconomyData economy;
    std::vector<Colony> colonies;
    std::array<EmpireState, kEmpires> empires{};

    // The colony on a system, or null.
    Colony* colonyAt(int system);
    const Colony* colonyAt(int system) const;
};

// Ends the turn: every colony makes, builds and grows.
struct TurnSummary {
    std::vector<std::string> events; // for the player
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
} // namespace text

} // namespace nagus
