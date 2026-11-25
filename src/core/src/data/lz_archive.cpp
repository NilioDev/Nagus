// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/data/lz_archive.hpp"

#include <array>

namespace nagus {

namespace {

constexpr std::size_t kHeader = 24;
constexpr std::size_t kEntry = 40;
constexpr std::uint32_t kMaxEntries = 10000;
constexpr std::size_t kWindow = 4096;

} // namespace

Bytes unpackLzss(ByteView in, std::size_t size) {
    Bytes out;
    out.reserve(size);
    std::array<std::uint8_t, kWindow> window{};
    std::size_t at = 1; // where the next byte goes in the window
    std::size_t i = 0;
    while (out.size() < size) {
        if (i >= in.size()) {
            throw FormatError("LZSS: data cut short");
        }
        const std::uint8_t flags = in[i++];
        for (int bit = 0; bit < 8 && out.size() < size; ++bit) {
            if ((flags >> bit) & 1) {
                if (i >= in.size()) {
                    throw FormatError("LZSS: data cut short");
                }
                const std::uint8_t c = in[i++];
                out.push_back(c);
                window[at] = c;
                at = (at + 1) % kWindow;
            } else {
                if (i + 1 >= in.size()) {
                    throw FormatError("LZSS: data cut short");
                }
                const std::uint8_t a = in[i];
                const std::uint8_t b = in[i + 1];
                i += 2;
                std::size_t from = static_cast<std::size_t>((a & 0x0f) << 8 | b);
                const int length = (a >> 4) + 2;
                for (int k = 0; k < length && out.size() < size; ++k) {
                    const std::uint8_t c = window[from];
                    from = (from + 1) % kWindow;
                    out.push_back(c);
                    window[at] = c;
                    at = (at + 1) % kWindow;
                }
            }
        }
    }
    return out;
}

LzArchive::LzArchive(std::shared_ptr<Source> source) : source_(std::move(source)) {
    const std::uint64_t total = source_->size();
    if (total < kHeader) {
        throw FormatError("LZ archive: too short");
    }
    const Bytes head = source_->readAll(0, kHeader);
    const std::uint32_t list = readU32(head, 16);
    const std::uint32_t count = readU32(head, 20);
    if (count > kMaxEntries || list < kHeader || list > total || (total - list) / kEntry < count) {
        throw FormatError("LZ archive: bad list");
    }
    const Bytes l = source_->readAll(list, count * kEntry);
    Reader r(l);
    for (std::uint32_t n = 0; n < count; ++n) {
        Entry e;
        e.name = lower(r.text(16));
        e.offset = r.u32();
        r.u32(); // its number
        e.size = r.u32();
        e.packedSize = r.u32();
        e.method = r.u32();
        r.u32(); // a check value we do not know
        if (e.offset < kHeader || e.offset > list || e.packedSize > list - e.offset || e.method > 1) {
            throw FormatError("LZ archive: entry outside the file");
        }
        entries_.push_back(std::move(e));
    }
}

const LzArchive::Entry* LzArchive::find(std::string_view name) const {
    const std::string n = lower(name);
    for (const auto& e : entries_) {
        if (e.name == n) {
            return &e;
        }
    }
    return nullptr;
}

Bytes LzArchive::read(const Entry& e, std::size_t maxSize) const {
    if (e.size > maxSize || e.packedSize > maxSize) {
        throw FormatError("LZ archive: entry too big");
    }
    Bytes packed = source_->readAll(e.offset, e.packedSize);
    if (e.method == 0) {
        packed.resize(std::min<std::size_t>(packed.size(), e.size));
        return packed;
    }
    return unpackLzss(packed, e.size);
}

} // namespace nagus
