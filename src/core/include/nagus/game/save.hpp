// SPDX-License-Identifier: GPL-3.0-or-later
//
// Saved games: our own file format (see docs/formats/saves.md). The tables
// from the game's files are not saved; loading takes them from the game the
// player has.
#pragma once

#include "nagus/game/game.hpp"
#include "nagus/util/bytes.hpp"

#include <string>

namespace nagus {

struct SaveInfo {
    std::string name;        // what the player sees in the list
    std::string description;
    int turn = 0;
};

Bytes saveGame(const Game& game, const SaveInfo& info);
// Throws FormatError for a damaged file or one that does not fit the tables.
Game loadGame(ByteView data, const EconomyData& economy);
SaveInfo readSaveInfo(ByteView data);

} // namespace nagus
