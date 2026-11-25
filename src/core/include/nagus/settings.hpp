// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the player chose for a new game, remembered between runs like the
// original remembers it. Kept as a small text file of "key=value" lines in
// Nagus's own folder.
#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace nagus {

inline constexpr int kEmpires = 5; // Cardassian, Federation, Ferengi, Klingon, Romulan

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
