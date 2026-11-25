// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reading zip archives through a Source, without loading them whole. Used
// for the original's resource archives (which are plain zip files), for the
// zip the CD image may come in, and for the zipped patch.
#pragma once

#include "nagus/util/bytes.hpp"
#include "nagus/util/inflate.hpp"
#include "nagus/util/source.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nagus {

class ZipArchive {
public:
    struct Entry {
        std::string name; // path inside the archive, with '/'
        std::uint16_t method = 0;
        std::uint32_t crc = 0;
        std::uint64_t packedSize = 0;
        std::uint64_t size = 0;
        std::uint64_t headerOffset = 0;
    };

    // Reads the central directory. Throws FormatError for anything that is
    // not a zip archive we can read (encrypted, split, damaged).
    explicit ZipArchive(std::shared_ptr<Source> source);

    const std::vector<Entry>& entries() const { return entries_; }
    // Finds an entry by name, without regard to case.
    const Entry* find(std::string_view name) const;

    // Unpacks an entry whole, checking its CRC. `maxSize` guards against
    // hostile archives.
    Bytes read(const Entry& entry, std::size_t maxSize = 64u << 20) const;
    // Unpacks an entry piece by piece, for entries too big to hold.
    void stream(const Entry& entry, const deflate::Push& out) const;

    // True when the source starts like a zip archive.
    static bool looksLikeZip(Source& source);

private:
    std::uint64_t dataOffset(const Entry& entry) const;

    std::shared_ptr<Source> source_;
    std::vector<Entry> entries_;
    std::unordered_map<std::string, std::size_t> byName_;
};

} // namespace nagus
