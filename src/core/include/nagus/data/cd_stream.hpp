// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reads the files out of a CD image (ISO 9660) while the image streams past
// once from start to end. That lets the importer take the CD image straight
// out of the zip it came in, without unpacking 400 MB to disk first. Images
// with 2048-byte sectors (.iso) and raw 2352-byte sectors (.bin with a .cue,
// mode 1 or mode 2 form 1) are understood.
#pragma once

#include "nagus/util/bytes.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace nagus {

class CdStreamReader {
public:
    // Decides which files to read, by their path on the CD (with '/', no
    // ";1" version, as written on the CD).
    using Want = std::function<bool(const std::string& path, std::uint32_t size)>;
    // Takes the next piece of a wanted file. `offset` is where the piece goes
    // in the file; pieces come in order and the last one ends at `size`.
    using Sink = std::function<void(const std::string& path, std::uint32_t size, std::uint32_t offset, ByteView piece)>;

    CdStreamReader(Want want, Sink sink) : want_(std::move(want)), sink_(std::move(sink)) {}

    // Gives the reader the next bytes of the image.
    void feed(ByteView bytes);
    // Call after the last byte: throws FormatError when the image was not an
    // ISO 9660 CD or ended before all wanted files were read.
    void finish();

    // True when the first bytes look like the start of a CD image (a raw
    // sector's sync pattern, or zeros as in the system area of an .iso).
    static bool looksLikeImage(ByteView start);

private:
    struct Task {
        bool directory = false;
        std::string path; // of the file or folder
        std::uint32_t size = 0;
        std::uint32_t sectors = 0;
        Bytes data; // a folder's records, gathered until complete
    };

    void sector(ByteView data);
    void schedule(std::uint32_t lba, Task task);
    void parseDirectory(const Task& dir);

    Want want_;
    Sink sink_;
    std::size_t sectorSize_ = 0; // 2048 or 2352, decided from the first bytes
    Bytes pending_;
    std::uint32_t lba_ = 0; // the sector the next complete one is
    bool sawVolume_ = false;
    std::map<std::uint32_t, Task> tasks_; // by first sector
    std::size_t directoriesRead_ = 0;
};

} // namespace nagus
