// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the player chose for a new game, remembered between runs like the
// original remembers it. Kept as a small text file of "key=value" lines in
// Nagus's own folder.
#pragma once

#include "nagus/data/tables.hpp"

#include <array>
#include <filesystem>
#include <string>

namespace nagus {


struct GameSettings {
    int galaxyShape = 0;  // irregular, elliptical, ring, spiral
    int galaxySize = 0;   // small, medium, large
    int minorRaces = 2;   // none, few, some, many
    int difficulty = 2;   // simple, easy, normal, hard, impossible
    std::array<int, kEmpires> civilization{}; // beginning … advanced
    bool randomEvents = true;
    bool automaticCombat = false;
    int victory = 0;        // domination, team play, vendetta
    int empire = 1;         // the player's empire (0 = Cardassian … 4 = Romulan)
    // Volumes in percent: music, sound effects, speech (and films), and all
    // of them together.
    std::array<int, 4> volume{70, 80, 90, 100};
    static constexpr std::size_t kMasterVolume = 3;
    // The volume heard: one of the first three, times the volume of all.
    int heard(std::size_t which) const { return volume[which] * volume[kMasterVolume] / 100; }
    // Saves the game at the end of each turn, for Continue.
    bool autosave = true;
    // Shows a button's help text when the pointer rests on it.
    bool tooltips = true;
    // Multiplayer: the player's name, the game's name and the host last
    // joined.
    std::string playerName;
    std::string gameName;
    std::string hostAddress;
    static constexpr std::size_t kMaxText = 64;

    static constexpr int kShapes = 4;
    static constexpr int kSizes = 3;
    static constexpr int kMinorRaces = 4;
    static constexpr int kDifficulties = 5;
    static constexpr int kCivilizations = 5;
    static constexpr int kVictories = 3;

    // Brings every value into its range (after reading a damaged file).
    void clamp();
};

GameSettings loadSettings(const std::filesystem::path& file);
void saveSettings(const std::filesystem::path& file, const GameSettings& settings);

} // namespace nagus
