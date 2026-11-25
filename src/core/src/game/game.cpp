// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/game.hpp"

#include "nagus/game/ai.hpp"
#include "nagus/game/combat.hpp"
#include "nagus/game/diplomacy.hpp"
#include "nagus/game/events.hpp"
#include "nagus/game/intel.hpp"
#include "nagus/game/research.hpp"
#include "nagus/game/trade.hpp"
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

bool Game::isHuman(int empire) const {
    return empire == player || (empire >= 0 && empire < kEmpires && humans[static_cast<std::size_t>(empire)]);
}

bool Game::multiplayer() const {
    for (int e = 0; e < kEmpires; ++e) {
        if (e != player && humans[static_cast<std::size_t>(e)]) {
            return true;
        }
    }
    return false;
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

Station* Game::stationAt(Sector s) {
    for (auto& st : stations) {
        if (st.at == s) {
            return &st;
        }
    }
    return nullptr;
}

const Station* Game::stationAt(Sector s) const { return const_cast<Game*>(this)->stationAt(s); }

int distanceFromBases(const Game& game, Sector s, int owner) {
    int best = distanceFromColonies(s, owner, game.colonies, game.galaxy);
    for (const auto& st : game.stations) {
        if (st.owner == owner) {
            best = std::min(best, distance(st.at, s));
        }
    }
    return best;
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
    if (distanceFromBases(game, s, empire) <= 2) {
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
    for (const auto& st : game.stations) {
        mark(st.owner, st.at, 2);
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
        distanceFromBases(game, to, f->owner) > fleetRange(*f, game.economy, empire)) {
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

bool canRaid(const Game& game, const Fleet& f) {
    const int at = game.galaxy.systemAt(f.at);
    const Colony* c = at >= 0 ? game.colonyAt(at) : nullptr;
    if (c == nullptr || c->owner < 0 || c->owner >= kEmpires || !atWar(game, f.owner, c->owner)) {
        return false;
    }
    return std::any_of(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
        return s.shipClass >= 0 && static_cast<std::size_t>(s.shipClass) < game.economy.ships.size() &&
               isArmed(game.economy.ships[static_cast<std::size_t>(s.shipClass)]);
    });
}

bool orderRaid(Game& game, int id) {
    Fleet* f = game.fleet(id);
    if (f == nullptr || !canRaid(game, *f)) {
        return false;
    }
    f->destination.reset();
    f->order = Fleet::Order::Raid;
    f->cloaked = false;
    return true;
}

bool orderScrap(Game& game, int id) {
    Fleet* f = game.fleet(id);
    if (f == nullptr) {
        return false;
    }
    f->destination.reset();
    f->order = Fleet::Order::Scrap;
    return true;
}

bool orderAvoid(Game& game, int id, bool avoid) {
    Fleet* f = game.fleet(id);
    if (f == nullptr) {
        return false;
    }
    f->avoid = avoid;
    return true;
}

namespace {

const ShipClass* shipClassOf(const Game& game, const Ship& s) {
    return s.shipClass >= 0 && static_cast<std::size_t>(s.shipClass) < game.economy.ships.size()
               ? &game.economy.ships[static_cast<std::size_t>(s.shipClass)]
               : nullptr;
}

bool hasWarship(const Game& game, const Fleet& f) {
    return std::any_of(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
        const ShipClass* c = shipClassOf(game, s);
        return c != nullptr && isArmed(*c);
    });
}

} // namespace

bool fleetSeen(const Game& game, int empire, const Fleet& f) {
    return f.owner == empire || (!f.cloaked && visibleTo(game, empire, f.at));
}

bool canCloak(const Game& game, const Fleet& f) {
    return !f.ships.empty() && std::all_of(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
        const ShipClass* c = shipClassOf(game, s);
        return c != nullptr && c->cloak;
    });
}

bool orderCloak(Game& game, int id, bool cloak) {
    Fleet* f = game.fleet(id);
    if (f == nullptr || (cloak && !canCloak(game, *f))) {
        return false;
    }
    f->cloaked = cloak;
    if (cloak && (f->order == Fleet::Order::Attack || f->order == Fleet::Order::Raid || f->order == Fleet::Order::Defend)) {
        f->order = Fleet::Order::None;
    }
    return true;
}

bool canDefend(const Game& game, const Fleet& f) { return hasWarship(game, f); }

bool orderDefend(Game& game, int id) {
    Fleet* f = game.fleet(id);
    if (f == nullptr || !canDefend(game, *f)) {
        return false;
    }
    f->destination.reset();
    f->order = Fleet::Order::Defend;
    f->cloaked = false;
    f->avoid = false;
    return true;
}

int interceptChance(const Game& game, const Fleet& f) {
    if (f.owner < 0 || f.owner >= kEmpires) {
        return 0;
    }
    // Our choice: faster fleets catch more.
    return std::clamp(40 + 15 * fleetSpeed(f, game.economy, game.empires[static_cast<std::size_t>(f.owner)]), 0, 95);
}

int wormholeExit(const Game& game, int system) {
    // Our choice: the wormholes are paired in the order of the systems; an
    // odd one out has no endpoint.
    int count = 0;
    int mine = -1;
    std::vector<int> wormholes;
    for (std::size_t i = 0; i < game.galaxy.systems.size(); ++i) {
        const int kind = game.galaxy.systems[i].kind;
        if (kind >= 0 && static_cast<std::size_t>(kind) < game.kindCodes.size() &&
            game.kindCodes[static_cast<std::size_t>(kind)] == "worm") {
            if (static_cast<int>(i) == system) {
                mine = count;
            }
            wormholes.push_back(static_cast<int>(i));
            ++count;
        }
    }
    if (mine < 0) {
        return -1;
    }
    const int other = mine ^ 1;
    return other < count ? wormholes[static_cast<std::size_t>(other)] : -1;
}

bool canEnterWormhole(const Game& game, const Fleet& f) {
    const int at = game.galaxy.systemAt(f.at);
    if (at < 0) {
        return false;
    }
    const int kind = game.galaxy.systems[static_cast<std::size_t>(at)].kind;
    return kind >= 0 && static_cast<std::size_t>(kind) < game.kindCodes.size() &&
           game.kindCodes[static_cast<std::size_t>(kind)] == "worm";
}

bool orderWormhole(Game& game, int id) {
    Fleet* f = game.fleet(id);
    if (f == nullptr || !canEnterWormhole(game, *f)) {
        return false;
    }
    f->destination.reset();
    f->order = Fleet::Order::Wormhole;
    return true;
}

int stationClass(const Game& game, int empire, ShipRole role) {
    if (empire < 0 || empire >= kEmpires) {
        return -1;
    }
    const EmpireState& e = game.empires[static_cast<std::size_t>(empire)];
    int best = -1;
    for (std::size_t i = 0; i < game.economy.ships.size(); ++i) {
        const ShipClass& c = game.economy.ships[i];
        const bool known = std::equal(c.tech.begin(), c.tech.end(), e.tech.begin(), [](int need, int have) { return need <= have; });
        // The best: the one that costs most.
        if (c.race == empire && c.role == role && known &&
            (best < 0 || c.cost > game.economy.ships[static_cast<std::size_t>(best)].cost)) {
            best = static_cast<int>(i);
        }
    }
    return best;
}

namespace {

int transports(const Game& game, const Fleet& f) {
    return static_cast<int>(std::count_if(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
        const ShipClass* c = shipClassOf(game, s);
        return c != nullptr && c->role == ShipRole::Transport;
    }));
}

ShipRole stationRole(const Game& game, const Station& st) {
    return st.shipClass >= 0 && static_cast<std::size_t>(st.shipClass) < game.economy.ships.size()
               ? game.economy.ships[static_cast<std::size_t>(st.shipClass)].role
               : ShipRole::Outpost;
}

} // namespace

bool canBuildOutpost(const Game& game, const Fleet& f) {
    return transports(game, f) > 0 && game.stationAt(f.at) == nullptr && stationClass(game, f.owner, ShipRole::Outpost) >= 0;
}

bool canBuildStarbase(const Game& game, const Fleet& f) {
    const Station* st = game.stationAt(f.at);
    return transports(game, f) > 0 && st != nullptr && st->owner == f.owner && stationRole(game, *st) == ShipRole::Outpost &&
           stationClass(game, f.owner, ShipRole::Starbase) >= 0;
}

bool canUpgradeStation(const Game& game, const Fleet& f) {
    const Station* st = game.stationAt(f.at);
    if (transports(game, f) == 0 || st == nullptr || st->owner != f.owner) {
        return false;
    }
    const int best = stationClass(game, f.owner, stationRole(game, *st));
    return best >= 0 && best != st->shipClass &&
           game.economy.ships[static_cast<std::size_t>(best)].cost > game.economy.ships[static_cast<std::size_t>(st->shipClass)].cost;
}

bool orderBuildStation(Game& game, int id, Fleet::Order order) {
    Fleet* f = game.fleet(id);
    if (f == nullptr) {
        return false;
    }
    const bool ok = order == Fleet::Order::BuildOutpost    ? canBuildOutpost(game, *f)
                    : order == Fleet::Order::BuildStarbase ? canBuildStarbase(game, *f)
                    : order == Fleet::Order::Upgrade       ? canUpgradeStation(game, *f)
                                                           : false;
    if (!ok) {
        return false;
    }
    f->destination.reset();
    if (f->order != order) {
        f->progress = 0;
    }
    f->order = order;
    return true;
}

int stationTarget(const Game& game, const Fleet& f) {
    const Station* st = game.stationAt(f.at);
    switch (f.order) {
    case Fleet::Order::BuildOutpost:
        return stationClass(game, f.owner, ShipRole::Outpost);
    case Fleet::Order::BuildStarbase:
        return stationClass(game, f.owner, ShipRole::Starbase);
    case Fleet::Order::Upgrade:
        return st != nullptr ? stationClass(game, f.owner, stationRole(game, *st)) : -1;
    default:
        return -1;
    }
}

int stationWork(const Game& game, const Fleet& f) {
    const int target = stationTarget(game, f);
    if (target < 0) {
        return 0;
    }
    const int cost = game.economy.ships[static_cast<std::size_t>(target)].cost;
    // An upgrade costs what the better class costs more.
    const Station* st = game.stationAt(f.at);
    if (f.order == Fleet::Order::Upgrade && st != nullptr && st->shipClass >= 0) {
        return std::max(1, cost - game.economy.ships[static_cast<std::size_t>(st->shipClass)].cost);
    }
    return std::max(1, cost);
}

int terraformCost(const Planet& p) { return std::max(20, p.room); }

int terraformTarget(const Game& game, const Fleet& f) {
    const int at = game.galaxy.systemAt(f.at);
    const bool colonyShip = std::any_of(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
        const ShipClass* c = shipClassOf(game, s);
        return c != nullptr && c->role == ShipRole::Colony;
    });
    if (!colonyShip || at < 0) {
        return -1;
    }
    const StarSystem& s = game.galaxy.systems[static_cast<std::size_t>(at)];
    const Colony* c = game.colonyAt(at);
    if (!s.star || (c != nullptr && c->owner != f.owner) || (c == nullptr && s.race >= 0)) {
        return -1;
    }
    int best = -1;
    int bestRoom = 0;
    for (std::size_t i = 0; i < s.planets.size(); ++i) {
        const Planet& p = s.planets[i];
        const int room = p.type == kGasGiant ? 0 : planetRoom(p, f.owner, game.economy);
        if (!p.terraformed && room > bestRoom) {
            best = static_cast<int>(i);
            bestRoom = room;
        }
    }
    return best;
}

bool orderTerraform(Game& game, int id) {
    Fleet* f = game.fleet(id);
    if (f == nullptr || terraformTarget(game, *f) < 0) {
        return false;
    }
    f->destination.reset();
    if (f->order != Fleet::Order::Terraform) {
        f->progress = 0;
    }
    f->order = Fleet::Order::Terraform;
    return true;
}

bool canTrain(const Game& game, const Fleet& f) {
    const Colony* c = game.colonyAt(game.galaxy.systemAt(f.at));
    if (c == nullptr || c->owner != f.owner) {
        return false;
    }
    const bool school = std::any_of(c->structures.begin(), c->structures.end(), [&](const Structure& st) {
        return st.on && st.building >= 0 && static_cast<std::size_t>(st.building) < game.economy.buildings.size() &&
               game.economy.buildings[static_cast<std::size_t>(st.building)].bonus == bonus::kShipExperience;
    });
    return school && std::any_of(f.ships.begin(), f.ships.end(), [](const Ship& s) { return s.experience < kTrainingLimit; });
}

bool orderTrain(Game& game, int id) {
    Fleet* f = game.fleet(id);
    if (f == nullptr || !canTrain(game, *f)) {
        return false;
    }
    f->destination.reset();
    f->order = Fleet::Order::Train;
    return true;
}

int splitShip(Game& game, int id, int ship) {
    Fleet* f = game.fleet(id);
    if (f == nullptr || f->ships.size() < 2 || ship < 0 || static_cast<std::size_t>(ship) >= f->ships.size()) {
        return -1;
    }
    Fleet out;
    out.id = game.nextFleetId++;
    out.owner = f->owner;
    out.at = f->at;
    out.avoid = f->avoid;
    out.cloaked = f->cloaked;
    out.ships.push_back(f->ships[static_cast<std::size_t>(ship)]);
    f->ships.erase(f->ships.begin() + ship);
    // f may move when the list grows.
    game.fleets.push_back(std::move(out));
    return game.fleets.back().id;
}

bool mergeFleets(Game& game, int into, int from) {
    Fleet* a = game.fleet(into);
    Fleet* b = game.fleet(from);
    if (a == nullptr || b == nullptr || into == from || a->owner != b->owner || !(a->at == b->at)) {
        return false;
    }
    a->ships.insert(a->ships.end(), b->ships.begin(), b->ships.end());
    // Ships without a cloaking device show the fleet.
    a->cloaked = a->cloaked && canCloak(game, *a);
    game.fleets.erase(std::find_if(game.fleets.begin(), game.fleets.end(), [&](const Fleet& f) { return f.id == from; }));
    return true;
}

namespace {

void scrapFleets(Game& game) {
    std::erase_if(game.fleets, [](const Fleet& f) { return f.order == Fleet::Order::Scrap; });
}

void raid(Game& game, TurnSummary& summary) {
    for (auto& f : game.fleets) {
        if (f.order != Fleet::Order::Raid) {
            continue;
        }
        f.order = Fleet::Order::None;
        if (!canRaid(game, f)) {
            continue;
        }
        const int at = game.galaxy.systemAt(f.at);
        const Colony& c = *game.colonyAt(at);
        int warships = 0;
        for (const auto& s : f.ships) {
            warships += isArmed(game.economy.ships[static_cast<std::size_t>(s.shipClass)]) ? 1 : 0;
        }
        auto& victim = game.empires[static_cast<std::size_t>(c.owner)];
        const int taken = std::clamp(warships * kRaidCredits, 0, std::max(0, victim.credits));
        victim.credits -= taken;
        game.empires[static_cast<std::size_t>(f.owner)].credits += taken;
        const std::string& where = game.galaxy.systems[static_cast<std::size_t>(at)].name;
        if (f.owner == game.player) {
            summary.events.push_back("Raid on " + where + ": " + std::to_string(taken) + " credits taken");
        } else if (c.owner == game.player) {
            summary.events.push_back(where + " was raided: " + std::to_string(taken) + " credits lost");
        }
    }
}

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

void travelWormholes(Game& game, TurnSummary& summary) {
    for (auto& f : game.fleets) {
        if (f.order != Fleet::Order::Wormhole) {
            continue;
        }
        f.order = Fleet::Order::None;
        if (!canEnterWormhole(game, f)) {
            continue;
        }
        const int at = game.galaxy.systemAt(f.at);
        const int exit = wormholeExit(game, at);
        const std::string& where = game.galaxy.systems[static_cast<std::size_t>(at)].name;
        if (exit < 0) {
            if (f.owner == game.player) {
                summary.events.push_back(std::to_string(f.ships.size()) + (f.ships.size() == 1 ? " ship" : " ships") +
                                         " lost in the " + where + " wormhole");
            }
            f.ships.clear();
            continue;
        }
        f.at = game.galaxy.systems[static_cast<std::size_t>(exit)].sector;
        if (f.owner == game.player) {
            summary.events.push_back("A fleet went through the " + where + " wormhole to " +
                                     game.galaxy.systems[static_cast<std::size_t>(exit)].name);
        }
    }
    std::erase_if(game.fleets, [](const Fleet& f) { return f.ships.empty(); });
}

// Defending fleets catch enemies that stopped next to them.
void intercept(Game& game, TurnSummary& summary) {
    for (std::size_t i = 0; i < game.fleets.size(); ++i) {
        Fleet& f = game.fleets[i];
        if (f.order != Fleet::Order::Defend || !canDefend(game, f)) {
            continue;
        }
        const Fleet* target = nullptr;
        for (const auto& o : game.fleets) {
            if (distance(o.at, f.at) == 1 && hostile(game, f.owner, o.owner) && fleetSeen(game, f.owner, o) &&
                (target == nullptr || o.id < target->id)) {
                target = &o;
            }
        }
        if (target == nullptr) {
            continue;
        }
        const std::uint32_t roll = combatRandom(static_cast<std::uint32_t>(game.turn), static_cast<std::uint32_t>(f.id),
                                                static_cast<std::uint32_t>(target->id), 0x1e7u) % 100;
        if (static_cast<int>(roll) >= interceptChance(game, f)) {
            continue;
        }
        if (f.owner == game.player || target->owner == game.player) {
            const std::string who = f.owner == game.player ? "Our fleet intercepted an enemy fleet"
                                                           : "An enemy fleet intercepted our fleet";
            const int system = game.galaxy.systemAt(target->at);
            summary.events.push_back(system >= 0 ? who + " at " + game.galaxy.systems[static_cast<std::size_t>(system)].name : who);
        }
        f.at = target->at;
    }
}

void buildStations(Game& game, TurnSummary& summary) {
    for (auto& f : game.fleets) {
        if (f.order != Fleet::Order::BuildOutpost && f.order != Fleet::Order::BuildStarbase && f.order != Fleet::Order::Upgrade) {
            continue;
        }
        const bool ok = f.order == Fleet::Order::BuildOutpost    ? canBuildOutpost(game, f)
                        : f.order == Fleet::Order::BuildStarbase ? canBuildStarbase(game, f)
                                                                 : canUpgradeStation(game, f);
        if (!ok) {
            f.order = Fleet::Order::None;
            f.progress = 0;
            continue;
        }
        f.progress += transports(game, f) * kStationWorkPerTransport;
        if (f.progress < stationWork(game, f)) {
            continue;
        }
        // Done: the oldest transport is used up to arm and man it.
        const int target = stationTarget(game, f);
        const ShipClass& c = game.economy.ships[static_cast<std::size_t>(target)];
        if (Station* st = game.stationAt(f.at)) {
            st->shipClass = target;
            st->hull = c.hull;
        } else {
            game.stations.push_back({f.owner, f.at, target, c.hull});
        }
        f.ships.erase(std::find_if(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
            const ShipClass* sc = shipClassOf(game, s);
            return sc != nullptr && sc->role == ShipRole::Transport;
        }));
        f.order = Fleet::Order::None;
        f.progress = 0;
        if (f.owner == game.player) {
            const int system = game.galaxy.systemAt(f.at);
            const std::string where = system >= 0 ? " at " + game.galaxy.systems[static_cast<std::size_t>(system)].name
                                                  : " at " + std::to_string(f.at.x + 1) + "," + std::to_string(f.at.y + 1);
            summary.events.push_back(c.name + " finished" + where);
        }
    }
    std::erase_if(game.fleets, [](const Fleet& f) { return f.ships.empty(); });
}

void terraform(Game& game, TurnSummary& summary) {
    for (auto& f : game.fleets) {
        if (f.order != Fleet::Order::Terraform) {
            continue;
        }
        const int target = terraformTarget(game, f);
        if (target < 0) {
            f.order = Fleet::Order::None;
            f.progress = 0;
            continue;
        }
        const int ships = static_cast<int>(std::count_if(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
            const ShipClass* c = shipClassOf(game, s);
            return c != nullptr && c->role == ShipRole::Colony;
        }));
        f.progress += ships * kTerraformPerShip;
        const int at = game.galaxy.systemAt(f.at);
        StarSystem& s = game.galaxy.systems[static_cast<std::size_t>(at)];
        Planet& p = s.planets[static_cast<std::size_t>(target)];
        if (f.progress < terraformCost(p)) {
            continue;
        }
        p.terraformed = true;
        f.order = Fleet::Order::None;
        f.progress = 0;
        if (Colony* c = game.colonyAt(at)) {
            c->maxPopulation = std::max(c->maxPopulation, systemRoom(s, c->owner, game.economy));
        }
        if (f.owner == game.player) {
            summary.events.push_back(p.name.empty() ? "Planet terraformed in " + s.name : p.name + " terraformed");
        }
    }
}

void trainCrews(Game& game) {
    for (auto& f : game.fleets) {
        if (f.order != Fleet::Order::Train) {
            continue;
        }
        if (!canTrain(game, f)) {
            f.order = Fleet::Order::None;
            continue;
        }
        for (auto& s : f.ships) {
            s.experience = std::max(s.experience, std::min(kTrainingLimit, s.experience + kTrainingExperience));
        }
        if (!canTrain(game, f)) {
            f.order = Fleet::Order::None;
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

void refreshBonuses(Game& game) {
    for (auto& c : game.colonies) {
        c.foodBonus = 0;
        c.energyBonus = 0;
        c.dilithium = false;
        if (c.system < 0 || static_cast<std::size_t>(c.system) >= game.galaxy.systems.size()) {
            continue;
        }
        const StarSystem& s = game.galaxy.systems[static_cast<std::size_t>(c.system)];
        c.dilithium = s.dilithium;
        for (const Planet& p : s.planets) {
            if (!p.terraformed) {
                continue;
            }
            if (p.bonus == kFoodBonus) {
                c.foodBonus += kPlanetBonusPercent;
            } else if (p.bonus == kEnergyBonus) {
                c.energyBonus += kPlanetBonusPercent;
            }
        }
    }
}

TurnSummary beginEndTurn(Game& game) {
    TurnSummary summary;
    refreshBonuses(game);
    // The computer players give their orders.
    for (int e = 0; e < kEmpires; ++e) {
        if (!game.isHuman(e)) {
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
    scrapFleets(game);
    travelWormholes(game, summary);
    colonize(game, summary);
    moveFleets(game);
    intercept(game, summary);
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
    raid(game, summary);
    buildStations(game, summary);
    terraform(game, summary);
    tradeTurn(game, summary.events);
    repairShips(game);
    trainCrews(game);
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
    refreshBonuses(game);
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
    refreshBonuses(g);
    return g;
}

} // namespace nagus
