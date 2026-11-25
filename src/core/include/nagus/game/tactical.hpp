// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tactical combat: a battle the player fights round by round, giving their
// ships a maneuver each round. The maneuvers and what they are good against
// come from the game's own help texts; the numbers are ours (see
// docs/rules/combat.md).
#pragma once

#include "nagus/game/combat.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nagus {

enum class Maneuver : std::uint8_t { Assault, Charge, Circle, Harry, Flyby, Strafe, Evade, Ram, Retreat, Hail };
inline constexpr int kManeuvers = 10;
// The maneuver's name and help in the lexicon.
std::size_t maneuverName(Maneuver m);
std::size_t maneuverHelp(Maneuver m);

// A tactical battle lasts at most this many rounds.
inline constexpr int kTacticalRounds = 8;

class TacticalBattle {
public:
    TacticalBattle(const Game& game, Sector at);

    Sector at() const { return at_; }
    // Everyone taking part; hull 0 is destroyed.
    const std::vector<CombatUnit>& units() const { return units_; }
    // Whether the unit has left the battle.
    bool gone(std::size_t unit) const { return gone_[unit]; }
    // The sides, the player's first.
    const std::vector<int>& sides() const { return sides_; }
    int round() const { return round_; }
    // The maneuver each side chose last round (Assault before the first).
    Maneuver lastManeuver(int owner) const;
    // How many hits each unit took last round, for showing.
    const std::vector<int>& lastDamage() const { return lastDamage_; }
    // Who fired at (or rammed) whom last round, as places in units().
    struct Shot {
        std::size_t from = 0;
        std::size_t to = 0;
    };
    const std::vector<Shot>& lastShots() const { return lastShots_; }
    bool over() const;
    // Every side opened hailing frequencies: nobody fought.
    bool avoided() const { return avoided_; }

    // Fights a round: the player's ships do as told, the others choose.
    void fight(const Game& game, Maneuver order);
    // Fights to the end, the player's ships choosing as the computer would.
    void fightToEnd(const Game& game);
    // What the computer would order a side.
    Maneuver choose(const Game& game, int owner) const;

    // Writes the result into the game: hulls, and retreating fleets moved
    // to a neighbouring sector. Destroyed ships stay (with hull 0) until
    // removeWrecks, so the fleets' places do not change.
    Battle finish(Game& game) const;

private:
    int strength(int owner) const;
    bool hasEnemy(const Game& game, int owner) const;

    Sector at_;
    int player_ = -1;
    std::vector<CombatUnit> units_;
    std::vector<bool> gone_;
    std::vector<int> sides_;
    std::vector<Maneuver> orders_; // for each side, last round's
    std::vector<int> lastDamage_;
    std::vector<Shot> lastShots_;
    Battle battle_;
    int round_ = 0;
    bool avoided_ = false;
    bool ended_ = false;
};

// The sectors where the player would fight this turn.
std::vector<Sector> playerBattles(const Game& game);

} // namespace nagus
