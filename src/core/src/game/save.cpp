// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/save.hpp"

#include <cstdio>
#include <fstream>

namespace nagus {

namespace {

constexpr std::string_view kMagic = "NAGUSSAV";
constexpr std::uint32_t kVersion = 1;
// Limits for what a saved game may hold, so a damaged file cannot ask for
// huge amounts of memory.
constexpr std::uint32_t kMaxSystems = 10000;
constexpr std::uint32_t kMaxPlanets = 64;
constexpr std::uint32_t kMaxNames = 1000;
constexpr std::uint32_t kMaxColonies = 10000;
constexpr std::uint32_t kMaxFleets = 100000;
constexpr std::uint32_t kMaxShips = 10000;
constexpr std::uint32_t kMaxList = 1000;
constexpr int kMaxSide = 1000;
constexpr std::uintmax_t kMaxFileBytes = 64u * 1024 * 1024;

std::string readText(Reader& r) {
    const std::size_t n = r.u16();
    const ByteView b = r.bytes(n);
    return std::string(b.begin(), b.end());
}

std::uint32_t count(Reader& r, std::uint32_t max) {
    const std::uint32_t n = r.u32();
    if (n > max) {
        throw FormatError("damaged saved game");
    }
    return n;
}

int number(Reader& r, int lo, int hi) {
    const auto v = static_cast<std::int32_t>(r.u32());
    if (v < lo || v > hi) {
        throw FormatError("damaged saved game");
    }
    return v;
}

void writeSector(Writer& w, Sector s) {
    w.i32(s.x);
    w.i32(s.y);
}

Sector readSector(Reader& r) {
    Sector s;
    s.x = number(r, -1, kMaxSide);
    s.y = number(r, -1, kMaxSide);
    return s;
}

constexpr int kBig = 1 << 30;

} // namespace

Bytes saveGame(const Game& g, const SaveInfo& info) {
    Writer w;
    w.bytes(kMagic);
    w.u32(kVersion);
    w.text(info.name);
    w.text(info.description);
    w.i32(g.turn);

    const GameSettings& s = g.settings;
    for (const int v : {s.galaxyShape, s.galaxySize, s.minorRaces, s.difficulty, s.victory, s.empire}) {
        w.i32(v);
    }
    for (const int v : s.civilization) {
        w.i32(v);
    }
    w.u8(s.randomEvents ? 1 : 0);
    w.u8(s.automaticCombat ? 1 : 0);
    w.i32(g.player);

    w.u32(static_cast<std::uint32_t>(g.kindCodes.size()));
    for (const auto& k : g.kindCodes) {
        w.text(k);
    }
    w.u32(static_cast<std::uint32_t>(g.raceNames.size()));
    for (const auto& n : g.raceNames) {
        w.text(n);
    }

    const Galaxy& gal = g.galaxy;
    w.i32(gal.width);
    w.i32(gal.height);
    for (const int h : gal.homes) {
        w.i32(h);
    }
    w.u32(static_cast<std::uint32_t>(gal.systems.size()));
    for (const auto& sys : gal.systems) {
        writeSector(w, sys.sector);
        w.i32(sys.kind);
        w.text(sys.name);
        w.i32(sys.race);
        w.u8(sys.star ? 1 : 0);
        w.u32(static_cast<std::uint32_t>(sys.planets.size()));
        for (const auto& p : sys.planets) {
            w.text(p.name);
            for (const int v : {p.type, p.atmosphere, p.size, p.room, p.variant}) {
                w.i32(v);
            }
        }
    }

    w.u32(static_cast<std::uint32_t>(g.colonies.size()));
    for (const auto& c : g.colonies) {
        for (const int v : {c.system, c.owner, c.population, c.maxPopulation, c.food, c.progress}) {
            w.i32(v);
        }
        w.u8(c.home ? 1 : 0);
        for (int o = 0; o < kOutputs; ++o) {
            const auto i = static_cast<std::size_t>(o);
            w.i32(c.facility[i]);
            w.i32(c.facilities[i]);
            w.i32(c.working[i]);
        }
        w.u32(static_cast<std::uint32_t>(c.structures.size()));
        for (const auto& st : c.structures) {
            w.i32(st.building);
            w.u8(st.on ? 1 : 0);
        }
        w.u32(static_cast<std::uint32_t>(c.queue.size()));
        for (const auto& q : c.queue) {
            w.u8(static_cast<std::uint32_t>(q.kind));
            w.i32(q.building);
        }
    }

    for (const auto& e : g.empires) {
        w.i32(e.credits);
        for (const int v : e.tech) {
            w.i32(v);
        }
        for (const int v : e.progress) {
            w.i32(v);
        }
        for (const int v : e.share) {
            w.i32(v);
        }
        for (const bool v : e.locked) {
            w.u8(v ? 1 : 0);
        }
        w.i32(e.researchPoints);
        w.i32(e.intelPoints);
    }

    w.u32(static_cast<std::uint32_t>(g.explored.size()));
    for (const std::uint8_t b : g.explored) {
        w.u8(b);
    }

    w.i32(g.nextFleetId);
    w.u32(static_cast<std::uint32_t>(g.fleets.size()));
    for (const auto& f : g.fleets) {
        w.i32(f.id);
        w.i32(f.owner);
        writeSector(w, f.at);
        w.u8(f.destination ? 1 : 0);
        writeSector(w, f.destination.value_or(Sector{-1, -1}));
        w.u8(static_cast<std::uint32_t>(f.order));
        w.u32(static_cast<std::uint32_t>(f.ships.size()));
        for (const auto& ship : f.ships) {
            w.i32(ship.shipClass);
            w.i32(ship.hull);
        }
    }
    return w.take();
}

SaveInfo readSaveInfo(ByteView data) {
    Reader r(data);
    const ByteView magic = r.bytes(kMagic.size());
    if (std::string_view(reinterpret_cast<const char*>(magic.data()), magic.size()) != kMagic || r.u32() != kVersion) {
        throw FormatError("not a saved game of this version");
    }
    SaveInfo info;
    info.name = readText(r);
    info.description = readText(r);
    info.turn = number(r, 0, kBig);
    return info;
}

Game loadGame(ByteView data, const EconomyData& economy) {
    Reader r(data);
    const SaveInfo info = readSaveInfo(data);
    r.seek(kMagic.size() + 4);
    readText(r);
    readText(r);
    r.u32();

    Game g;
    g.turn = info.turn;
    g.economy = economy;
    GameSettings& s = g.settings;
    for (int* v : {&s.galaxyShape, &s.galaxySize, &s.minorRaces, &s.difficulty, &s.victory, &s.empire}) {
        *v = number(r, 0, 100);
    }
    for (int& v : s.civilization) {
        v = number(r, 0, 100);
    }
    s.randomEvents = r.u8() != 0;
    s.automaticCombat = r.u8() != 0;
    s.clamp();
    g.player = number(r, 0, kEmpires - 1);

    for (std::uint32_t i = 0, n = count(r, kMaxNames); i < n; ++i) {
        g.kindCodes.push_back(readText(r));
    }
    for (std::uint32_t i = 0, n = count(r, kMaxNames); i < n; ++i) {
        g.raceNames.push_back(readText(r));
    }

    Galaxy& gal = g.galaxy;
    gal.width = number(r, 1, kMaxSide);
    gal.height = number(r, 1, kMaxSide);
    for (int& h : gal.homes) {
        h = number(r, -1, static_cast<int>(kMaxSystems));
    }
    const std::uint32_t systems = count(r, kMaxSystems);
    for (std::uint32_t i = 0; i < systems; ++i) {
        StarSystem sys;
        sys.sector = readSector(r);
        if (!gal.inside(sys.sector)) {
            throw FormatError("damaged saved game");
        }
        sys.kind = number(r, 0, static_cast<int>(g.kindCodes.size()));
        sys.name = readText(r);
        sys.race = number(r, -1, static_cast<int>(kMaxNames));
        sys.star = r.u8() != 0;
        for (std::uint32_t k = 0, n = count(r, kMaxPlanets); k < n; ++k) {
            Planet p;
            p.name = readText(r);
            p.type = number(r, 0, kPlanetTypes - 1);
            p.atmosphere = number(r, 8, 8 + kAtmospheres - 1);
            p.size = number(r, 0, 2);
            p.room = number(r, 0, kBig);
            p.variant = number(r, 0, 100);
            sys.planets.push_back(p);
        }
        gal.systems.push_back(std::move(sys));
    }
    for (const int h : gal.homes) {
        if (h >= static_cast<int>(systems)) {
            throw FormatError("damaged saved game");
        }
    }

    const int lastSystem = static_cast<int>(systems) - 1;
    const int lastBuilding = static_cast<int>(economy.buildings.size()) - 1;
    const int lastShip = static_cast<int>(economy.ships.size()) - 1;
    for (std::uint32_t i = 0, n = count(r, kMaxColonies); i < n; ++i) {
        Colony c;
        c.system = number(r, 0, lastSystem);
        c.owner = number(r, 0, static_cast<int>(kMaxNames));
        c.population = number(r, 0, kBig);
        c.maxPopulation = number(r, 0, kBig);
        c.food = number(r, -kBig, kBig);
        c.progress = number(r, 0, kBig);
        c.home = r.u8() != 0;
        for (int o = 0; o < kOutputs; ++o) {
            const auto k = static_cast<std::size_t>(o);
            c.facility[k] = number(r, -1, lastBuilding);
            c.facilities[k] = number(r, 0, kBig);
            c.working[k] = number(r, 0, c.facilities[k]);
        }
        for (std::uint32_t k = 0, m = count(r, kMaxList); k < m; ++k) {
            Structure st;
            st.building = number(r, 0, lastBuilding);
            st.on = r.u8() != 0;
            c.structures.push_back(st);
        }
        for (std::uint32_t k = 0, m = count(r, kMaxList); k < m; ++k) {
            BuildItem q;
            const int kind = r.u8();
            if (kind > static_cast<int>(BuildItem::Kind::Ship)) {
                throw FormatError("damaged saved game");
            }
            q.kind = static_cast<BuildItem::Kind>(kind);
            q.building = number(r, 0, q.kind == BuildItem::Kind::Ship ? lastShip : lastBuilding);
            c.queue.push_back(q);
        }
        g.colonies.push_back(std::move(c));
    }

    for (auto& e : g.empires) {
        e.credits = number(r, -kBig, kBig);
        for (int& v : e.tech) {
            v = number(r, 0, kMaxTechLevel);
        }
        for (int& v : e.progress) {
            v = number(r, 0, kBig);
        }
        for (int& v : e.share) {
            v = number(r, 0, 100);
        }
        for (auto&& v : e.locked) {
            v = r.u8() != 0;
        }
        e.researchPoints = number(r, 0, kBig);
        e.intelPoints = number(r, 0, kBig);
    }

    const std::uint32_t sectors = count(r, static_cast<std::uint32_t>(gal.width * gal.height));
    const ByteView seen = r.bytes(sectors);
    if (sectors != 0 && sectors != static_cast<std::uint32_t>(gal.width * gal.height)) {
        throw FormatError("damaged saved game");
    }
    g.explored.assign(seen.begin(), seen.end());

    g.nextFleetId = number(r, 1, kBig);
    for (std::uint32_t i = 0, n = count(r, kMaxFleets); i < n; ++i) {
        Fleet f;
        f.id = number(r, 1, g.nextFleetId - 1);
        f.owner = number(r, 0, static_cast<int>(kMaxNames));
        f.at = readSector(r);
        const bool going = r.u8() != 0;
        const Sector to = readSector(r);
        if (!gal.inside(f.at) || (going && !gal.inside(to))) {
            throw FormatError("damaged saved game");
        }
        if (going) {
            f.destination = to;
        }
        const int order = r.u8();
        if (order > static_cast<int>(Fleet::Order::Colonize)) {
            throw FormatError("damaged saved game");
        }
        f.order = static_cast<Fleet::Order>(order);
        for (std::uint32_t k = 0, m = count(r, kMaxShips); k < m; ++k) {
            Ship ship;
            ship.shipClass = number(r, 0, lastShip);
            ship.hull = number(r, 0, kBig);
            f.ships.push_back(ship);
        }
        g.fleets.push_back(std::move(f));
    }
    if (r.left() != 0) {
        throw FormatError("damaged saved game");
    }
    return g;
}

std::filesystem::path saveSlotFile(const std::filesystem::path& folder, int slot) {
    char name[32];
    std::snprintf(name, sizeof name, "slot%02d.sav", slot + 1);
    return folder / name;
}

Bytes readSaveFile(const std::filesystem::path& file) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec || size > kMaxFileBytes) {
        throw FormatError("cannot read saved game");
    }
    std::ifstream in(file, std::ios::binary);
    Bytes data(static_cast<std::size_t>(size));
    if (!in || !in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        throw FormatError("cannot read saved game");
    }
    return data;
}

SaveSlot readSaveSlot(const std::filesystem::path& file) {
    SaveSlot slot;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) {
        return slot;
    }
    slot.used = true;
    try {
        slot.info = readSaveInfo(readSaveFile(file));
    } catch (const FormatError&) {
        slot.damaged = true;
    }
    return slot;
}

bool writeSaveFile(const std::filesystem::path& file, const Bytes& data) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    const auto part = std::filesystem::path(file).concat(".part");
    {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!out.flush()) {
            std::filesystem::remove(part, ec);
            return false;
        }
    }
    std::filesystem::rename(part, file, ec);
    return !ec;
}

} // namespace nagus
