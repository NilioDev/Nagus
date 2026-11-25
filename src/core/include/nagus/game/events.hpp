// SPDX-License-Identifier: GPL-3.0-or-later
//
// Random events, when the game's settings have them: plague, earthquakes,
// comets, terrorists, immigration and a ship virus. The texts are the
// game's own; how often and how hard they strike is our choice (see
// docs/rules/events.md).
#pragma once

#include "nagus/game/game.hpp"

namespace nagus {

// The chance (percent) each turn that an empire is struck by an event.
inline constexpr int kEventChance = 4;

// Rolls this turn's events; returns those that struck the player.
std::vector<GameEvent> randomEvents(Game& game);

} // namespace nagus
