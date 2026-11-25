// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/combat.hpp"

#include "nagus/game/diplomacy.hpp"

#include <algorithm>
#include <map>

namespace nagus {

namespace {

// Our choices: a battle lasts this many rounds, a round does a fifth of the
// weapons' damage, and shields count three times their rating.
constexpr int kRounds = 3;
constexpr int kDamageDivisor = 5;
constexpr int kShieldFactor = 3;
constexpr int kRepairElsewhere = 10; // percent of the hull a turn
// Troops: each million people defends with ten.
constexpr int kDefendersPerMillion = 10;

const ShipClass* classOf(const Game& g, const Ship& s) {
    return s.shipClass >= 0 && static_cast<std::size_t>(s.shipClass) < g.economy.ships.size()
               ? &g.economy.ships[static_cast<std::size_t>(s.shipClass)]
               : nullptr;
}

const EmpireState* empireOf(const Game& g, int owner) {
    return owner >= 0 && owner < kEmpires ? &g.empires[static_cast<std::size_t>(owner)] : nullptr;
}

std::uint32_t mix(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    std::uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu ^ (c + 0x165667b1u) * 0xc2b2ae35u ^
                      (d + 0x27d4eb2fu) * 0x61c88647u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return h;
}

// What a colony's structures add to a battle.
struct ColonyDefence {
    int firepower = 0;
    int shields = 0;
};

ColonyDefence colonyDefence(const Game& g, const Colony& c) {
    ColonyDefence d;
    const EmpireState* e = empireOf(g, c.owner);
    const int energyTech = e != nullptr ? e->tech[static_cast<std::size_t>(TechField::Energy)] : 1;
    const int weaponsTech = e != nullptr ? e->tech[static_cast<std::size_t>(TechField::Weapons)] : 1;
    for (const auto& st : c.structures) {
        if (!st.on || st.building < 0 || static_cast<std::size_t>(st.building) >= g.economy.buildings.size()) {
            continue;
        }
        const Building& b = g.economy.buildings[static_cast<std::size_t>(st.building)];
        if (b.bonus == bonus::kDefense) {
            d.firepower += b.amount * (10 + weaponsTech) / 10;
        } else if (b.bonus == bonus::kShieldPerEnergyTech) {
            d.shields += b.amount * energyTech;
        }
    }
    return d;
}

int percentBonus(const Game& g, const Colony& c, int which) {
    int total = 0;
    for (const auto& st : c.structures) {
        if (st.on && st.building >= 0 && static_cast<std::size_t>(st.building) < g.economy.buildings.size()) {
            const Building& b = g.economy.buildings[static_cast<std::size_t>(st.building)];
            total += b.bonus == which ? b.amount : 0;
        }
    }
    return total;
}

} // namespace

bool hostile(const Game& game, int a, int b) { return a != b && atWar(game, a, b); }

int shipFirepower(const ShipClass& s, const EmpireState& empire) {
    const int raw = s.beamCount * s.beamDamage + s.torpedoCount * s.torpedoDamage;
    return raw * (10 + empire.tech[static_cast<std::size_t>(TechField::Weapons)]) / 10 / kDamageDivisor;
}

int shipShields(const ShipClass& s, const EmpireState& empire) {
    return s.shields * kShieldFactor * (10 + empire.tech[static_cast<std::size_t>(TechField::Energy)]) / 10;
}

bool isArmed(const ShipClass& s) { return s.beamCount * s.beamDamage + s.torpedoCount * s.torpedoDamage > 0; }

std::vector<CombatUnit> combatUnits(const Game& game, Sector at) {
    std::vector<CombatUnit> units;
    for (int fi = 0; fi < static_cast<int>(game.fleets.size()); ++fi) {
        const Fleet& f = game.fleets[static_cast<std::size_t>(fi)];
        if (!(f.at == at)) {
            continue;
        }
        const EmpireState* e = empireOf(game, f.owner);
        for (int si = 0; si < static_cast<int>(f.ships.size()); ++si) {
            const Ship& ship = f.ships[static_cast<std::size_t>(si)];
            const ShipClass* c = classOf(game, ship);
            if (c == nullptr || e == nullptr || ship.hull <= 0) {
                continue;
            }
            units.push_back({f.owner, fi, si, ship.shipClass, shipFirepower(*c, *e), shipShields(*c, *e), ship.hull, c->hull});
        }
    }
    const int system = game.galaxy.systemAt(at);
    if (const Colony* col = game.colonyAt(system); col != nullptr) {
        const ColonyDefence d = colonyDefence(game, *col);
        if (d.firepower > 0) {
            units.push_back({col->owner, -1, -1, -1, d.firepower, d.shields, 1, 1});
        }
    }
    return units;
}

bool battleStarts(const Game& game, const std::vector<CombatUnit>& units) {
    // A battle needs someone who can shoot at an enemy.
    return std::any_of(units.begin(), units.end(), [&](const CombatUnit& u) {
        return u.firepower > 0 && u.hull > 0 && std::any_of(units.begin(), units.end(), [&](const CombatUnit& v) {
                   return v.hull > 0 && hostile(game, u.owner, v.owner);
               });
    });
}

Battle battleSides(const std::vector<CombatUnit>& units, Sector at) {
    Battle battle;
    battle.at = at;
    for (const auto& u : units) {
        auto it = std::find(battle.owners.begin(), battle.owners.end(), u.owner);
        if (it == battle.owners.end()) {
            battle.owners.push_back(u.owner);
            battle.ships.push_back(0);
            battle.lost.push_back(0);
            it = battle.owners.end() - 1;
        }
        battle.ships[static_cast<std::size_t>(it - battle.owners.begin())] += u.ship >= 0 ? 1 : 0;
    }
    return battle;
}

std::uint32_t combatRandom(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) { return mix(a, b, c, d); }

std::vector<Battle> fightBattles(Game& game, const std::vector<Sector>& skip) {
    std::vector<Battle> battles;
    std::map<std::pair<int, int>, bool> sectors;
    for (const Fleet& f : game.fleets) {
        sectors[{f.at.y, f.at.x}] = true;
    }
    for (const auto& [key, unused] : sectors) {
        const Sector at{key.second, key.first};
        if (std::find(skip.begin(), skip.end(), at) != skip.end()) {
            continue;
        }
        std::vector<CombatUnit> units = combatUnits(game, at);
        if (!battleStarts(game, units)) {
            continue;
        }
        Battle battle = battleSides(units, at);
        for (int round = 0; round < kRounds; ++round) {
            std::vector<int> damage(units.size());
            for (std::size_t a = 0; a < units.size(); ++a) {
                const CombatUnit& u = units[a];
                if (u.hull <= 0 || u.firepower <= 0) {
                    continue;
                }
                std::vector<std::size_t> targets;
                for (std::size_t b = 0; b < units.size(); ++b) {
                    if (units[b].hull > 0 && hostile(game, u.owner, units[b].owner)) {
                        targets.push_back(b);
                    }
                }
                if (targets.empty()) {
                    continue;
                }
                const std::uint32_t h = mix(static_cast<std::uint32_t>(game.turn), static_cast<std::uint32_t>(round),
                                            static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(at.x * 1000 + at.y));
                // Between half and four fifths of the weapons hit.
                damage[targets[h % targets.size()]] += u.firepower * static_cast<int>(50 + (h >> 8) % 31) / 100;
            }
            for (std::size_t b = 0; b < units.size(); ++b) {
                CombatUnit& v = units[b];
                const int through = damage[b] - std::min(damage[b], v.shields);
                v.shields = std::max(0, v.shields - damage[b]);
                // Colonies' defences are not destroyed, only kept busy.
                if (v.ship >= 0) {
                    v.hull -= through;
                }
            }
        }
        for (const auto& u : units) {
            if (u.ship < 0) {
                continue;
            }
            game.fleets[static_cast<std::size_t>(u.fleet)].ships[static_cast<std::size_t>(u.ship)].hull = std::max(0, u.hull);
            if (u.hull <= 0) {
                const auto side = std::find(battle.owners.begin(), battle.owners.end(), u.owner) - battle.owners.begin();
                battle.lost[static_cast<std::size_t>(side)] += 1;
            }
        }
        battles.push_back(std::move(battle));
    }
    removeWrecks(game);
    return battles;
}

void removeWrecks(Game& game) {
    for (auto& f : game.fleets) {
        std::erase_if(f.ships, [](const Ship& s) { return s.hull <= 0; });
    }
    std::erase_if(game.fleets, [](const Fleet& f) { return f.ships.empty(); });
}

std::vector<Assault> attackSystems(Game& game) {
    std::vector<Assault> assaults;
    // Each empire's attacking fleets at each system act together.
    std::map<std::pair<int, int>, std::vector<std::size_t>> attacks; // (system, attacker) -> fleets
    for (std::size_t i = 0; i < game.fleets.size(); ++i) {
        Fleet& f = game.fleets[i];
        if (f.order != Fleet::Order::Attack) {
            continue;
        }
        const int system = game.galaxy.systemAt(f.at);
        const Colony* col = game.colonyAt(system);
        if (col == nullptr || !hostile(game, f.owner, col->owner)) {
            f.order = Fleet::Order::None;
            continue;
        }
        attacks[{system, f.owner}].push_back(i);
    }
    for (const auto& [key, fleets] : attacks) {
        const auto [system, attacker] = key;
        Colony* col = game.colonyAt(system);
        if (col == nullptr || !hostile(game, attacker, col->owner)) {
            continue; // taken by another empire this turn
        }
        const Sector at = game.galaxy.systems[static_cast<std::size_t>(system)].sector;
        // Armed defenders in the sector must be beaten first.
        const bool defended = std::any_of(game.fleets.begin(), game.fleets.end(), [&](const Fleet& o) {
            return o.at == at && o.owner == col->owner && std::any_of(o.ships.begin(), o.ships.end(), [&](const Ship& s) {
                       const ShipClass* c = classOf(game, s);
                       return c != nullptr && isArmed(*c);
                   });
        });
        if (defended) {
            continue;
        }
        Assault a;
        a.system = system;
        a.attacker = attacker;
        a.defender = col->owner;
        const EmpireState& e = game.empires[static_cast<std::size_t>(attacker)];
        // Warships bombard: a million people for each hundred points of
        // firepower that the planetary shields do not stop.
        int bombard = 0;
        int troops = 0;
        for (const std::size_t fi : fleets) {
            for (const auto& s : game.fleets[fi].ships) {
                const ShipClass* c = classOf(game, s);
                if (c == nullptr) {
                    continue;
                }
                bombard += shipFirepower(*c, e);
                if (c->role == ShipRole::Transport) {
                    troops += c->capacity;
                }
            }
        }
        bombard = std::max(0, bombard - colonyDefence(game, *col).shields);
        const int killed = std::min(col->population, bombard / 100);
        col->population -= killed;
        a.killed += killed;
        if (troops > 0) {
            // The transports land: they are used up whatever happens.
            for (const std::size_t fi : fleets) {
                std::erase_if(game.fleets[fi].ships, [&](const Ship& s) {
                    const ShipClass* c = classOf(game, s);
                    return c != nullptr && c->role == ShipRole::Transport;
                });
            }
            troops = troops * (100 + percentBonus(game, *col, bonus::kGroundCombat)) / 100;
            const int defenders =
                col->population * kDefendersPerMillion * (100 + percentBonus(game, *col, bonus::kGroundDefense)) / 100;
            if (troops > defenders) {
                // The fight costs the colony a part of its people.
                const int lost = col->population * defenders / std::max(1, troops) / 2;
                col->population -= lost;
                a.killed += lost;
                col->owner = attacker;
                col->home = false;
                col->queue.clear();
                col->progress = 0;
                a.captured = true;
                for (const std::size_t fi : fleets) {
                    game.fleets[fi].order = Fleet::Order::None;
                }
            } else {
                const int lost = std::min(col->population, troops / kDefendersPerMillion / 2);
                col->population -= lost;
                a.killed += lost;
            }
        }
        assaults.push_back(a);
    }
    // A system with nobody left is free again.
    std::erase_if(game.colonies, [](const Colony& c) { return c.population <= 0; });
    std::erase_if(game.fleets, [](const Fleet& f) { return f.ships.empty(); });
    return assaults;
}

bool canAttack(const Game& game, const Fleet& f) {
    const Colony* col = game.colonyAt(game.galaxy.systemAt(f.at));
    if (col == nullptr || !hostile(game, f.owner, col->owner)) {
        return false;
    }
    return std::any_of(f.ships.begin(), f.ships.end(), [&](const Ship& s) {
        const ShipClass* c = classOf(game, s);
        return c != nullptr && (isArmed(*c) || (c->role == ShipRole::Transport && c->capacity > 0));
    });
}

bool orderAttack(Game& game, int id) {
    Fleet* f = game.fleet(id);
    if (f == nullptr || !canAttack(game, *f)) {
        return false;
    }
    f->destination.reset();
    f->order = Fleet::Order::Attack;
    return true;
}

void repairShips(Game& game) {
    for (auto& f : game.fleets) {
        const Colony* col = game.colonyAt(game.galaxy.systemAt(f.at));
        const bool home = col != nullptr && col->owner == f.owner;
        for (auto& s : f.ships) {
            const ShipClass* c = classOf(game, s);
            if (c == nullptr) {
                continue;
            }
            s.hull = home ? c->hull : std::min(c->hull, s.hull + std::max(1, c->hull * kRepairElsewhere / 100));
        }
    }
}

} // namespace nagus
