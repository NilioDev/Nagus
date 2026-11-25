// SPDX-License-Identifier: GPL-3.0-or-later
//
// For tests that need the original files: imports whatever the player put in
// gamedata/ into a library in the build folder (once), the way the app does.
#pragma once

#include "nagus/game_files.hpp"

#include <filesystem>
#include <optional>

namespace nagus::test {

inline std::optional<std::filesystem::path> gameLibrary() {
    namespace fs = std::filesystem;
    static const std::optional<fs::path> library = []() -> std::optional<fs::path> {
        const fs::path data = NAGUS_GAMEDATA_DIR;
        const fs::path lib = NAGUS_TEST_LIBRARY;
        if (Importer::state(lib).game) {
            return lib;
        }
        std::error_code ec;
        if (!fs::is_directory(data, ec)) {
            return std::nullopt;
        }
        try {
            Importer(lib).importFolder(data);
        } catch (const std::exception&) {
            return std::nullopt;
        }
        if (!Importer::state(lib).game) {
            return std::nullopt;
        }
        return lib;
    }();
    return library;
}

} // namespace nagus::test
