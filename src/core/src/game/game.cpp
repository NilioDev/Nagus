// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/game.hpp"

#include <algorithm>

namespace nagus {

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

Game newGame(const GameSettings& settings, const GalaxyData& data, std::uint32_t seed) {
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
    return g;
}

} // namespace nagus
