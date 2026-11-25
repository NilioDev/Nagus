// SPDX-License-Identifier: GPL-3.0-or-later
//
// Importing the player's own copy of the game into Nagus's library.
//
// The player gives us whatever they have: the CD image (.bin/.cue or .iso),
// the zip the image came in, a copy of the CD's files, an installed game
// folder, and the 1.0.2 patch (its zip or unpacked folder). We copy the files
// the engine reads into the library's base pack; nothing is changed in the
// player's files. See docs/formats/library.md.
#pragma once

#include "nagus/util/bytes.hpp"
#include "nagus/util/source.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nagus {

// What is in the library's base pack.
struct LibraryState {
    bool game = false;  // the original files are there
    bool patch = false; // with the 1.0.2 patch on top
};

// What one import step found.
enum class Found { Nothing, Game, Patch, GameAndPatch };

class Importer {
public:
    // Imports into `library` (created when missing). `progress` is told how
    // far a long step is, from 0 to 1, with a short label.
    using Progress = std::function<void(const std::string& label, double done)>;

    explicit Importer(std::filesystem::path library, Progress progress = {});

    // Imports from one file. `name` is its file name as the player sees it
    // (the content decides what it is, the name only helps with messages).
    Found importFile(const std::string& name, std::shared_ptr<Source> source);
    // Imports from a folder and the files in it (a few levels deep).
    Found importFolder(const std::filesystem::path& folder);

    static LibraryState state(const std::filesystem::path& library);
    // The folder with the imported files.
    static std::filesystem::path baseDir(const std::filesystem::path& library) { return library / "base"; }
    static std::filesystem::path patchDir(const std::filesystem::path& library) { return library / "base" / "patch"; }

    // Which files of the CD the engine keeps, by their name (any folder).
    static bool wanted(const std::string& path);

private:
    Found fromZip(const std::string& name, const std::shared_ptr<Source>& source);
    Found fromImage(const std::string& name, const std::function<void(const std::function<void(ByteView)>&)>& stream,
                    std::uint64_t size);
    Found fromPatchFiles(const std::vector<std::pair<std::string, Bytes>>& files);
    void commitGame();
    void say(const std::string& label, double done) const;

    std::filesystem::path library_;
    Progress progress_;
};

} // namespace nagus
