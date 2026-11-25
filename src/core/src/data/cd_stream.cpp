// SPDX-License-Identifier: GPL-3.0-or-later
//
// ISO 9660 as published in ECMA-119: the primary volume descriptor in sector
// 16 holds the root folder's record; a folder is a list of records, each with
// the first sector, size, flags and name of a file or folder in it.
#include "nagus/data/cd_stream.hpp"

#include <algorithm>
#include <array>

namespace nagus {

namespace {

constexpr std::size_t kUser = 2048;
constexpr std::size_t kRaw = 2352;
constexpr std::uint32_t kMaxDirectories = 4096;
constexpr std::uint32_t kMaxDirectorySize = 1u << 20;

constexpr std::array<std::uint8_t, 12> kSync{0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
                                             0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

bool isSync(ByteView b) { return b.size() >= 12 && std::equal(kSync.begin(), kSync.end(), b.begin()); }

} // namespace

bool CdStreamReader::looksLikeImage(ByteView start) {
    if (isSync(start)) {
        return true;
    }
    // An .iso begins with 32 KB of system area, normally all zeros.
    if (start.size() < 64) {
        return false;
    }
    return std::all_of(start.begin(), start.begin() + 64, [](std::uint8_t b) { return b == 0; });
}

void CdStreamReader::feed(ByteView bytes) {
    while (!bytes.empty()) {
        if (sectorSize_ == 0) {
            const std::size_t need = 12 - std::min<std::size_t>(12, pending_.size());
            const std::size_t n = std::min(need, bytes.size());
            pending_.insert(pending_.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n));
            bytes = bytes.subspan(n);
            if (pending_.size() < 12) {
                return;
            }
            sectorSize_ = isSync(pending_) ? kRaw : kUser;
        }
        const std::size_t n = std::min(sectorSize_ - pending_.size(), bytes.size());
        pending_.insert(pending_.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n));
        bytes = bytes.subspan(n);
        if (pending_.size() == sectorSize_) {
            if (sectorSize_ == kUser) {
                sector(pending_);
            } else {
                // Mode 1 keeps the data after a 16-byte header; mode 2 form 1
                // (XA) has an 8-byte subheader on top.
                const std::uint8_t mode = pending_[15];
                const std::size_t at = mode == 2 ? 24 : 16;
                sector(ByteView(pending_).subspan(at, kUser));
            }
            pending_.clear();
            ++lba_;
        }
    }
}

void CdStreamReader::schedule(std::uint32_t lba, Task task) {
    if (task.sectors == 0) {
        if (!task.directory) {
            sink_(task.path, 0, 0, {});
        }
        return;
    }
    if (lba <= lba_) {
        throw FormatError("the CD image's folders come after its files; this layout is not supported");
    }
    tasks_.emplace(lba, std::move(task));
}

void CdStreamReader::sector(ByteView data) {
    if (lba_ == 16) {
        if (data[0] != 1 || !std::equal(data.begin() + 1, data.begin() + 6, "CD001")) {
            throw FormatError("not an ISO 9660 CD image");
        }
        sawVolume_ = true;
        // The root folder's record sits at byte 156 of the descriptor.
        Reader root(data, 156 + 2);
        const std::uint32_t lba = root.u32();
        root.skip(4);
        const std::uint32_t size = root.u32();
        if (size > kMaxDirectorySize) {
            throw FormatError("damaged CD image (root folder)");
        }
        Task t;
        t.directory = true;
        t.size = size;
        t.sectors = static_cast<std::uint32_t>((size + kUser - 1) / kUser);
        schedule(lba, std::move(t));
        return;
    }
    if (tasks_.empty()) {
        return;
    }
    auto it = tasks_.upper_bound(lba_);
    if (it == tasks_.begin()) {
        return;
    }
    --it;
    const std::uint32_t first = it->first;
    Task& task = it->second;
    const std::uint32_t index = lba_ - first;
    if (index >= task.sectors) {
        return;
    }
    if (task.directory) {
        task.data.insert(task.data.end(), data.begin(), data.end());
        if (index + 1 == task.sectors) {
            const Task done = std::move(task);
            tasks_.erase(it);
            parseDirectory(done);
        }
        return;
    }
    const std::uint32_t offset = index * static_cast<std::uint32_t>(kUser);
    const std::uint32_t len = std::min<std::uint32_t>(static_cast<std::uint32_t>(kUser), task.size - offset);
    sink_(task.path, task.size, offset, data.first(len));
    if (index + 1 == task.sectors) {
        tasks_.erase(it);
    }
}

void CdStreamReader::parseDirectory(const Task& dir) {
    if (++directoriesRead_ > kMaxDirectories) {
        throw FormatError("damaged CD image (too many folders)");
    }
    const ByteView all = ByteView(dir.data).first(std::min<std::size_t>(dir.size, dir.data.size()));
    std::size_t pos = 0;
    while (pos < all.size()) {
        const std::uint8_t len = all[pos];
        if (len == 0) {
            // Records do not cross sectors: skip to the next one.
            pos = (pos / kUser + 1) * kUser;
            continue;
        }
        if (len < 34 || pos + len > all.size()) {
            throw FormatError("damaged CD image (folder record)");
        }
        Reader r(all.subspan(pos, len));
        r.skip(2);
        const std::uint32_t lba = r.u32();
        r.skip(4);
        const std::uint32_t size = r.u32();
        r.skip(4);
        r.skip(7); // date
        const std::uint8_t flags = r.u8();
        r.skip(6);
        const std::uint8_t nameLen = r.u8();
        const ByteView rawName = r.bytes(nameLen);
        pos += len;
        if (nameLen == 1 && (rawName[0] == 0 || rawName[0] == 1)) {
            continue; // "." and ".."
        }
        std::string name(rawName.begin(), rawName.end());
        if (const auto semi = name.find(';'); semi != std::string::npos) {
            name.resize(semi);
        }
        if (!name.empty() && name.back() == '.') {
            name.pop_back();
        }
        if (name.empty() || name.find('/') != std::string::npos || name == "..") {
            throw FormatError("damaged CD image (file name)");
        }
        const std::string path = dir.path.empty() ? name : dir.path + "/" + name;
        Task t;
        t.path = path;
        t.size = size;
        t.sectors = static_cast<std::uint32_t>((static_cast<std::uint64_t>(size) + kUser - 1) / kUser);
        if (flags & 2u) {
            if (size > kMaxDirectorySize) {
                throw FormatError("damaged CD image (folder size)");
            }
            t.directory = true;
            schedule(lba, std::move(t));
        } else if (want_(path, size)) {
            schedule(lba, std::move(t));
        }
    }
}

void CdStreamReader::finish() {
    if (!sawVolume_) {
        throw FormatError("not an ISO 9660 CD image");
    }
    for (const auto& [lba, task] : tasks_) {
        if (!task.directory) {
            throw FormatError("the CD image ends before " + task.path);
        }
    }
}

} // namespace nagus
