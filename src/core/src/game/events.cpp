// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/events.hpp"

#include <algorithm>

namespace nagus {

namespace {

// Lexicon numbers of the event texts.
constexpr std::size_t kPlague = 909;        // "Plague has stricken the citizens of %1.  The death toll is estimated at %d2 million."
constexpr std::size_t kQuakes = 254;        // "... devastated %1.  %d2 structures were destroyed ... %d3 million people were killed."
constexpr std::size_t kQuake = 255;         // the same for one structure
constexpr std::size_t kComet = 176;         // "A comet has struck the %1 planet of the %2 system. ... %d3 million casualties ..."
constexpr std::size_t kImmigrants = 1021;   // "Immigrants flock to %s; +%d million citizens."
constexpr std::size_t kTerrorists = 1263;   // "Terrorists have destroyed a %1 in the %2 system."
constexpr std::size_t kVirus = 1124;        // "%s1 Ships in sector %2 infected with Hull Degrading Virus."

std::uint32_t mix(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    std::uint32_t h = a * 0x9e3779b1u ^ (b + 0x632be5abu) * 0x85ebca6bu ^ (c + 0x7f4a7c15u) * 0xc2b2ae35u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    return h;
}

int share(int people, int percent) { return std::max(1, people * percent / 100); }

} // namespace

std::vector<GameEvent> randomEvents(Game& g) {
    std::vector<GameEvent> heard;
    if (!g.settings.randomEvents) {
        return heard;
    }
    for (int e = 0; e < kEmpires; ++e) {
        std::uint32_t h = mix(static_cast<std::uint32_t>(g.turn), static_cast<std::uint32_t>(e), 0x5eed);
        if (static_cast<int>(h % 100) >= kEventChance) {
            continue;
        }
        h = mix(h, 1, 2);
        std::vector<std::size_t> mine;
        for (std::size_t i = 0; i < g.colonies.size(); ++i) {
            if (g.colonies[i].owner == e) {
                mine.push_back(i);
            }
        }
        if (mine.empty()) {
            continue;
        }
        Colony& c = g.colonies[mine[h % mine.size()]];
        const StarSystem& s = g.galaxy.systems[static_cast<std::size_t>(c.system)];
        GameEvent ev;
        ev.empire = e;
        ev.system = c.system;
        switch ((h >> 8) % 6) {
        case 0: { // plague
            const int dead = std::min(c.population - 1, share(c.population, 10));
            if (dead <= 0) {
                continue;
            }
            c.population -= dead;
            ev.text = kPlague;
            ev.values = {s.name, std::to_string(dead)};
            break;
        }
        case 1: { // earthquakes
            const int dead = std::min(c.population - 1, share(c.population, 5));
            int lost = 0;
            for (int k = 0; k < 2 && !c.structures.empty(); ++k) {
                c.structures.erase(c.structures.begin() + static_cast<std::ptrdiff_t>((h >> (12 + k)) % c.structures.size()));
                ++lost;
            }
            if (lost == 0 || dead <= 0) {
                continue;
            }
            c.population -= dead;
            ev.text = lost == 1 ? kQuake : kQuakes;
            ev.values = {s.name, std::to_string(lost), std::to_string(dead)};
            break;
        }
        case 2: { // a comet
            const int dead = std::min(c.population - 1, share(c.population, 10));
            if (dead <= 0 || s.planets.empty()) {
                continue;
            }
            c.population -= dead;
            ev.text = kComet;
            ev.values = {s.planets[(h >> 12) % s.planets.size()].name, s.name, std::to_string(dead)};
            break;
        }
        case 3: { // immigrants
            const int more = std::min(share(c.population, 10), c.maxPopulation - c.population);
            if (more <= 0) {
                continue;
            }
            c.population += more;
            ev.text = kImmigrants;
            ev.values = {s.name, std::to_string(more)};
            break;
        }
        case 4: { // terrorists
            if (c.structures.empty()) {
                continue;
            }
            const auto at = c.structures.begin() + static_cast<std::ptrdiff_t>((h >> 12) % c.structures.size());
            const int b = at->building;
            c.structures.erase(at);
            ev.text = kTerrorists;
            ev.values = {b >= 0 && static_cast<std::size_t>(b) < g.economy.buildings.size() ? g.economy.buildings[static_cast<std::size_t>(b)].name : "?",
                         s.name};
            break;
        }
        default: { // a hull virus in one of the empire's fleets
            std::vector<Fleet*> fleets;
            for (auto& f : g.fleets) {
                if (f.owner == e && !f.ships.empty()) {
                    fleets.push_back(&f);
                }
            }
            if (fleets.empty()) {
                continue;
            }
            Fleet& f = *fleets[(h >> 12) % fleets.size()];
            for (auto& ship : f.ships) {
                ship.hull = std::max(1, ship.hull * 3 / 4);
            }
            ev.text = kVirus;
            ev.system = -1;
            ev.values = {std::to_string(f.ships.size()), std::to_string(f.at.x + 1) + "." + std::to_string(f.at.y + 1)};
            break;
        }
        }
        if (e == g.player) {
            heard.push_back(ev);
        }
    }
    return heard;
}

} // namespace nagus
