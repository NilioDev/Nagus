// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.hpp"

#include "nagus/game/galaxy.hpp"

#include <cstdio>
#include <cstdlib>
#include <set>

using namespace nagus;

namespace {

GalaxyData sampleData() {
    GalaxyData d;
    for (const char* code : {"blkh", "neb-", "s-r-", "s-y-", "s-b-"}) {
        d.kinds.push_back({code, 0.2f});
    }
    for (int i = 0; i < 400; ++i) {
        d.starNames.push_back("Star " + std::to_string(i));
    }
    for (const char* r : {"Cardassians", "Federation", "Ferengi", "Klingons", "Romulans"}) {
        d.races.push_back({r, ""});
    }
    for (int i = 0; i < 30; ++i) {
        d.races.push_back({"Minor " + std::to_string(i), ""});
    }
    d.homeNames = {"Cardassia", "Sol", "Ferenginar", "Qo'nos", "Romulus"};
    return d;
}

} // namespace

NGS_TEST("a new galaxy has every empire's home far apart, minor races and names") {
    const GalaxyData data = sampleData();
    for (int shape = 0; shape < 4; ++shape) {
        for (int size = 0; size < 3; ++size) {
            GalaxySetup setup;
            setup.shape = static_cast<GalaxyShape>(shape);
            setup.size = static_cast<GalaxySize>(size);
            setup.minors = MinorRaces::Many;
            setup.seed = 7 + static_cast<std::uint32_t>(shape * 3 + size);
            const Galaxy g = makeGalaxy(setup, data);
            const auto [w, h] = galaxyDimensions(setup.size);
            NGS_CHECK(g.width == w && g.height == h);
            NGS_CHECK(static_cast<int>(g.systems.size()) > w * h / 10);
            std::set<std::pair<int, int>> sectors;
            std::set<std::string> names;
            int minors = 0;
            for (const auto& s : g.systems) {
                NGS_CHECK(g.inside(s.sector));
                NGS_CHECK(sectors.insert({s.sector.x, s.sector.y}).second);
                if (s.star) {
                    NGS_CHECK(!s.name.empty() && names.insert(s.name).second);
                }
                minors += s.race >= kEmpires ? 1 : 0;
            }
            NGS_CHECK(minors == minorRaceCount(setup.size, setup.minors, 30));
            for (int a = 0; a < kEmpires; ++a) {
                const int ha = g.homes[static_cast<std::size_t>(a)];
                NGS_CHECK(ha >= 0 && g.systems[static_cast<std::size_t>(ha)].race == a);
                NGS_CHECK(g.systems[static_cast<std::size_t>(ha)].name == data.homeNames[static_cast<std::size_t>(a)]);
                for (int b = a + 1; b < kEmpires; ++b) {
                    const Sector sa = g.systems[static_cast<std::size_t>(ha)].sector;
                    const Sector sb = g.systems[static_cast<std::size_t>(g.homes[static_cast<std::size_t>(b)])].sector;
                    NGS_CHECK(std::max(std::abs(sa.x - sb.x), std::abs(sa.y - sb.y)) >= w / 6);
                }
            }
        }
    }
}

NGS_TEST("the same seed makes the same galaxy") {
    const GalaxyData data = sampleData();
    GalaxySetup setup;
    setup.shape = GalaxyShape::Spiral;
    setup.seed = 1234;
    const Galaxy a = makeGalaxy(setup, data);
    const Galaxy b = makeGalaxy(setup, data);
    NGS_CHECK(a.systems.size() == b.systems.size());
    for (std::size_t i = 0; i < a.systems.size(); ++i) {
        NGS_CHECK(a.systems[i].sector == b.systems[i].sector && a.systems[i].name == b.systems[i].name);
    }
    // The first numbers of the generator are fixed, on every system.
    Random r(5);
    NGS_CHECK(r.below(1000) == Random(5).below(1000));
    if (std::getenv("NAGUS_PRINT_GALAXY") != nullptr) {
        for (int shape = 0; shape < 4; ++shape) {
            setup.shape = static_cast<GalaxyShape>(shape);
            setup.size = GalaxySize::Large;
            const Galaxy g = makeGalaxy(setup, data);
            for (int y = 0; y < g.height; ++y) {
                for (int x = 0; x < g.width; ++x) {
                    const int i = g.systemAt({x, y});
                    char c = '.';
                    if (i >= 0) {
                        const auto& s = g.systems[static_cast<std::size_t>(i)];
                        c = s.race >= 0 && s.race < kEmpires ? static_cast<char>('A' + s.race) : s.race >= 0 ? 'm' : s.star ? '*' : 'o';
                    }
                    std::putchar(c);
                }
                std::putchar('\n');
            }
            std::putchar('\n');
        }
    }
}

NGS_TEST("stars have planets, and the empires their own home systems") {
    GalaxyData data = sampleData();
    data.planetKinds.shares = {0.1f, 0.2f, 0.1f, 0.2f, 0.1f, 0.1f, 0.1f, 0.1f};
    for (int t = 0; t < kPlanetTypes; ++t) {
        data.habitats.push_back({t, 8 + t % kAtmospheres, {}});
        for (int size = 0; size < 3; ++size) {
            if (t != kGasGiant) {
                PlanetRoom r{t, size, {}};
                r.population.fill(static_cast<std::uint32_t>(10 + size * 5));
                data.rooms.push_back(r);
            }
        }
    }
    data.homePlanets = {{"Mercury", "Federation", 0, 0}, {"Earth", "Federation", 0, 0}, {"Jupiter", "Federation", 10, 0}};
    GalaxySetup setup;
    setup.seed = 99;
    const Galaxy g = makeGalaxy(setup, data);
    for (const auto& s : g.systems) {
        NGS_CHECK(s.star ? !s.planets.empty() && s.planets.size() <= 8 : s.planets.empty());
        for (const auto& p : s.planets) {
            NGS_CHECK(!p.name.empty() && p.type >= 0 && p.type < kPlanetTypes);
            NGS_CHECK(p.atmosphere >= 8 && p.atmosphere < 8 + kAtmospheres);
            NGS_CHECK((p.type == kGasGiant) == (p.room == 0));
        }
        if (s.race >= kEmpires) {
            bool livable = false;
            for (const auto& p : s.planets) {
                livable = livable || p.room > 0;
            }
            NGS_CHECK(livable);
        }
    }
    const auto& sol = g.systems[static_cast<std::size_t>(g.homes[1])];
    NGS_CHECK(sol.planets.size() == 3 && sol.planets[1].name == "Earth" && sol.planets[1].type == 6);
    NGS_CHECK(sol.planets[2].type == kGasGiant && sol.planets[2].room == 0);
    NGS_CHECK(g.systems[static_cast<std::size_t>(g.homes[0])].planets.front().name == "Cardassia I");
}
