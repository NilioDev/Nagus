// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game's resources as the engine sees them: the files in the main
// resource archive (STBOF.RES), with the 1.0.2 patch's files taking their
// place where it has them. Names are compared without regard to case.
#pragma once

#include "nagus/data/zip_archive.hpp"
#include "nagus/util/bytes.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nagus {

class Resources {
public:
    // Opens the base pack of a library (see Importer).
    explicit Resources(const std::filesystem::path& library);
    // Opens an archive directly, with optional loose files over it (tests).
    Resources(std::shared_ptr<Source> archive, std::map<std::string, Bytes> overrides);

    bool has(std::string_view name) const;
    // The file's bytes. Throws FormatError when there is no such file.
    Bytes file(std::string_view name) const;

    // The names (in lower case) of all files whose name ends with the given
    // text, such as ".wdf", sorted.
    std::vector<std::string> names(std::string_view ending) const;

    // Other files of the base pack, outside the archive (sounds, movies).
    std::filesystem::path loose(std::string_view name) const { return base_ / lower(name); }

private:
    std::filesystem::path base_;
    std::unique_ptr<ZipArchive> archive_;
    std::map<std::string, Bytes> overrides_;
};

} // namespace nagus
