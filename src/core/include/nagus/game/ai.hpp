// SPDX-License-Identifier: GPL-3.0-or-later
//
// The computer players: each empire the player does not play chooses what
// its colonies build, buys with spare credits and sends its ships out. This
// is our own design (see docs/rules/computer-players.md), not the
// original's.
#pragma once

#include "nagus/game/game.hpp"

namespace nagus {

// Gives one empire's orders for the coming turn. The same game gives the
// same orders.
void playEmpire(Game& game, int empire);

// The systems an empire could found a colony on: stars with room for its
// people and no colony or minor race.
bool freeForColony(const Game& game, int system, int empire);

} // namespace nagus
