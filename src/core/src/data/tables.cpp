// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/data/tables.hpp"

#include <cstring>

namespace nagus {

namespace {

constexpr std::size_t kRaceRecord = 108;
constexpr std::size_t kMaxRaces = 64;
constexpr std::size_t kObjectRecord = 24;
constexpr std::size_t kMaxObjectKinds = 64;
constexpr std::size_t kStarNameRecord = 24;
constexpr std::size_t kMaxStarNames = 4096;
constexpr std::size_t kHomePlanetRecord = 88;
constexpr std::size_t kTechRecord = 72;

// Checks that a table holds exactly whole records.
std::size_t records(ByteView data, std::size_t header, std::size_t record, const char* what) {
    if (data.size() < header || (data.size() - header) % record != 0) {
        throw FormatError(std::string("bad ") + what);
    }
    return (data.size() - header) / record;
}

} // namespace

std::vector<RaceInfo> readRaces(ByteView data) {
    Reader r(data);
    const std::size_t count = r.u16();
    if (count > kMaxRaces || count > r.left() / kRaceRecord) {
        throw FormatError("bad race table");
    }
    std::vector<RaceInfo> races(count);
    for (std::size_t i = 0; i < count; ++i) {
        Reader rec(data, 2 + i * kRaceRecord);
        races[i].name = rec.text(40);
        races[i].picture = rec.text(16);
    }
    return races;
}

std::vector<StarObjectKind> readStarObjectKinds(ByteView data) {
    Reader r(data);
    const std::size_t count = r.u16();
    if (count > kMaxObjectKinds || count > r.left() / kObjectRecord) {
        throw FormatError("bad star object table");
    }
    std::vector<StarObjectKind> kinds(count);
    for (std::size_t i = 0; i < count; ++i) {
        Reader rec(data, 2 + i * kObjectRecord);
        if (rec.u16() != i) {
            throw FormatError("bad star object table");
        }
        rec.u16();
        const std::uint32_t bits = rec.u32();
        float share = 0;
        static_assert(sizeof share == sizeof bits);
        std::memcpy(&share, &bits, sizeof share);
        if (!(share >= 0 && share <= 1)) {
            throw FormatError("bad star object table");
        }
        kinds[i].share = share;
        std::string code = rec.text(6);
        // Codes are padded with '-' in the picture names, but some are cut
        // short by a stray zero ("ns").
        kinds[i].code = code;
    }
    return kinds;
}

std::vector<std::string> readStarNames(ByteView data) {
    Reader r(data);
    const std::size_t count = r.u32();
    if (count > kMaxStarNames || count != r.left() / kStarNameRecord) {
        throw FormatError("bad star name table");
    }
    std::vector<std::string> names(count);
    for (std::size_t i = 0; i < count; ++i) {
        Reader rec(data, 4 + i * kStarNameRecord);
        names[i] = rec.text(20);
    }
    return names;
}

std::vector<HomePlanet> readHomePlanets(ByteView data) {
    const std::size_t count = records(data, 0, kHomePlanetRecord, "home system table");
    std::vector<HomePlanet> planets(count);
    for (std::size_t i = 0; i < count; ++i) {
        Reader rec(data, i * kHomePlanetRecord);
        planets[i].name = rec.text(40);
        planets[i].race = rec.text(40);
        planets[i].kind = rec.u32();
        planets[i].star = rec.u32();
    }
    return planets;
}

std::vector<Tech> readTechTree(ByteView data) {
    const std::size_t count = records(data, 0, kTechRecord, "technology table");
    std::vector<Tech> techs(count);
    for (std::size_t i = 0; i < count; ++i) {
        Reader rec(data, i * kTechRecord);
        techs[i].name = rec.text(20);
        rec.seek(i * kTechRecord + 0x37);
        const std::uint8_t field = rec.u8();
        const std::uint8_t level = rec.u8();
        if (field >= kTechFields || level > 10) {
            throw FormatError("bad technology table");
        }
        techs[i].field = static_cast<TechField>(field);
        techs[i].level = level;
        rec.skip(3);
        techs[i].cost = rec.u32();
        rec.u32();
        techs[i].description = rec.u32();
    }
    return techs;
}

} // namespace nagus
