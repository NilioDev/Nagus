// SPDX-License-Identifier: GPL-3.0-or-later
//
// The zip layout we read is the public one from PKWARE's APPNOTE: the
// end-of-central-directory record near the end, the central directory it
// points to, and a local header before each entry's data. Zip64 sizes are
// read from the extra field when an archive needs them.
#include "nagus/data/zip_archive.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr std::uint32_t kEndSig = 0x06054b50;
constexpr std::uint32_t kEnd64Sig = 0x06064b50;
constexpr std::uint32_t kEnd64LocatorSig = 0x07064b50;
constexpr std::uint32_t kCentralSig = 0x02014b50;
constexpr std::uint32_t kLocalSig = 0x04034b50;
constexpr std::size_t kMaxEntries = 200000;

} // namespace

bool ZipArchive::looksLikeZip(Source& source) {
    if (source.size() < 22) {
        return false;
    }
    const Bytes head = source.readAll(0, 4);
    return readU32(head, 0) == kLocalSig || readU32(head, 0) == kEndSig;
}

ZipArchive::ZipArchive(std::shared_ptr<Source> source) : source_(std::move(source)) {
    const std::uint64_t total = source_->size();
    if (total < 22) {
        throw FormatError("not a zip archive");
    }
    // The end record is the last 22 bytes plus a comment of up to 64 KB.
    const std::uint64_t tailSize = std::min<std::uint64_t>(total, 22 + 65535);
    const Bytes tail = source_->readAll(total - tailSize, static_cast<std::size_t>(tailSize));
    std::optional<std::size_t> end;
    for (std::size_t i = tail.size() - 22 + 1; i-- > 0;) {
        if (readU32(tail, i) == kEndSig) {
            end = i;
            break;
        }
    }
    if (!end) {
        throw FormatError("not a zip archive (no end record)");
    }
    Reader e(tail, *end + 4);
    const std::uint16_t disk = e.u16();
    const std::uint16_t cdDisk = e.u16();
    e.u16();
    std::uint64_t count = e.u16();
    std::uint64_t cdSize = e.u32();
    std::uint64_t cdOffset = e.u32();
    if (disk != 0 || cdDisk != 0) {
        throw FormatError("split zip archives are not supported");
    }
    const std::uint64_t endAt = total - tailSize + *end;
    if ((count == 0xffff || cdSize == 0xffffffffu || cdOffset == 0xffffffffu) && endAt >= 20) {
        const Bytes loc = source_->readAll(endAt - 20, 20);
        if (readU32(loc, 0) == kEnd64LocatorSig) {
            Reader l(loc, 8);
            const std::uint64_t at = l.u32() | (static_cast<std::uint64_t>(l.u32()) << 32);
            const Bytes rec = source_->readAll(at, 56);
            Reader z(rec);
            if (z.u32() != kEnd64Sig) {
                throw FormatError("damaged zip64 end record");
            }
            z.skip(8 + 2 + 2 + 4 + 4 + 8);
            count = z.u32() | (static_cast<std::uint64_t>(z.u32()) << 32);
            cdSize = z.u32() | (static_cast<std::uint64_t>(z.u32()) << 32);
            cdOffset = z.u32() | (static_cast<std::uint64_t>(z.u32()) << 32);
        }
    }
    if (count > kMaxEntries || cdOffset > total || cdSize > total - cdOffset || cdSize > (64u << 20)) {
        throw FormatError("damaged zip central directory");
    }
    const Bytes cd = source_->readAll(cdOffset, static_cast<std::size_t>(cdSize));
    Reader r(cd);
    entries_.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        if (r.u32() != kCentralSig) {
            throw FormatError("damaged zip central directory");
        }
        r.skip(4); // versions
        const std::uint16_t flags = r.u16();
        Entry entry;
        entry.method = r.u16();
        r.skip(4); // time, date
        entry.crc = r.u32();
        entry.packedSize = r.u32();
        entry.size = r.u32();
        const std::uint16_t nameLen = r.u16();
        const std::uint16_t extraLen = r.u16();
        const std::uint16_t commentLen = r.u16();
        r.skip(2 + 2 + 4); // disk, internal and external attributes
        entry.headerOffset = r.u32();
        entry.name = r.text(nameLen);
        Reader extra(r.bytes(extraLen));
        r.skip(commentLen);
        while (extra.left() >= 4) {
            const std::uint16_t id = extra.u16();
            const std::uint16_t len = extra.u16();
            Reader field(extra.bytes(len));
            if (id != 1) {
                continue;
            }
            auto u64 = [&field] { return field.u32() | (static_cast<std::uint64_t>(field.u32()) << 32); };
            if (entry.size == 0xffffffffu) {
                entry.size = u64();
            }
            if (entry.packedSize == 0xffffffffu) {
                entry.packedSize = u64();
            }
            if (entry.headerOffset == 0xffffffffu) {
                entry.headerOffset = u64();
            }
        }
        if (flags & 1u) {
            throw FormatError("encrypted zip archives are not supported");
        }
        std::replace(entry.name.begin(), entry.name.end(), '\\', '/');
        if (entry.name.empty() || entry.name.back() == '/') {
            continue; // a folder
        }
        byName_.emplace(lower(entry.name), entries_.size());
        entries_.push_back(std::move(entry));
    }
}

const ZipArchive::Entry* ZipArchive::find(std::string_view name) const {
    const auto it = byName_.find(lower(name));
    return it == byName_.end() ? nullptr : &entries_[it->second];
}

std::uint64_t ZipArchive::dataOffset(const Entry& entry) const {
    const Bytes local = source_->readAll(entry.headerOffset, 30);
    Reader r(local);
    if (r.u32() != kLocalSig) {
        throw FormatError("damaged zip entry " + entry.name);
    }
    r.skip(22);
    const std::uint16_t nameLen = r.u16();
    const std::uint16_t extraLen = r.u16();
    const std::uint64_t at = entry.headerOffset + 30 + nameLen + extraLen;
    if (at > source_->size() || entry.packedSize > source_->size() - at) {
        throw FormatError("damaged zip entry " + entry.name);
    }
    return at;
}

Bytes ZipArchive::read(const Entry& entry, std::size_t maxSize) const {
    if (entry.size > maxSize) {
        throw FormatError("zip entry too big: " + entry.name);
    }
    Bytes out;
    out.reserve(static_cast<std::size_t>(entry.size));
    stream(entry, [&out, maxSize](ByteView piece) {
        if (piece.size() > maxSize - out.size()) {
            throw FormatError("zip entry bigger than it says");
        }
        out.insert(out.end(), piece.begin(), piece.end());
    });
    return out;
}

void ZipArchive::stream(const Entry& entry, const deflate::Push& out) const {
    const std::uint64_t at = dataOffset(entry);
    std::uint32_t crc = 0;
    std::uint64_t produced = 0;
    const deflate::Push check = [&](ByteView piece) {
        crc = deflate::crc32(piece, crc);
        produced += piece.size();
        if (produced > entry.size) {
            throw FormatError("zip entry bigger than it says: " + entry.name);
        }
        out(piece);
    };
    if (entry.method == 0) {
        if (entry.packedSize != entry.size) {
            throw FormatError("damaged zip entry " + entry.name);
        }
        Bytes buf(1u << 20);
        for (std::uint64_t done = 0; done < entry.size;) {
            const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(buf.size(), entry.size - done));
            source_->read(at + done, std::span(buf).first(n));
            check(std::span<const std::uint8_t>(buf).first(n));
            done += n;
        }
    } else if (entry.method == 8) {
        std::uint64_t done = 0;
        const deflate::Pull pull = [&](std::span<std::uint8_t> buf) {
            const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(buf.size(), entry.packedSize - done));
            if (n > 0) {
                source_->read(at + done, buf.first(n));
                done += n;
            }
            return n;
        };
        deflate::inflate(pull, check, entry.size);
    } else {
        throw FormatError("unsupported zip compression in " + entry.name);
    }
    if (produced != entry.size || crc != entry.crc) {
        throw FormatError("damaged zip entry (checksum) " + entry.name);
    }
}

} // namespace nagus
