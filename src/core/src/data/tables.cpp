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

PlanetKinds readPlanetKinds(ByteView data) {
    constexpr std::size_t kHeader = 0x82;
    constexpr std::size_t kRecord = 50;
    Reader r(data);
    const std::size_t count = r.u16();
    if (count != kPlanetTypes + kAtmospheres || data.size() < kHeader + count * kRecord) {
        throw FormatError("bad planet type table");
    }
    PlanetKinds k;
    for (int i = 0; i < kPlanetTypes; ++i) {
        const std::uint32_t bits = r.u32();
        float share = 0;
        std::memcpy(&share, &bits, sizeof share);
        if (!(share >= 0 && share <= 1)) {
            throw FormatError("bad planet type table");
        }
        k.shares.push_back(share);
    }
    for (std::size_t i = 0; i < count; ++i) {
        Reader rec(data, kHeader + i * kRecord);
        k.names.push_back(rec.text(12));
    }
    return k;
}

std::vector<Habitat> readHabitats(ByteView data) {
    const std::size_t count = records(data, 0, 8, "habitat table");
    std::vector<Habitat> h(count);
    for (std::size_t i = 0; i < count; ++i) {
        Reader rec(data, i * 8);
        h[i].type = rec.u8();
        h[i].atmosphere = rec.u8();
        if (h[i].type >= kPlanetTypes || h[i].atmosphere < kPlanetTypes ||
            h[i].atmosphere >= kPlanetTypes + kAtmospheres) {
            throw FormatError("bad habitat table");
        }
        for (auto& v : h[i].values) {
            v = rec.u8();
        }
    }
    return h;
}

std::vector<PlanetRoom> readPlanetRoom(ByteView data) {
    const std::size_t count = records(data, 0, 40, "planet room table");
    std::vector<PlanetRoom> rooms(count);
    for (std::size_t i = 0; i < count; ++i) {
        Reader rec(data, i * 40);
        rooms[i].type = rec.u8();
        rooms[i].size = rec.u8();
        rec.u16();
        if (rooms[i].type >= kPlanetTypes || rooms[i].size > 2) {
            throw FormatError("bad planet room table");
        }
        for (auto& v : rooms[i].population) {
            v = rec.u32();
            if (v > 100000) {
                throw FormatError("bad planet room table");
            }
        }
    }
    return rooms;
}

std::vector<Building> readBuildings(ByteView data) {
    constexpr std::size_t kHeader = 0x2e4;
    constexpr std::size_t kRecord = 176;
    constexpr std::size_t kMaxBuildings = 2048;
    Reader r(data);
    const std::size_t count = r.u32();
    if (count > kMaxBuildings || data.size() < kHeader || count > (data.size() - kHeader) / kRecord) {
        throw FormatError("bad building table");
    }
    std::vector<Building> list(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = kHeader + i * kRecord;
        Reader rec(data, at);
        Building& b = list[i];
        b.name = rec.text(40);
        b.plural = rec.text(40);
        b.id = rec.u16();
        const int from = rec.u16();
        b.upgradesFrom = from == 0x7fff ? -1 : from;
        rec.skip(4);
        b.picture = rec.text(8);
        rec.seek(at + 0x65);
        const std::uint8_t field = rec.u8();
        b.level = rec.u8();
        b.excluded = rec.u8();
        b.minorRaces = rec.u32();
        rec.seek(at + 0x70);
        b.systemKind = rec.u8();
        b.planetType = rec.u8();
        b.bonus = rec.u8();
        const std::uint8_t kind = rec.u8();
        rec.u8(); // which empire's or minor race's style (not used)
        const std::uint8_t output = rec.u8();
        if (field >= kTechFields || kind > 2 || output >= kOutputs) {
            throw FormatError("bad building table");
        }
        b.field = static_cast<TechField>(field);
        b.kind = static_cast<Building::Class>(kind);
        b.output = static_cast<Output>(output);
        rec.seek(at + 0x78);
        b.cost = rec.u32();
        b.energy = rec.u32();
        b.workers = rec.u16();
        rec.seek(at + 0x86);
        b.amount = rec.u16();
    }
    return list;
}

std::vector<GrowthBracket> readGrowth(ByteView data) {
    const std::size_t count = records(data, 0, 12, "growth table");
    std::vector<GrowthBracket> list(count);
    for (std::size_t i = 0; i < count; ++i) {
        Reader rec(data, i * 12);
        list[i].from = rec.u16();
        list[i].to = rec.u16();
        list[i].rate = rec.u32();
    }
    return list;
}

std::vector<ShipClass> readShipClasses(ByteView list, ByteView tech) {
    constexpr std::size_t kShip = 288;
    constexpr std::size_t kTech = 16;
    const std::size_t count = records(list, 0, kShip, "ship table");
    if (records(tech, 0, kTech, "ship technology table") != count) {
        throw FormatError("bad ship technology table");
    }
    std::vector<ShipClass> ships(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = i * kShip;
        Reader rec(list, at);
        ShipClass& s = ships[i];
        s.name = rec.text(40);
        s.className = rec.text(20);
        rec.seek(at + 0x58);
        s.picture = rec.text(8);
        rec.seek(at + 0x68);
        const int role = rec.u16();
        if (role >= kShipRoles) {
            throw FormatError("bad ship table");
        }
        s.role = static_cast<ShipRole>(role);
        rec.seek(at + 0x6c);
        s.beamDamage = static_cast<int>(rec.u32());
        s.beamCount = static_cast<int>(rec.u32());
        rec.seek(at + 0x80);
        s.torpedoDamage = static_cast<int>(rec.u32());
        s.torpedoCount = static_cast<int>(rec.u32());
        rec.seek(at + 0x94);
        s.hull = static_cast<int>(rec.u32());
        s.shields = static_cast<int>(rec.u32());
        rec.u32();
        s.cost = static_cast<int>(rec.u32());
        rec.seek(at + 0x104);
        const std::uint64_t bits = static_cast<std::uint64_t>(rec.u32()) | (static_cast<std::uint64_t>(rec.u32()) << 32);
        double range = 0;
        static_assert(sizeof range == sizeof bits);
        std::memcpy(&range, &bits, sizeof range);
        s.range = range >= 0 && range <= 2 ? static_cast<int>(range) : 0;
        rec.seek(at + 0x114);
        s.scan = rec.u8();
        rec.u8();
        s.capacity = rec.u16();
        s.race = rec.u8();
        if (s.hull < 0 || s.cost < 0 || s.beamCount < 0 || s.torpedoCount < 0) {
            throw FormatError("bad ship table");
        }

        Reader t(tech, i * kTech);
        t.u32(); // the race again
        if (t.u16() != i) {
            throw FormatError("bad ship technology table");
        }
        const int from = t.u16();
        s.upgradesFrom = from == 0xffff || static_cast<std::size_t>(from) >= count ? -1 : from;
        for (auto& level : s.tech) {
            level = t.u8();
        }
    }
    return ships;
}

} // namespace nagus
