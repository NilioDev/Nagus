// SPDX-License-Identifier: GPL-3.0-or-later
//
// Makes small zip archives and CD images in memory, so the readers and the
// importer can be tested without the original files.
#pragma once

#include "nagus/util/bytes.hpp"
#include "nagus/util/inflate.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace nagus::test {

inline void put16(Bytes& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
inline void put32(Bytes& b, std::uint32_t v) {
    put16(b, v & 0xffff);
    put16(b, v >> 16);
}
inline void putText(Bytes& b, const std::string& s) { b.insert(b.end(), s.begin(), s.end()); }
inline Bytes bytesOf(const std::string& s) { return Bytes(s.begin(), s.end()); }

// A zip archive with stored (uncompressed) entries.
inline Bytes makeZip(const std::vector<std::pair<std::string, Bytes>>& files) {
    Bytes out;
    Bytes central;
    for (const auto& [name, data] : files) {
        const auto offset = static_cast<std::uint32_t>(out.size());
        const std::uint32_t crc = deflate::crc32(data);
        put32(out, 0x04034b50);
        put16(out, 20);
        put16(out, 0);
        put16(out, 0); // stored
        put32(out, 0); // time, date
        put32(out, crc);
        put32(out, static_cast<std::uint32_t>(data.size()));
        put32(out, static_cast<std::uint32_t>(data.size()));
        put16(out, static_cast<std::uint32_t>(name.size()));
        put16(out, 0);
        putText(out, name);
        out.insert(out.end(), data.begin(), data.end());

        put32(central, 0x02014b50);
        put16(central, 20);
        put16(central, 20);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0);
        put32(central, crc);
        put32(central, static_cast<std::uint32_t>(data.size()));
        put32(central, static_cast<std::uint32_t>(data.size()));
        put16(central, static_cast<std::uint32_t>(name.size()));
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0);
        put32(central, offset);
        putText(central, name);
    }
    const auto cdOffset = static_cast<std::uint32_t>(out.size());
    out.insert(out.end(), central.begin(), central.end());
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<std::uint32_t>(files.size()));
    put16(out, static_cast<std::uint32_t>(files.size()));
    put32(out, static_cast<std::uint32_t>(central.size()));
    put32(out, cdOffset);
    put16(out, 0);
    return out;
}

// An ISO 9660 image with files in the root folder and in one subfolder.
// `raw` makes 2352-byte mode 1 sectors as in a .bin file.
inline Bytes makeIso(const std::vector<std::pair<std::string, Bytes>>& rootFiles, const std::string& subName,
                     const std::vector<std::pair<std::string, Bytes>>& subFiles, bool raw) {
    constexpr std::uint32_t kSector = 2048;
    std::vector<Bytes> sectors(16, Bytes(kSector, 0));
    auto sectorsFor = [](std::size_t size) { return static_cast<std::uint32_t>((size + kSector - 1) / kSector); };
    const std::uint32_t rootLba = 18;
    const std::uint32_t subLba = 19;
    std::uint32_t next = 20;
    std::vector<std::uint32_t> rootLbas;
    std::vector<std::uint32_t> subLbas;
    for (const auto& f : rootFiles) {
        rootLbas.push_back(next);
        next += std::max<std::uint32_t>(1, sectorsFor(f.second.size()));
    }
    for (const auto& f : subFiles) {
        subLbas.push_back(next);
        next += std::max<std::uint32_t>(1, sectorsFor(f.second.size()));
    }
    auto record = [](Bytes& dir, std::uint32_t lba, std::uint32_t size, bool folder, const std::string& name) {
        Bytes r;
        const std::size_t nameLen = name.size();
        const std::size_t len = 33 + nameLen + ((33 + nameLen) % 2);
        r.push_back(static_cast<std::uint8_t>(len));
        r.push_back(0);
        put32(r, lba);
        put32(r, 0);
        put32(r, size);
        put32(r, 0);
        r.insert(r.end(), 7, 0);
        r.push_back(folder ? 2 : 0);
        r.push_back(0);
        r.push_back(0);
        put32(r, 0);
        r.push_back(static_cast<std::uint8_t>(nameLen));
        putText(r, name);
        r.resize(len, 0);
        dir.insert(dir.end(), r.begin(), r.end());
    };
    Bytes root;
    record(root, rootLba, kSector, true, std::string(1, '\0'));
    record(root, rootLba, kSector, true, std::string(1, '\1'));
    record(root, subLba, kSector, true, subName);
    for (std::size_t i = 0; i < rootFiles.size(); ++i) {
        record(root, rootLbas[i], static_cast<std::uint32_t>(rootFiles[i].second.size()), false, rootFiles[i].first + ";1");
    }
    Bytes sub;
    record(sub, subLba, kSector, true, std::string(1, '\0'));
    record(sub, rootLba, kSector, true, std::string(1, '\1'));
    for (std::size_t i = 0; i < subFiles.size(); ++i) {
        record(sub, subLbas[i], static_cast<std::uint32_t>(subFiles[i].second.size()), false, subFiles[i].first + ";1");
    }
    Bytes pvd(kSector, 0);
    pvd[0] = 1;
    std::copy_n("CD001", 5, pvd.begin() + 1);
    pvd[6] = 1;
    Bytes rootRecord;
    record(rootRecord, rootLba, kSector, true, std::string(1, '\0'));
    std::copy(rootRecord.begin(), rootRecord.end(), pvd.begin() + 156);
    sectors.push_back(pvd);
    Bytes term(kSector, 0);
    term[0] = 255;
    std::copy_n("CD001", 5, term.begin() + 1);
    sectors.push_back(term);
    root.resize(kSector, 0);
    sub.resize(kSector, 0);
    sectors.push_back(root);
    sectors.push_back(sub);
    auto addFile = [&](const Bytes& data) {
        const std::uint32_t n = std::max<std::uint32_t>(1, sectorsFor(data.size()));
        for (std::uint32_t s = 0; s < n; ++s) {
            Bytes sec(kSector, 0);
            const std::size_t from = static_cast<std::size_t>(s) * kSector;
            if (from < data.size()) {
                std::copy(data.begin() + static_cast<std::ptrdiff_t>(from),
                          data.begin() + static_cast<std::ptrdiff_t>(std::min(data.size(), from + kSector)), sec.begin());
            }
            sectors.push_back(sec);
        }
    };
    for (const auto& f : rootFiles) {
        addFile(f.second);
    }
    for (const auto& f : subFiles) {
        addFile(f.second);
    }
    Bytes out;
    for (const auto& s : sectors) {
        if (raw) {
            const std::uint8_t sync[12] = {0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0};
            out.insert(out.end(), sync, sync + 12);
            out.insert(out.end(), {0, 0, 0, 1}); // address (unused), mode 1
            out.insert(out.end(), s.begin(), s.end());
            out.insert(out.end(), 288, 0); // error correction (unused)
        } else {
            out.insert(out.end(), s.begin(), s.end());
        }
    }
    return out;
}

// A small lexicon file: the given texts, in order.
// A tooltip table (teachtip.tdb) with the given texts for the given buttons.
inline Bytes makeTips(const std::vector<std::pair<std::uint32_t, std::string>>& tips) {
    Bytes text;
    Bytes out;
    put32(out, static_cast<std::uint32_t>(tips.size()));
    put32(out, 0);
    put32(out, 0);
    put32(out, 0);
    for (const auto& [id, t] : tips) {
        put32(out, id);
        put32(out, static_cast<std::uint32_t>(t.size() + 1));
        put32(out, 0x5a0000);
        put32(out, static_cast<std::uint32_t>(text.size()));
        putText(text, t);
        text.push_back(0);
    }
    out.insert(out.end(), text.begin(), text.end());
    return out;
}

inline Bytes makeLexicon(const std::vector<std::string>& texts) {
    Bytes body;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> entries;
    for (const auto& t : texts) {
        entries.emplace_back(static_cast<std::uint32_t>(4 + body.size()), static_cast<std::uint32_t>(t.size() + 1));
        putText(body, t);
        body.push_back(0);
    }
    Bytes out;
    put32(out, static_cast<std::uint32_t>(4 + body.size()));
    out.insert(out.end(), body.begin(), body.end());
    put32(out, 0);
    put16(out, static_cast<std::uint32_t>(entries.size()));
    for (const auto& [o, l] : entries) {
        put32(out, o);
        put32(out, l);
    }
    return out;
}

} // namespace nagus::test
