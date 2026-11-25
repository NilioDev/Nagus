// SPDX-License-Identifier: GPL-3.0-or-later
//
// Saved games: our own file format (see docs/formats/saves.md). The tables
// from the game's files are not saved; loading takes them from the game the
// player has.
#pragma once

#include "nagus/game/game.hpp"
#include "nagus/util/bytes.hpp"

#include <filesystem>
#include <optional>
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

// The places in the save list, each a file in the saves folder.
inline constexpr int kSaveSlots = 12;
std::filesystem::path saveSlotFile(const std::filesystem::path& folder, int slot);
// What a slot holds: nothing, a saved game, or a file that cannot be read.
struct SaveSlot {
    bool used = false;
    bool damaged = false;
    SaveInfo info;
};
SaveSlot readSaveSlot(const std::filesystem::path& file);
// Reads a whole saved game, refusing files too big to be one.
Bytes readSaveFile(const std::filesystem::path& file);
// Writes next to the file first, so a failed write leaves the old one.
// Returns false when it could not be written.
bool writeSaveFile(const std::filesystem::path& file, const Bytes& data);

} // namespace nagus
