// SPDX-License-Identifier: GPL-3.0-or-later
//
// alt.res, the second resource archive (the pictures of the game's end):
// a list of files, each packed with a simple LZSS (see
// docs/formats/library.md).
#pragma once

#include "nagus/util/source.hpp"

#include <memory>
#include <string>
#include <vector>

namespace nagus {

class LzArchive {
public:
    struct Entry {
        std::string name; // lower case
        std::uint32_t offset = 0;
        std::uint32_t size = 0;       // unpacked
        std::uint32_t packedSize = 0;
        std::uint32_t method = 0;     // 0 stored, 1 LZSS
    };

    // Reads the list. Throws FormatError if it is damaged.
    explicit LzArchive(std::shared_ptr<Source> source);

    const std::vector<Entry>& entries() const { return entries_; }
    const Entry* find(std::string_view name) const;
    // Unpacks an entry; throws FormatError if it is damaged or too big.
    Bytes read(const Entry& entry, std::size_t maxSize = 16u << 20) const;

private:
    std::shared_ptr<Source> source_;
    std::vector<Entry> entries_;
};

// Unpacks LZSS data: a flag byte for each eight items (lowest bit first; 1
// a byte as it is, 0 a copy of 2 to 17 bytes from a 4096-byte window).
Bytes unpackLzss(ByteView packed, std::size_t size);

} // namespace nagus
