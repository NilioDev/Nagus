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
} // namespace text

} // namespace nagus
