// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/game.hpp"

#include "nagus/game/ai.hpp"
#include "nagus/game/combat.hpp"
#include "nagus/game/diplomacy.hpp"
#include "nagus/game/events.hpp"
#include "nagus/game/intel.hpp"
#include "nagus/game/research.hpp"
#include "nagus/game/victory.hpp"

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
    d.techs = readTechTree(resources.file("techtree.tec"));
    d.buildings = readBuildings(resources.file("edifice.bst"));
    d.ships = readShipClasses(resources.file("shiplist.sst"), resources.file("shiptech.sst"));
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

Fleet* Game::fleet(int id) {
    for (auto& f : fleets) {
        if (f.id == id) {
            return &f;
        }
    }
    return nullptr;
}

const Fleet* Game::fleet(int id) const { return const_cast<Game*>(this)->fleet(id); }

std::vector<int> Game::fleetsAt(Sector s, int owner) const {
    std::vector<int> ids;
    for (const auto& f : fleets) {
        if (f.at == s && (owner < 0 || f.owner == owner)) {
            ids.push_back(f.id);
        }
    }
    return ids;
}

Fleet& Game::addFleet(int owner, Sector at, int shipClass) {
    Fleet f;
    f.id = nextFleetId++;
    f.owner = owner;
    f.at = at;
    int hull = 0;
    if (shipClass >= 0 && static_cast<std::size_t>(shipClass) < economy.ships.size()) {
        hull = economy.ships[static_cast<std::size_t>(shipClass)].hull;
    }
    f.ships.push_back({shipClass, hull});
    fleets.push_back(f);
    return fleets.back();
}

bool visibleTo(const Game& game, int empire, Sector s) {
    if (distanceFromColonies(s, empire, game.colonies, game.galaxy) <= 2) {
        return true;
    }
    for (const auto& f : game.fleets) {
        if (f.owner != empire) {
            continue;
        }
        int scan = 0;
        for (const auto& ship : f.ships) {
            if (ship.shipClass >= 0 && static_cast<std::size_t>(ship.shipClass) < game.economy.ships.size()) {
                scan = std::max(scan, game.economy.ships[static_cast<std::size_t>(ship.shipClass)].scan);
            }
        }
        if (distance(f.at, s) <= 1 + scan) {
            return true;
        }
    }
    return false;
}

bool exploredBy(const Game& game, int empire, Sector s) {
    if (!game.galaxy.inside(s) || empire < 0 || empire >= kEmpires) {
        return false;
    }
    const auto i = static_cast<std::size_t>(s.y * game.galaxy.width + s.x);
    return i < game.explored.size() && (game.explored[i] & (1u << empire)) != 0;
}

void explore(Game& game) {
    const Galaxy& gal = game.galaxy;
    game.explored.resize(static_cast<std::size_t>(gal.width * gal.height));
    auto mark = [&](int owner, Sector centre, int radius) {
        if (owner < 0 || owner >= kEmpires) {
            return;
        }
        for (int y = centre.y - radius; y <= centre.y + radius; ++y) {
            for (int x = centre.x - radius; x <= centre.x + radius; ++x) {
                if (gal.inside({x, y})) {
                    game.explored[static_cast<std::size_t>(y * gal.width + x)] |= static_cast<std::uint8_t>(1u << owner);
                }
            }
        }
    };
    for (const auto& c : game.colonies) {
        mark(c.owner, gal.systems[static_cast<std::size_t>(c.system)].sector, 2);
    }
    for (const auto& f : game.fleets) {
        int scan = 0;
        for (const auto& ship : f.ships) {
            if (ship.shipClass >= 0 && static_cast<std::size_t>(ship.shipClass) < game.economy.ships.size()) {
                scan = std::max(scan, game.economy.ships[static_cast<std::size_t>(ship.shipClass)].scan);
            }
        }
        mark(f.owner, f.at, 1 + scan);
    }
}

bool orderMove(Game& game, int id, Sector to) {
    Fleet* f = game.fleet(id);
    if (f == nullptr || !game.galaxy.inside(to) || f->owner < 0 || f->owner >= kEmpires) {
        return false;
    }
    const auto& empire = game.empires[static_cast<std::size_t>(f->owner)];
    if (fleetSpeed(*f, game.economy, empire) <= 0 ||
        distanceFromColonies(to, f->owner, game.colonies, game.galaxy) > fleetRange(*f, game.economy, empire)) {
        return false;
    }
    f->destination = to == f->at ? std::nullopt : std::optional<Sector>(to);
    f->order = Fleet::Order::None;
    return true;
}

bool orderColonize(Game& game, int id) {
    Fleet* f = game.fleet(id);
    if (f == nullptr || !canColonize(*f, game.galaxy, game.colonies, game.economy)) {
        return false;
    }
    f->destination.reset();
    f->order = Fleet::Order::Colonize;
    return true;
}

namespace {

void moveFleets(Game& game) {
    for (auto& f : game.fleets) {
        if (!f.destination || f.owner < 0 || f.owner >= kEmpires) {
            continue;
        }
        const int speed = fleetSpeed(f, game.economy, game.empires[static_cast<std::size_t>(f.owner)]);
        const auto steps = path(f.at, *f.destination);
        if (steps.empty()) {
            f.destination.reset();
            continue;
        }
        const auto n = static_cast<std::size_t>(std::min<int>(speed, static_cast<int>(steps.size())));
        if (n > 0) {
            f.at = steps[n - 1];
        }
        if (f.at == *f.destination) {
            f.destination.reset();
        }
    }
}

void colonize(Game& game, TurnSummary& summary) {
    for (auto& f : game.fleets) {
        if (f.order != Fleet::Order::Colonize) {
            continue;
        }
        f.order = Fleet::Order::None;
        if (!canColonize(f, game.galaxy, game.colonies, game.economy)) {
            continue;
        }
        // The colony ship stays as the colony's first people.
        const auto ship = std::find_if(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
            return game.economy.ships[static_cast<std::size_t>(s.shipClass)].role == ShipRole::Colony;
        });
        const int colonists = std::max(1, game.economy.ships[static_cast<std::size_t>(ship->shipClass)].capacity);
        f.ships.erase(ship);
        const int at = game.galaxy.systemAt(f.at);
        game.colonies.push_back(foundColony(at, game.galaxy.systems[static_cast<std::size_t>(at)], f.owner, colonists,
                                            game.empires[static_cast<std::size_t>(f.owner)], game.economy));
        if (f.owner == game.player) {
            summary.colonized.push_back(game.galaxy.systems[static_cast<std::size_t>(at)].name);
        }
    }
    std::erase_if(game.fleets, [](const Fleet& f) { return f.ships.empty(); });
}

} // namespace

TurnSummary beginEndTurn(Game& game) {
    TurnSummary summary;
    // The computer players give their orders.
    for (int e = 0; e < kEmpires; ++e) {
        if (e != game.player) {
            playEmpire(game, e);
        }
    }
    for (auto& c : game.colonies) {
        const TurnReport r = colonyTurn(c, game.economy);
        if (c.owner < 0 || c.owner >= kEmpires) {
            continue;
        }
        auto& e = game.empires[static_cast<std::size_t>(c.owner)];
        e.credits += r.credits;
        e.researchPoints += r.research;
        e.intelPoints += r.intel;
        const Sector sector = game.galaxy.systems[static_cast<std::size_t>(c.system)].sector;
        for (const int ship : r.ships) {
            game.addFleet(c.owner, sector, ship);
        }
        if (c.owner == game.player) {
            const std::string& where = game.galaxy.systems[static_cast<std::size_t>(c.system)].name;
            for (const auto& f : r.finished) {
                summary.events.push_back(where + ": " + f);
            }
        }
    }
    colonize(game, summary);
    moveFleets(game);
    return summary;
}

void finishEndTurn(Game& game, TurnSummary& summary, const std::vector<Battle>& fought) {
    std::vector<Sector> skip;
    for (const auto& b : fought) {
        skip.push_back(b.at);
    }
    auto battles = fightBattles(game, skip);
    battles.insert(battles.begin(), fought.begin(), fought.end());
    for (const auto& b : battles) {
        if (std::find(b.owners.begin(), b.owners.end(), game.player) != b.owners.end()) {
            summary.battles.push_back(b);
        }
    }
    const auto assaults = attackSystems(game);
    for (const auto& a : assaults) {
        if (a.attacker == game.player || a.defender == game.player) {
            summary.assaults.push_back(a);
        }
    }
    repairShips(game);
    summary.intel = intelTurn(game);
    summary.happenings = randomEvents(game);
    for (int e = 0; e < kEmpires; ++e) {
        auto& empire = game.empires[static_cast<std::size_t>(e)];
        const auto reached = researchTurn(empire, game.economy.techs);
        if (e == game.player) {
            for (const Tech* t : reached) {
                summary.researched.push_back(t->name);
            }
        }
    }
    explore(game);
    summary.diplomacy = diplomacyTurn(game, battles, assaults);
    summary.fallen = removeFallenEmpires(game);
    if (game.ending == Ending::None) {
        game.ending = checkEnding(game);
        summary.ending = game.ending;
    }
    game.turn += 1;
}

TurnSummary endTurn(Game& game) {
    TurnSummary summary = beginEndTurn(game);
    finishEndTurn(game, summary, {});
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
            // A colony ship and a scout to start with: the cheapest of each.
            for (const ShipRole role : {ShipRole::Colony, ShipRole::Scout}) {
                int best = -1;
                for (std::size_t i = 0; i < economy.ships.size(); ++i) {
                    const ShipClass& s = economy.ships[i];
                    if (s.race == e && s.role == role && (best < 0 || s.cost < economy.ships[static_cast<std::size_t>(best)].cost)) {
                        best = static_cast<int>(i);
                    }
                }
                if (best >= 0) {
                    g.addFleet(e, g.galaxy.systems[static_cast<std::size_t>(home)].sector, best);
                }
            }
        }
    }
    startDiplomacy(g, data.races);
    explore(g);
    return g;
}

} // namespace nagus
