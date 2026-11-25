// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/settings.hpp"

#include <algorithm>
#include <fstream>
#include <map>

namespace nagus {

namespace {

int clampTo(int v, int count) { return std::clamp(v, 0, count - 1); }

} // namespace

void GameSettings::clamp() {
    galaxyShape = clampTo(galaxyShape, kShapes);
    galaxySize = clampTo(galaxySize, kSizes);
    minorRaces = clampTo(minorRaces, kMinorRaces);
    difficulty = clampTo(difficulty, kDifficulties);
    for (int& c : civilization) {
        c = clampTo(c, kCivilizations);
    }
    victory = clampTo(victory, kVictories);
    empire = clampTo(empire, kEmpires);
    for (int& v : volume) {
        v = std::clamp(v, 0, 100);
    }
}

GameSettings loadSettings(const std::filesystem::path& file) {
    GameSettings s;
    std::ifstream in(file);
    std::map<std::string, int> values;
    std::string line;
    while (std::getline(in, line) && values.size() < 256) {
        const auto eq = line.find('=');
        if (eq == std::string::npos || eq > 64) {
            continue;
        }
        try {
            values[line.substr(0, eq)] = std::stoi(line.substr(eq + 1));
        } catch (const std::exception&) {
            // A damaged line keeps its default.
        }
    }
    auto get = [&](const std::string& key, int& out) {
        if (const auto it = values.find(key); it != values.end()) {
            out = it->second;
        }
    };
    get("galaxy_shape", s.galaxyShape);
    get("galaxy_size", s.galaxySize);
    get("minor_races", s.minorRaces);
    get("difficulty", s.difficulty);
    for (int i = 0; i < kEmpires; ++i) {
        get("civilization_" + std::to_string(i), s.civilization[static_cast<std::size_t>(i)]);
    }
    int events = s.randomEvents ? 1 : 0;
    int combat = s.automaticCombat ? 1 : 0;
    get("random_events", events);
    get("automatic_combat", combat);
    s.randomEvents = events != 0;
    s.automaticCombat = combat != 0;
    get("victory", s.victory);
    get("empire", s.empire);
    get("volume_music", s.volume[0]);
    get("volume_sfx", s.volume[1]);
    get("volume_voice", s.volume[2]);
    s.clamp();
    return s;
}

void saveSettings(const std::filesystem::path& file, const GameSettings& s) {
    std::error_code made;
    std::filesystem::create_directories(file.parent_path(), made);
    const auto part = std::filesystem::path(file).concat(".part");
    {
        std::ofstream out(part, std::ios::trunc);
        out << "galaxy_shape=" << s.galaxyShape << '\n'
            << "galaxy_size=" << s.galaxySize << '\n'
            << "minor_races=" << s.minorRaces << '\n'
            << "difficulty=" << s.difficulty << '\n';
        for (int i = 0; i < kEmpires; ++i) {
            out << "civilization_" << i << '=' << s.civilization[static_cast<std::size_t>(i)] << '\n';
        }
        out << "random_events=" << (s.randomEvents ? 1 : 0) << '\n'
            << "automatic_combat=" << (s.automaticCombat ? 1 : 0) << '\n'
            << "victory=" << s.victory << '\n'
            << "empire=" << s.empire << '\n'
            << "volume_music=" << s.volume[0] << '\n'
            << "volume_sfx=" << s.volume[1] << '\n'
            << "volume_voice=" << s.volume[2] << '\n';
        if (!out) {
            return;
        }
    }
    std::error_code ec;
    std::filesystem::rename(part, file, ec);
}

} // namespace nagus
