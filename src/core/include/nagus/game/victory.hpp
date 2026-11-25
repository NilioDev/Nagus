// SPDX-License-Identifier: GPL-3.0-or-later
//
// How a game ends, and the score. Our own rules (see docs/rules/victory.md).
#pragma once

#include "nagus/game/game.hpp"

namespace nagus {

// An empire is out of the game once it has no colonies left.
bool outOfGame(const Game& game, int empire);
// The share of all colonies (of empires and minor races) an empire holds,
// in percent.
int colonyShare(const Game& game, int empire);
inline constexpr int kDominationShare = 66;

// Takes the fleets of empires that have lost their last colony; returns
// those empires.
std::vector<int> removeFallenEmpires(Game& game);
// How the game stands for the player after a turn (None while it goes on).
Ending checkEnding(const Game& game);
// The player's score.
int score(const Game& game);

// The file name part of the original's end-of-game pictures and speech for
// an ending ("alli", "anni", "domi", "loss"), or empty.
std::string endingName(Ending ending);

} // namespace nagus
