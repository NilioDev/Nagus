// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/ai.hpp"

#include "nagus/game/combat.hpp"
#include "nagus/game/intel.hpp"

#include <algorithm>
#include <limits>

namespace nagus {

namespace {

// Credits kept back before buying, and colony ships at once (our choice).
constexpr int kReserve = 150;
constexpr int kColonyShips = 2;
// Warships wanted: one for each this many turns, by difficulty (simple ...
// impossible).
constexpr int kTurnsPerWarship[GameSettings::kDifficulties] = {40, 30, 20, 15, 10};
// Warships waiting at home before they go to attack, by difficulty.
constexpr int kStrikeGroup[GameSettings::kDifficulties] = {6, 5, 4, 3, 3};
constexpr int kTransports = 2;

bool isWarship(ShipRole r) {
    return r == ShipRole::Destroyer || r == ShipRole::Cruiser || r == ShipRole::StrikeCruiser ||
           r == ShipRole::Command;
}

const ShipClass* classOf(const Game& g, const Ship& s) {
    return s.shipClass >= 0 && static_cast<std::size_t>(s.shipClass) < g.economy.ships.size()
               ? &g.economy.ships[static_cast<std::size_t>(s.shipClass)]
               : nullptr;
}

bool fleetHas(const Game& g, const Fleet& f, bool (*test)(ShipRole)) {
    return std::any_of(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
        const ShipClass* c = classOf(g, s);
        return c != nullptr && test(c->role);
    });
}

bool isColonyShip(ShipRole r) { return r == ShipRole::Colony; }
bool isTransport(ShipRole r) { return r == ShipRole::Transport; }
bool isWarshipRole(ShipRole r) { return isWarship(r); }
bool isScout(ShipRole r) { return r == ShipRole::Scout; }

// A number from the game's state, for choices that should vary but be the
// same each time the same game is played.
std::uint32_t mix(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    std::uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu ^ (c + 0x165667b1u) * 0xc2b2ae35u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return h;
}

struct Census {
    int colonyShips = 0; // in fleets and in build queues
    int warships = 0;
    int transports = 0;
    int colonies = 0;
    bool attacking = false; // a fleet is attacking a system
};

Census count(const Game& g, int empire) {
    Census n;
    for (const auto& f : g.fleets) {
        if (f.owner != empire) {
            continue;
        }
        for (const auto& s : f.ships) {
            const ShipClass* c = classOf(g, s);
            n.colonyShips += c != nullptr && c->role == ShipRole::Colony ? 1 : 0;
            n.warships += c != nullptr && isWarship(c->role) ? 1 : 0;
            n.transports += c != nullptr && c->role == ShipRole::Transport ? 1 : 0;
        }
        n.attacking = n.attacking || f.order == Fleet::Order::Attack;
    }
    for (const auto& c : g.colonies) {
        if (c.owner != empire) {
            continue;
        }
        n.colonies += 1;
        for (const auto& q : c.queue) {
            if (q.kind == BuildItem::Kind::Ship && q.building >= 0 &&
                static_cast<std::size_t>(q.building) < g.economy.ships.size()) {
                const ShipRole r = g.economy.ships[static_cast<std::size_t>(q.building)].role;
                n.colonyShips += r == ShipRole::Colony ? 1 : 0;
                n.warships += isWarship(r) ? 1 : 0;
                n.transports += r == ShipRole::Transport ? 1 : 0;
            }
        }
    }
    return n;
}

// How many free systems are within reach of the empire's colonies.
int freeSystems(const Game& g, int empire, int reach) {
    int n = 0;
    for (int i = 0; i < static_cast<int>(g.galaxy.systems.size()); ++i) {
        if (freeForColony(g, i, empire) &&
            distanceFromBases(g, g.galaxy.systems[static_cast<std::size_t>(i)].sector, empire) <= reach) {
            ++n;
        }
    }
    return n;
}

// What a colony should build next, or nothing.
std::optional<BuildItem> choose(const Game& g, const Colony& c, const Census& census, int empire) {
    const EmpireState& e = g.empires[static_cast<std::size_t>(empire)];
    const StarSystem& s = g.galaxy.systems[static_cast<std::size_t>(c.system)];
    const auto options = buildOptions(c, e, s, g.economy);
    if (options.empty()) {
        return std::nullopt;
    }
    const ColonyOutput out = colonyOutput(c, g.economy);
    auto facility = [&](Output o) -> std::optional<BuildItem> {
        for (const auto& item : options) {
            if (item.kind == BuildItem::Kind::Facility && item.building >= 0) {
                const Building& b = g.economy.buildings[static_cast<std::size_t>(item.building)];
                if (b.output == o) {
                    return item;
                }
            }
        }
        return std::nullopt;
    };
    auto ship = [&](bool (*test)(ShipRole)) -> std::optional<BuildItem> {
        std::optional<BuildItem> best;
        int bestCost = -1;
        for (const auto& item : options) {
            if (item.kind != BuildItem::Kind::Ship) {
                continue;
            }
            const ShipClass& sc = g.economy.ships[static_cast<std::size_t>(item.building)];
            if (test(sc.role) && sc.cost > bestCost) {
                best = item;
                bestCost = sc.cost;
            }
        }
        return best;
    };

    // Food first: a starving colony shrinks.
    if (out.made[static_cast<std::size_t>(Output::Food)] < out.foodNeeded) {
        if (auto f = facility(Output::Food)) {
            return f;
        }
    }
    // Power for the structures that need it.
    if (out.made[static_cast<std::size_t>(Output::Energy)] < out.energyNeeded) {
        if (auto f = facility(Output::Energy)) {
            return f;
        }
    }
    // Colony ships while there are systems to settle.
    const int reach = 6 + e.tech[static_cast<std::size_t>(TechField::Propulsion)] / 3;
    if (census.colonyShips < kColonyShips && freeSystems(g, empire, reach) > census.colonyShips) {
        if (auto sh = ship(isColonyShip)) {
            return sh;
        }
    }
    // Warships, more of them as the game goes on.
    const int level = std::clamp(g.settings.difficulty, 0, GameSettings::kDifficulties - 1);
    if (census.warships < 1 + g.turn / kTurnsPerWarship[level]) {
        if (auto sh = ship(isWarship)) {
            return sh;
        }
    }
    // Troops to take a system the warships are attacking.
    if (census.attacking && census.transports < kTransports) {
        if (auto sh = ship(isTransport)) {
            return sh;
        }
    }
    // Better facilities.
    for (const auto& item : options) {
        if (item.kind == BuildItem::Kind::Upgrade) {
            return item;
        }
    }
    // The cheapest structure there is energy for.
    const int spare = out.made[static_cast<std::size_t>(Output::Energy)] - out.energyNeeded;
    std::optional<BuildItem> cheapest;
    std::uint32_t lowest = std::numeric_limits<std::uint32_t>::max();
    for (const auto& item : options) {
        if (item.kind != BuildItem::Kind::Structure) {
            continue;
        }
        const Building& b = g.economy.buildings[static_cast<std::size_t>(item.building)];
        if (static_cast<int>(b.energy) > spare) {
            continue;
        }
        if (const std::uint32_t cost = itemCost(item, c, g.economy); cost < lowest) {
            cheapest = item;
            lowest = cost;
        }
    }
    if (cheapest) {
        return cheapest;
    }
    // Then more facilities, by what the empire needs most.
    for (const Output o : {Output::Industry, Output::Research, Output::Energy, Output::Food, Output::Intelligence}) {
        if (auto f = facility(o)) {
            return f;
        }
    }
    return std::nullopt;
}

// A colony ship looks for the best free system it can reach: near, with
// room for many.
std::optional<int> colonyTarget(const Game& g, const Fleet& f, const std::vector<int>& taken) {
    const EmpireState& e = g.empires[static_cast<std::size_t>(f.owner)];
    const int range = fleetRange(f, g.economy, e);
    std::optional<int> best;
    int bestScore = std::numeric_limits<int>::min();
    for (int i = 0; i < static_cast<int>(g.galaxy.systems.size()); ++i) {
        if (!freeForColony(g, i, f.owner) || std::find(taken.begin(), taken.end(), i) != taken.end()) {
            continue;
        }
        const StarSystem& s = g.galaxy.systems[static_cast<std::size_t>(i)];
        if (distanceFromBases(g, s.sector, f.owner) > range) {
            continue;
        }
        // Room now counts double: it needs no terraforming first.
        const int score = (systemRoom(s, f.owner, g.economy) + systemRoomTerraformed(s, f.owner, g.economy)) / 10 -
                          4 * distance(f.at, s.sector);
        if (score > bestScore) {
            best = i;
            bestScore = score;
        }
    }
    return best;
}

int difficulty(const Game& g) { return std::clamp(g.settings.difficulty, 0, GameSettings::kDifficulties - 1); }

// Warships with nothing to do.
int idleWarships(const Game& g, int empire) {
    int n = 0;
    for (const auto& f : g.fleets) {
        if (f.owner == empire && !f.destination && (f.order == Fleet::Order::None || f.order == Fleet::Order::Defend) &&
            fleetHas(g, f, isWarshipRole)) {
            n += 1;
        }
    }
    return n;
}

// The enemy colony to attack: one the empire knows of, within reach, the
// nearest to the fleet.
std::optional<int> attackTarget(const Game& g, int empire, const Fleet& f) {
    const int range = fleetRange(f, g.economy, g.empires[static_cast<std::size_t>(empire)]);
    std::optional<int> best;
    int bestDistance = std::numeric_limits<int>::max();
    for (const auto& c : g.colonies) {
        const Sector at = g.galaxy.systems[static_cast<std::size_t>(c.system)].sector;
        if (!hostile(g, empire, c.owner) || !exploredBy(g, empire, at) ||
            distanceFromBases(g, at, empire) > range) {
            continue;
        }
        if (const int d = distance(f.at, at); d < bestDistance) {
            best = c.system;
            bestDistance = d;
        }
    }
    return best;
}

// Where the empire's warships are attacking, for its transports.
std::optional<Sector> attackedSector(const Game& g, int empire, const Fleet& f) {
    const int range = fleetRange(f, g.economy, g.empires[static_cast<std::size_t>(empire)]);
    for (const auto& o : g.fleets) {
        if (o.owner == empire && o.order == Fleet::Order::Attack &&
            distanceFromBases(g, o.at, empire) <= range) {
            return o.at;
        }
    }
    return std::nullopt;
}

} // namespace

bool freeForColony(const Game& game, int system, int empire) {
    if (system < 0 || static_cast<std::size_t>(system) >= game.galaxy.systems.size()) {
        return false;
    }
    const StarSystem& s = game.galaxy.systems[static_cast<std::size_t>(system)];
    return s.star && s.race < 0 && game.colonyAt(system) == nullptr && systemRoomTerraformed(s, empire, game.economy) > 0;
}

void playEmpire(Game& game, int empire) {
    if (empire < 0 || empire >= kEmpires) {
        return;
    }
    EmpireState& e = game.empires[static_cast<std::size_t>(empire)];
    planIntel(game, empire);

    // Build queues: keep one thing in each.
    for (auto& c : game.colonies) {
        if (c.owner != empire || !c.queue.empty()) {
            continue;
        }
        const Census census = count(game, empire);
        if (const auto item = choose(game, c, census, empire)) {
            c.queue.push_back(*item);
        }
    }
    // Spare credits finish what is being built, colony ships first.
    std::vector<Colony*> buyers;
    for (auto& c : game.colonies) {
        if (c.owner == empire && !c.queue.empty()) {
            buyers.push_back(&c);
        }
    }
    std::stable_sort(buyers.begin(), buyers.end(), [&](const Colony* a, const Colony* b) {
        auto colonyShip = [&](const Colony* c) {
            const BuildItem& q = c->queue.front();
            return q.kind == BuildItem::Kind::Ship && game.economy.ships[static_cast<std::size_t>(q.building)].role == ShipRole::Colony;
        };
        return colonyShip(a) && !colonyShip(b);
    });
    for (Colony* c : buyers) {
        const int cost = buyCost(*c, game.economy);
        if (cost > 0 && e.credits - cost >= kReserve) {
            e.credits -= cost;
            c->progress = static_cast<int>(itemCost(c->queue.front(), *c, game.economy));
        }
    }

    // Fleets.
    std::vector<int> taken; // systems a colony ship is going to
    for (const auto& f : game.fleets) {
        if (f.owner == empire && (f.destination || f.order == Fleet::Order::Terraform) && fleetHas(game, f, isColonyShip)) {
            const int at = game.galaxy.systemAt(f.destination.value_or(f.at));
            if (at >= 0) {
                taken.push_back(at);
            }
        }
    }
    std::vector<int> ids;
    for (const auto& f : game.fleets) {
        if (f.owner == empire && !f.destination && (f.order == Fleet::Order::None || f.order == Fleet::Order::Defend)) {
            ids.push_back(f.id);
        }
    }
    for (const int id : ids) {
        const Fleet& f = *game.fleet(id);
        if (fleetHas(game, f, isColonyShip)) {
            const int here = game.galaxy.systemAt(f.at);
            if (here >= 0 && std::find(taken.begin(), taken.end(), here) == taken.end() && orderColonize(game, id)) {
                taken.push_back(here);
                continue;
            }
            // A free system with no planet ready yet: make one ready.
            if (here >= 0 && std::find(taken.begin(), taken.end(), here) == taken.end() && freeForColony(game, here, empire) &&
                orderTerraform(game, id)) {
                taken.push_back(here);
                continue;
            }
            if (const auto target = colonyTarget(game, f, taken)) {
                taken.push_back(*target);
                orderMove(game, id, game.galaxy.systems[static_cast<std::size_t>(*target)].sector);
            }
        } else if (fleetHas(game, f, isScout)) {
            // Scouts look around, unseen where they can: a sector within
            // reach, a different one each time.
            if (!f.cloaked && canCloak(game, f)) {
                orderCloak(game, id, true);
            }
            const int range = fleetRange(f, game.economy, e);
            for (int attempt = 0; attempt < 8; ++attempt) {
                const std::uint32_t h = mix(static_cast<std::uint32_t>(game.turn), static_cast<std::uint32_t>(id),
                                            static_cast<std::uint32_t>(attempt));
                const Sector to{f.at.x + static_cast<int>(h % 9) - 4, f.at.y + static_cast<int>((h / 9) % 9) - 4};
                if (game.galaxy.inside(to) && distanceFromBases(game, to, empire) <= range &&
                    orderMove(game, id, to)) {
                    break;
                }
            }
        } else if (fleetHas(game, f, isWarshipRole) || fleetHas(game, f, isTransport)) {
            if (orderAttack(game, id)) {
                continue;
            }
            // Transports join an attack; warships go once there are enough.
            if (fleetHas(game, f, isTransport) && !fleetHas(game, f, isWarshipRole)) {
                if (const auto to = attackedSector(game, empire, f)) {
                    orderMove(game, id, *to);
                } else if (const Colony* home = game.colonyAt(game.galaxy.systemAt(f.at)); home != nullptr && home->owner == empire) {
                    // With no attack to join, they guard their colony with
                    // an outpost, and then a starbase.
                    for (const auto order : {Fleet::Order::BuildOutpost, Fleet::Order::BuildStarbase, Fleet::Order::Upgrade}) {
                        if (orderBuildStation(game, id, order)) {
                            break;
                        }
                    }
                }
                continue;
            }
            if (idleWarships(game, empire) >= kStrikeGroup[difficulty(game)]) {
                if (const auto target = attackTarget(game, empire, f)) {
                    orderMove(game, id, game.galaxy.systems[static_cast<std::size_t>(*target)].sector);
                    continue;
                }
            }
            // Waiting warships guard where they are.
            if (f.order != Fleet::Order::Defend) {
                orderDefend(game, id);
            }
        }
    }
}

} // namespace nagus
