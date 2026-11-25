// SPDX-License-Identifier: GPL-3.0-or-later
//
// The rules of a multiplayer game. Every player plays their turn on their
// own copy of the game; the host takes each player's orders into its game,
// ends the turn once for each player (the same way each time, so the
// galaxy comes out the same) and sends every player the game as they see
// it with their own report. See docs/rules/multiplayer.md.
#pragma once

#include "nagus/game/game.hpp"
#include "nagus/util/bytes.hpp"

#include <vector>

namespace nagus {

// Takes what `empire`'s player did this turn into `merged`: `played` is
// their copy of the game, which was `start` when the turn began. Their
// colonies, fleets' orders, research and intelligence plans, the credits
// they spent or gave, the treaties they made and the proposals they made
// or answered.
void mergeOrders(Game& merged, const Game& start, const Game& played, int empire);

// The end of a multiplayer turn, for one player.
struct PlayerTurn {
    int empire = -1;
    Game game; // the game after the turn, as this player has it
    TurnSummary summary;
};
// Ends the turn of the merged game for every human player. The first is
// the host's (merged.player).
std::vector<PlayerTurn> endMultiplayerTurn(const Game& merged);

// A turn's report, sent from the host to a player.
Bytes saveSummary(const TurnSummary& summary);
// Throws FormatError for damaged data.
TurnSummary loadSummary(ByteView data);

} // namespace nagus
