// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/game.hpp"

#include <algorithm>
#include <cctype>

namespace nagus {

namespace {

std::string planetAnimationByKind(const Planet& planet, const std::vector<std::string>& names) {
    static constexpr char kTypes[] = "abdgjotv";
    static constexpr char kAirs[] = "mnrst"; // methane, none, rich, sulfuric, thin
    static constexpr char kSizes[] = "sml";
    if (planet.type < 0 || planet.type >= kPlanetTypes) {
        return {};
    }
    const char type = kTypes[planet.type];
    const char atmosphere = planet.atmosphere >= 8 && planet.atmosphere < 8 + kAtmospheres ? kAirs[planet.atmosphere - 8] : '?';
    const char size = kSizes[std::clamp(planet.size, 0, 2)];
    // The best match: type, atmosphere and size; then type and size; then type.
    for (int level = 0; level < 3; ++level) {
        std::vector<std::string> found;
        for (const auto& n : names) {
            if (n.size() != 8 || n.compare(4, 4, ".ani") != 0 || n[0] != type) {
                continue;
            }
            if ((level == 0 && (n[1] != atmosphere || n[3] != size)) || (level == 1 && n[3] != size)) {
                continue;
            }
            found.push_back(n);
        }
        if (!found.empty()) {
            return found[static_cast<std::size_t>(std::max(0, planet.variant)) % found.size()];
        }
    }
    return {};
}

} // namespace

std::string planetAnimation(const Planet& planet, const std::vector<std::string>& names) {
    // Named planets of one word (Earth, Mars) may have their own.
    if (planet.name.size() >= 4 && planet.name.find(' ') == std::string::npos) {
        std::string own;
        for (const char c : planet.name.substr(0, 4)) {
            own.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        own += ".ani";
        if (std::find(names.begin(), names.end(), own) != names.end()) {
            return own;
        }
    }
    return planetAnimationByKind(planet, names);
}

GalaxyData loadGalaxyData(const Resources& resources, const Lexicon& lexicon) {
    GalaxyData d;
    d.kinds = readStarObjectKinds(resources.file("objstruc.smt"));
    d.starNames = readStarNames(resources.file("starname.bin"));
    d.races = readRaces(resources.file("race.rst"));
    d.planetKinds = readPlanetKinds(resources.file("environ.est"));
    d.habitats = readHabitats(resources.file("habit.bin"));
    d.rooms = readPlanetRoom(resources.file("planspac.bin"));
    d.homePlanets = readHomePlanets(resources.file("meplanet.bin"));
    for (int e = 0; e < kEmpires; ++e) {
        d.homeNames[static_cast<std::size_t>(e)] = lexicon[text::kHomeSystems[e]];
    }
    return d;
}

EconomyData loadEconomyData(const Resources& resources) {
    EconomyData d;
    d.buildings = readBuildings(resources.file("edifice.bst"));
    d.growth = readGrowth(resources.file("ppgrowth.bin"));
    d.habitats = readHabitats(resources.file("habit.bin"));
    return d;
}

Colony* Game::colonyAt(int system) {
    for (auto& c : colonies) {
        if (c.system == system) {
            return &c;
        }
    }
    return nullptr;
}

const Colony* Game::colonyAt(int system) const { return const_cast<Game*>(this)->colonyAt(system); }

TurnSummary endTurn(Game& game) {
    TurnSummary summary;
    for (auto& c : game.colonies) {
        const TurnReport r = colonyTurn(c, game.economy);
        if (c.owner < 0 || c.owner >= kEmpires) {
            continue;
        }
        auto& e = game.empires[static_cast<std::size_t>(c.owner)];
        e.credits += r.credits;
        e.researchPoints += r.research;
        e.intelPoints += r.intel;
        if (c.owner == game.player) {
            const std::string& where = game.galaxy.systems[static_cast<std::size_t>(c.system)].name;
            for (const auto& f : r.finished) {
                summary.events.push_back(where + ": " + f);
            }
        }
    }
    game.turn += 1;
    return summary;
}

Game newGame(const GameSettings& settings, const GalaxyData& data, const EconomyData& economy, std::uint32_t seed) {
    Game g;
    g.settings = settings;
    g.player = std::clamp(settings.empire, 0, kEmpires - 1);
    GalaxySetup setup;
    setup.shape = static_cast<GalaxyShape>(std::clamp(settings.galaxyShape, 0, 3));
    setup.size = static_cast<GalaxySize>(std::clamp(settings.galaxySize, 0, 2));
    setup.minors = static_cast<MinorRaces>(std::clamp(settings.minorRaces, 0, 3));
    setup.seed = seed;
    g.galaxy = makeGalaxy(setup, data);
    for (const auto& k : data.kinds) {
        g.kindCodes.push_back(k.code);
    }
    for (const auto& r : data.races) {
        g.raceNames.push_back(r.name);
    }
    g.economy = economy;
    for (int e = 0; e < kEmpires; ++e) {
        auto& state = g.empires[static_cast<std::size_t>(e)];
        state.tech.fill(kStartTech[std::clamp(settings.civilization[static_cast<std::size_t>(e)], 0, GameSettings::kCivilizations - 1)]);
        state.credits = 1000;
        const int home = g.galaxy.homes[static_cast<std::size_t>(e)];
        if (home >= 0) {
            g.colonies.push_back(homeColony(home, g.galaxy.systems[static_cast<std::size_t>(home)], e, state, economy));
        }
    }
    return g;
}

} // namespace nagus
