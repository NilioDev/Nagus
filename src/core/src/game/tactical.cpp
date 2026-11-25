// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/tactical.hpp"

#include "nagus/game/fleet.hpp"

#include <algorithm>
#include <map>
#include <optional>

namespace nagus {

namespace {

// The lexicon's names and help texts, in Maneuver's order.
constexpr std::array<std::size_t, kManeuvers> kNames = {1218, 1220, 1222, 1224, 1230, 1232, 1228, 1237, 1239, 1234};
constexpr std::array<std::size_t, kManeuvers> kHelp = {1219, 1221, 1223, 1225, 1231, 1233, 1229, 1238, 1240, 1235};

// Our numbers, in percent.
constexpr int kBeaten = 150;       // a maneuver against one it works well against
constexpr int kBeatenBy = 50;      // and against one it works poorly against
constexpr int kCloseRange = 125;   // close range: more damage, given and taken
constexpr int kEvadeGiven = 25;
constexpr int kEvadeTaken = 50;
constexpr int kRetreatTaken = 75;
constexpr int kRamShare = 50;      // of the rammer's full hull, to both ships

// The three kinds of attack: straight at the enemy, around it, past it.
int family(Maneuver m) {
    switch (m) {
    case Maneuver::Assault:
    case Maneuver::Charge: return 0;
    case Maneuver::Circle:
    case Maneuver::Harry: return 1;
    case Maneuver::Flyby:
    case Maneuver::Strafe: return 2;
    default: return -1;
    }
}

bool worksWellAgainst(Maneuver a, Maneuver b) {
    const int fa = family(a);
    const int fb = family(b);
    return (fa == 0 && fb == 2) || (fa == 1 && fb == 0) || (fa == 2 && fb == 1);
}

bool closeRange(Maneuver m) { return m == Maneuver::Charge || m == Maneuver::Harry || m == Maneuver::Strafe; }

bool shoots(Maneuver m) { return m != Maneuver::Retreat && m != Maneuver::Hail; }

std::uint32_t u32(int v) { return static_cast<std::uint32_t>(v); }

} // namespace

std::size_t maneuverName(Maneuver m) { return kNames[static_cast<std::size_t>(m)]; }
std::size_t maneuverHelp(Maneuver m) { return kHelp[static_cast<std::size_t>(m)]; }

TacticalBattle::TacticalBattle(const Game& game, Sector at) : at_(at), player_(game.player) {
    units_ = combatUnits(game, at);
    gone_.assign(units_.size(), false);
    lastDamage_.assign(units_.size(), 0);
    battle_ = battleSides(units_, at);
    sides_ = battle_.owners;
    // The player's side first.
    if (auto it = std::find(sides_.begin(), sides_.end(), player_); it != sides_.end()) {
        std::rotate(sides_.begin(), it, it + 1);
    }
    orders_.assign(sides_.size(), Maneuver::Assault);
    ended_ = !battleStarts(game, units_);
}

Maneuver TacticalBattle::lastManeuver(int owner) const {
    const auto it = std::find(sides_.begin(), sides_.end(), owner);
    return it == sides_.end() ? Maneuver::Assault : orders_[static_cast<std::size_t>(it - sides_.begin())];
}

bool TacticalBattle::over() const { return ended_ || avoided_; }

int TacticalBattle::strength(int owner) const {
    int total = 0;
    for (std::size_t i = 0; i < units_.size(); ++i) {
        const CombatUnit& u = units_[i];
        if (u.owner == owner && u.hull > 0 && !gone_[i]) {
            total += u.firepower + (u.ship >= 0 ? u.hull + u.shields : u.shields);
        }
    }
    return total;
}

bool TacticalBattle::hasEnemy(const Game& game, int owner) const {
    for (std::size_t i = 0; i < units_.size(); ++i) {
        if (units_[i].hull > 0 && !gone_[i] && hostile(game, owner, units_[i].owner)) {
            return true;
        }
    }
    return false;
}

Maneuver TacticalBattle::choose(const Game& game, int owner) const {
    const bool hasShips = std::any_of(units_.begin(), units_.end(), [&](const CombatUnit& u) {
        return u.owner == owner && u.ship >= 0 && u.hull > 0;
    });
    if (!hasShips) {
        return Maneuver::Assault; // a colony's defences only
    }
    int enemy = 0;
    int against = -1; // the strongest enemy side
    for (const int side : sides_) {
        if (hostile(game, owner, side)) {
            const int s = strength(side);
            enemy += s;
            if (against < 0 || s > strength(against)) {
                against = side;
            }
        }
    }
    const int own = strength(owner);
    if (own * 3 < enemy) {
        return Maneuver::Retreat;
    }
    if (round_ == 0 && own < enemy) {
        return Maneuver::Hail;
    }
    const std::uint32_t h = combatRandom(u32(game.turn), u32(round_), u32(owner) + 77u, u32(at_.x * 1000 + at_.y));
    // Often the answer to what the enemy did last round; otherwise any attack.
    if (round_ > 0 && h % 2 == 0) {
        const Maneuver last = lastManeuver(against);
        for (const Maneuver m : {Maneuver::Assault, Maneuver::Circle, Maneuver::Flyby}) {
            if (worksWellAgainst(m, last)) {
                const bool close = (h >> 4) % 2 == 0;
                return close ? static_cast<Maneuver>(static_cast<int>(m) + 1) : m;
            }
        }
    }
    return static_cast<Maneuver>((h >> 8) % 6u);
}

void TacticalBattle::fight(const Game& game, Maneuver order) {
    if (over()) {
        return;
    }
    lastShots_.clear();
    std::fill(lastDamage_.begin(), lastDamage_.end(), 0);
    for (std::size_t s = 0; s < sides_.size(); ++s) {
        orders_[s] = sides_[s] == player_ ? order : choose(game, sides_[s]);
    }
    if (std::all_of(orders_.begin(), orders_.end(), [](Maneuver m) { return m == Maneuver::Hail; })) {
        avoided_ = true;
        round_ += 1;
        return;
    }
    auto maneuverOf = [&](const CombatUnit& u) {
        // A colony's defences can only fire straight at the enemy.
        return u.ship < 0 ? (shoots(lastManeuver(u.owner)) ? Maneuver::Assault : Maneuver::Hail) : lastManeuver(u.owner);
    };
    std::vector<int> damage(units_.size());
    std::vector<int> collision(units_.size());
    for (std::size_t a = 0; a < units_.size(); ++a) {
        const CombatUnit& u = units_[a];
        const Maneuver ma = maneuverOf(u);
        if (u.hull <= 0 || gone_[a] || !shoots(ma)) {
            continue;
        }
        std::vector<std::size_t> targets;
        std::vector<std::size_t> ships;
        for (std::size_t b = 0; b < units_.size(); ++b) {
            if (units_[b].hull > 0 && !gone_[b] && hostile(game, u.owner, units_[b].owner)) {
                targets.push_back(b);
                if (units_[b].ship >= 0) {
                    ships.push_back(b);
                }
            }
        }
        if (targets.empty()) {
            continue;
        }
        const std::uint32_t h = combatRandom(u32(game.turn), u32(round_) + 1000u, static_cast<std::uint32_t>(a), u32(at_.x * 1000 + at_.y));
        if (ma == Maneuver::Ram && u.ship >= 0 && !ships.empty()) {
            const std::size_t t = ships[h % ships.size()];
            const int hit = std::max(1, u.maxHull * kRamShare / 100);
            collision[t] += hit;
            collision[a] += hit;
            lastShots_.push_back({a, t});
            continue;
        }
        if (u.firepower <= 0) {
            continue;
        }
        const std::size_t t = targets[h % targets.size()];
        const Maneuver mt = maneuverOf(units_[t]);
        // Between half and four fifths of the weapons hit.
        int hit = u.firepower * static_cast<int>(50 + (h >> 8) % 31) / 100;
        if (worksWellAgainst(ma, mt)) {
            hit = hit * kBeaten / 100;
        } else if (worksWellAgainst(mt, ma)) {
            hit = hit * kBeatenBy / 100;
        }
        if (closeRange(ma)) {
            hit = hit * kCloseRange / 100;
        }
        if (closeRange(mt)) {
            hit = hit * kCloseRange / 100;
        }
        if (ma == Maneuver::Evade) {
            hit = hit * kEvadeGiven / 100;
        }
        if (mt == Maneuver::Evade) {
            hit = hit * kEvadeTaken / 100;
        } else if (mt == Maneuver::Retreat) {
            hit = hit * kRetreatTaken / 100;
        }
        damage[t] += hit;
        lastShots_.push_back({a, t});
    }
    for (std::size_t b = 0; b < units_.size(); ++b) {
        CombatUnit& v = units_[b];
        const int through = damage[b] - std::min(damage[b], v.shields);
        v.shields = std::max(0, v.shields - damage[b]);
        lastDamage_[b] = damage[b] + collision[b];
        // Colonies' defences are not destroyed, only kept busy.
        if (v.ship >= 0) {
            v.hull = std::max(0, v.hull - through - collision[b]);
        }
        if (v.hull > 0 && lastManeuver(v.owner) == Maneuver::Retreat && v.ship >= 0) {
            gone_[b] = true;
        }
    }
    round_ += 1;
    // Over when nobody left can shoot at an enemy, or the time is up.
    bool fighting = false;
    for (std::size_t a = 0; a < units_.size() && !fighting; ++a) {
        fighting = units_[a].hull > 0 && !gone_[a] && units_[a].firepower > 0 && hasEnemy(game, units_[a].owner);
    }
    ended_ = !fighting || round_ >= kTacticalRounds;
}

void TacticalBattle::fightToEnd(const Game& game) {
    while (!over()) {
        fight(game, choose(game, player_));
    }
}

Battle TacticalBattle::finish(Game& game) const {
    Battle battle = battle_;
    std::map<int, bool> retreated;
    for (std::size_t i = 0; i < units_.size(); ++i) {
        const CombatUnit& u = units_[i];
        if (u.ship < 0 || u.fleet < 0 || static_cast<std::size_t>(u.fleet) >= game.fleets.size()) {
            continue;
        }
        Fleet& f = game.fleets[static_cast<std::size_t>(u.fleet)];
        if (static_cast<std::size_t>(u.ship) < f.ships.size()) {
            Ship& ship = f.ships[static_cast<std::size_t>(u.ship)];
            ship.hull = u.hull;
            ship.experience = std::min(kMaxExperience, ship.experience + kBattleExperience);
        }
        if (u.hull <= 0) {
            const auto side = std::find(battle.owners.begin(), battle.owners.end(), u.owner) - battle.owners.begin();
            battle.lost[static_cast<std::size_t>(side)] += 1;
        } else if (gone_[i]) {
            retreated[u.owner] = true;
        }
    }
    // Those who retreated go to the neighbouring sector nearest their colonies.
    for (const auto& [owner, unused] : retreated) {
        std::optional<Sector> refuge;
        int best = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const Sector s{at_.x + dx, at_.y + dy};
                if ((dx == 0 && dy == 0) || !game.galaxy.inside(s)) {
                    continue;
                }
                const int d = distanceFromColonies(s, owner, game.colonies, game.galaxy);
                if (!refuge || d < best) {
                    refuge = s;
                    best = d;
                }
            }
        }
        for (Fleet& f : game.fleets) {
            if (refuge && f.owner == owner && f.at == at_) {
                f.at = *refuge;
                f.destination.reset();
                f.order = Fleet::Order::None;
            }
        }
    }
    return battle;
}

std::vector<Sector> playerBattles(const Game& game) {
    std::vector<Sector> out;
    for (const Fleet& f : game.fleets) {
        if (std::find(out.begin(), out.end(), f.at) != out.end()) {
            continue;
        }
        const auto units = combatUnits(game, f.at);
        const bool mine = std::any_of(units.begin(), units.end(), [&](const CombatUnit& u) { return u.owner == game.player; });
        if (mine && battleStarts(game, units)) {
            out.push_back(f.at);
        }
    }
    return out;
}

} // namespace nagus
