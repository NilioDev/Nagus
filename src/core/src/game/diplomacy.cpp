// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/diplomacy.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr int kMaxRegard = 200;
constexpr int kGiftPerRegard = 25;   // credits for one point of regard
constexpr int kMaxGiftRegard = 10;
constexpr int kWarRegard = 30;       // lost by the race war is declared on
constexpr int kBattleRegard = 5;
constexpr int kAssaultRegard = 15;
constexpr int kWearyAfter = 20;      // turns of war before peace gets easier
constexpr int kPeaceRegard = 50;     // at least, after a peace treaty
constexpr int kPeaceNeeded = 30;
constexpr int kKeepTreaty = 30; // turns
constexpr int kMembershipTurns = 10; // of affiliation before a minor race joins
// Regard needed for each treaty, before the race's aggression (our choice).
constexpr int kNeeded[kTreaties] = {0, 0, 0, 70, 90, 115, 140, 0};
constexpr int kMinorNeeded[kTreaties] = {0, 0, 0, 0, 75, 100, 0, 130};
// A computer player goes to war below this regard, before its aggression.
constexpr int kWarBelow = 60;

int races(const Game& g) { return static_cast<int>(g.races.size()); }

const Relation* find(const Game& g, int empire, int race) {
    if (empire < 0 || empire >= kEmpires || race < 0 || race >= races(g)) {
        return nullptr;
    }
    const auto i = static_cast<std::size_t>(empire * races(g) + race);
    return i < g.relations.size() ? &g.relations[i] : nullptr;
}

Relation* find(Game& g, int empire, int race) { return const_cast<Relation*>(find(static_cast<const Game&>(g), empire, race)); }

// The relation as kept for an empire and a race, whichever way round.
const Relation* pair(const Game& g, int a, int b) { return a >= 0 && a < kEmpires ? find(g, a, b) : find(g, b, a); }

void setTreaty(Game& g, int a, int b, Treaty t) {
    for (Relation* r : {find(g, a, b), find(g, b, a)}) {
        if (r != nullptr) {
            r->treaty = t;
            r->since = g.turn;
        }
    }
}

void addRegard(Game& g, int race, int empire, int change) {
    if (Relation* r = find(g, empire, race)) {
        r->regard = std::clamp(r->regard + change, 0, kMaxRegard);
    }
}

int aggression(const Game& g, int race) {
    return race >= 0 && race < races(g) ? std::max(50, g.races[static_cast<std::size_t>(race)].aggression) : 100;
}

std::size_t proposalText(Treaty t, int what) {
    // what: 0 offered, 1 accepted, 2 rejected, 3 ignored
    switch (t) {
    case Treaty::NonAggression: return std::array<std::size_t, 4>{868, 864, 869, 867}[static_cast<std::size_t>(what)];
    case Treaty::Friendship: return std::array<std::size_t, 4>{385, 380, 386, 384}[static_cast<std::size_t>(what)];
    case Treaty::Affiliation: return std::array<std::size_t, 4>{11, 7, 12, 10}[static_cast<std::size_t>(what)];
    case Treaty::Alliance: return std::array<std::size_t, 4>{24, 20, 25, 23}[static_cast<std::size_t>(what)];
    case Treaty::Membership: return std::array<std::size_t, 4>{635, kTextJoined, 636, 634}[static_cast<std::size_t>(what)];
    default: return 507; // a peace treaty
    }
}

// A minor race joins: its system becomes a colony of the empire.
void join(Game& g, int empire, int race) {
    const int system = minorHome(g, race);
    if (system < 0 || g.colonyAt(system) != nullptr) {
        return;
    }
    const StarSystem& s = g.galaxy.systems[static_cast<std::size_t>(system)];
    Colony c = homeColony(system, s, empire, g.empires[static_cast<std::size_t>(empire)], g.economy);
    c.home = false;
    g.colonies.push_back(std::move(c));
}

void apply(Game& g, int from, int to, Treaty t, std::vector<DiplomacyEvent>& events) {
    const Treaty before = treaty(g, from, to);
    setTreaty(g, from, to, t);
    if (before == Treaty::War) {
        // Peace: old grudges are set aside, for a while.
        for (Relation* r : {find(g, from, to), find(g, to, from)}) {
            if (r != nullptr) {
                r->regard = std::max(r->regard, kPeaceRegard);
            }
        }
    }
    if (t == Treaty::Membership) {
        join(g, from, to);
    }
    events.push_back({before == Treaty::War && t != Treaty::Membership ? std::size_t{507} : proposalText(t, 1), to, from});
}

bool hasColonyNear(const Game& g, int owner, int other, int within) {
    for (const auto& a : g.colonies) {
        if (a.owner != owner) {
            continue;
        }
        const Sector sa = g.galaxy.systems[static_cast<std::size_t>(a.system)].sector;
        for (const auto& b : g.colonies) {
            if (b.owner == other && distance(sa, g.galaxy.systems[static_cast<std::size_t>(b.system)].sector) <= within) {
                return true;
            }
        }
    }
    return false;
}

int warships(const Game& g, int empire) {
    int n = 0;
    for (const auto& f : g.fleets) {
        if (f.owner != empire) {
            continue;
        }
        for (const auto& s : f.ships) {
            const ShipRole r = g.economy.ships[static_cast<std::size_t>(s.shipClass)].role;
            n += r == ShipRole::Destroyer || r == ShipRole::Cruiser || r == ShipRole::StrikeCruiser || r == ShipRole::Command;
        }
    }
    return n;
}

} // namespace

void startDiplomacy(Game& g, const std::vector<RaceInfo>& list) {
    g.races = list;
    g.relations.assign(static_cast<std::size_t>(kEmpires * races(g)), Relation{});
    for (int e = 0; e < kEmpires; ++e) {
        for (int r = 0; r < races(g); ++r) {
            g.relations[static_cast<std::size_t>(e * races(g) + r)].regard =
                g.races[static_cast<std::size_t>(r)].attitude[static_cast<std::size_t>(e)];
        }
    }
    g.offers.clear();
}

Treaty treaty(const Game& g, int a, int b) {
    const Relation* r = pair(g, a, b);
    return r != nullptr && a != b ? r->treaty : Treaty::None;
}

int regard(const Game& g, int race, int empire) {
    const Relation* r = find(g, empire, race);
    return r != nullptr ? r->regard : 0;
}

bool atWar(const Game& g, int a, int b) { return treaty(g, a, b) == Treaty::War; }
bool met(const Game& g, int a, int b) { return treaty(g, a, b) != Treaty::None; }
bool isMinor(const Game& g, int race) { return race >= kEmpires && race < races(g); }

int minorHome(const Game& g, int race) {
    for (int i = 0; i < static_cast<int>(g.galaxy.systems.size()); ++i) {
        if (g.galaxy.systems[static_cast<std::size_t>(i)].race == race) {
            return i;
        }
    }
    return -1;
}

// The empire a minor race has joined, or -1.
int memberOf(const Game& g, int race) {
    for (int e = 0; e < kEmpires; ++e) {
        if (treaty(g, e, race) == Treaty::Membership) {
            return e;
        }
    }
    return -1;
}

bool canPropose(const Game& g, int from, int to, Treaty t) {
    if (from < 0 || from >= kEmpires || from == to || !met(g, from, to)) {
        return false;
    }
    if (isMinor(g, to) && memberOf(g, to) >= 0) {
        return false;
    }
    const Treaty now = treaty(g, from, to);
    if (now == Treaty::Membership) {
        return false;
    }
    if (isMinor(g, to)) {
        switch (t) {
        case Treaty::Friendship: return now == Treaty::Neutral || now == Treaty::War;
        case Treaty::Affiliation: return now == Treaty::Friendship;
        case Treaty::Membership: return now == Treaty::Affiliation && minorHome(g, to) >= 0;
        default: return false;
        }
    }
    switch (t) {
    case Treaty::NonAggression:
    case Treaty::Friendship:
    case Treaty::Affiliation: return now == Treaty::War || t > now;
    case Treaty::Alliance: return now >= Treaty::Friendship && now < Treaty::Alliance;
    default: return false;
    }
}

bool wouldAccept(const Game& g, int from, int to, Treaty t) {
    if (!canPropose(g, from, to, t)) {
        return false;
    }
    const Relation* rel = pair(g, from, to);
    int r = regard(g, to, from);
    if (rel->treaty == Treaty::War) {
        r += std::clamp(g.turn - rel->since - kWearyAfter, 0, 40);
    }
    if (isMinor(g, to)) {
        if (t == Treaty::Membership && g.turn - rel->since < kMembershipTurns) {
            return false;
        }
        return r >= kMinorNeeded[static_cast<int>(t)];
    }
    // From war, a non-aggression treaty is peace, and needs less.
    const int needed = rel->treaty == Treaty::War && t == Treaty::NonAggression ? kPeaceNeeded : kNeeded[static_cast<int>(t)];
    return r >= needed * aggression(g, to) / 100;
}

Reply propose(Game& g, int from, int to, Treaty t, std::vector<DiplomacyEvent>& events) {
    if (!canPropose(g, from, to, t)) {
        return Reply::Rejected;
    }
    if (to == g.player) {
        if (std::none_of(g.offers.begin(), g.offers.end(), [&](const Offer& o) { return o.from == from; })) {
            g.offers.push_back({from, t, g.turn});
            events.push_back({proposalText(t, 0), from, to});
        }
        return Reply::Waiting;
    }
    if (wouldAccept(g, from, to, t)) {
        apply(g, from, to, t, events);
        return Reply::Accepted;
    }
    events.push_back({proposalText(t, 2), to, from});
    return Reply::Rejected;
}

bool canDeclareWar(const Game& g, int from, int to) {
    const Treaty t = treaty(g, from, to);
    return from >= 0 && from < kEmpires && from != to && t != Treaty::None && t != Treaty::War && t != Treaty::Membership;
}

void declareWar(Game& g, int from, int to, std::vector<DiplomacyEvent>& events) {
    if (!canDeclareWar(g, from, to)) {
        return;
    }
    setTreaty(g, from, to, Treaty::War);
    addRegard(g, to, from, -kWarRegard);
    std::erase_if(g.offers, [&](const Offer& o) { return o.from == from || o.from == to; });
    events.push_back({278, from, to});
}

bool giveGift(Game& g, int from, int to, int credits, std::vector<DiplomacyEvent>& events) {
    if (from < 0 || from >= kEmpires || !met(g, from, to) || credits <= 0 ||
        credits > g.empires[static_cast<std::size_t>(from)].credits) {
        return false;
    }
    g.empires[static_cast<std::size_t>(from)].credits -= credits;
    if (to >= 0 && to < kEmpires) {
        g.empires[static_cast<std::size_t>(to)].credits += credits;
    }
    addRegard(g, to, from, std::min(kMaxGiftRegard, credits / kGiftPerRegard));
    events.push_back({413, from, to});
    return true;
}

void answerOffer(Game& g, std::size_t i, bool accept, std::vector<DiplomacyEvent>& events) {
    if (i >= g.offers.size()) {
        return;
    }
    const Offer o = g.offers[i];
    g.offers.erase(g.offers.begin() + static_cast<std::ptrdiff_t>(i));
    if (accept && canPropose(g, o.from, g.player, o.treaty)) {
        apply(g, o.from, g.player, o.treaty, events);
    } else {
        addRegard(g, g.player, o.from, -5);
        events.push_back({proposalText(o.treaty, 2), g.player, o.from});
    }
}

std::vector<DiplomacyEvent> diplomacyTurn(Game& g, const std::vector<Battle>& battles, const std::vector<Assault>& assaults) {
    std::vector<DiplomacyEvent> events;
    if (g.relations.empty()) {
        return events;
    }
    // New contacts: a race's colony, home or fleet in explored space.
    for (int e = 0; e < kEmpires; ++e) {
        for (int r = 0; r < races(g); ++r) {
            if (r == e || met(g, e, r)) {
                continue;
            }
            bool seen = false;
            for (const auto& c : g.colonies) {
                seen = seen || (c.owner == r && exploredBy(g, e, g.galaxy.systems[static_cast<std::size_t>(c.system)].sector));
            }
            if (const int home = minorHome(g, r); isMinor(g, r) && home >= 0) {
                seen = seen || exploredBy(g, e, g.galaxy.systems[static_cast<std::size_t>(home)].sector);
            }
            for (const auto& f : g.fleets) {
                seen = seen || (f.owner == r && visibleTo(g, e, f.at));
            }
            if (seen) {
                setTreaty(g, e, r, Treaty::Neutral);
                if (e == g.player || r == g.player) {
                    events.push_back({kTextMet, e, r});
                }
            }
        }
    }
    // Regard: treaties bring races closer, war and crowded borders apart.
    for (int e = 0; e < kEmpires; ++e) {
        for (int r = 0; r < races(g); ++r) {
            Relation& rel = g.relations[static_cast<std::size_t>(e * races(g) + r)];
            if (r == e || rel.treaty == Treaty::None) {
                continue;
            }
            int change = 0;
            const bool even = g.turn % 2 == 0;
            if (rel.treaty == Treaty::War) {
                change = even ? -1 : 0;
            } else if (rel.treaty >= Treaty::Friendship) {
                change = even ? 1 : 0;
            } else if (g.turn % 4 == 0) {
                // Without friendship, regard goes back to where the race's
                // nature puts it.
                const int base = g.races[static_cast<std::size_t>(r)].attitude[static_cast<std::size_t>(e)] * 100 / aggression(g, r);
                change = rel.regard < base ? 1 : rel.regard > base ? -1 : 0;
            }
            // Empires do not like each other's colonies close by.
            if (r < kEmpires && g.turn % 4 == 2 && hasColonyNear(g, r, e, 3)) {
                change -= 1;
            }
            rel.regard = std::clamp(rel.regard + change, 0, kMaxRegard);
        }
    }
    for (const auto& b : battles) {
        for (const int x : b.owners) {
            for (const int y : b.owners) {
                if (x != y) {
                    addRegard(g, x, y, -kBattleRegard);
                }
            }
        }
    }
    for (const auto& a : assaults) {
        addRegard(g, a.defender, a.attacker, -kAssaultRegard);
    }
    // Offers the player left unanswered lapse.
    for (const auto& o : g.offers) {
        if (o.turn < g.turn) {
            events.push_back({proposalText(o.treaty, 3), g.player, o.from});
        }
    }
    std::erase_if(g.offers, [&](const Offer& o) { return o.turn < g.turn; });

    // The computer players.
    const int level = std::clamp(g.settings.difficulty, 0, GameSettings::kDifficulties - 1);
    constexpr int kStrength[GameSettings::kDifficulties] = {6, 5, 4, 3, 3};
    for (int e = 0; e < kEmpires; ++e) {
        if (e == g.player) {
            continue;
        }
        bool fighting = false;
        for (int m = 0; m < kEmpires; ++m) {
            fighting = fighting || (m != e && atWar(g, e, m));
        }
        for (int m = 0; m < kEmpires; ++m) {
            if (m == e || !met(g, e, m)) {
                continue;
            }
            const Relation& rel = *pair(g, e, m);
            const int mine = regard(g, e, m); // what e thinks of m
            if (rel.treaty == Treaty::War) {
                if (mine + std::clamp(g.turn - rel.since - kWearyAfter, 0, 40) >= 25) {
                    propose(g, e, m, Treaty::NonAggression, events);
                }
                continue;
            }
            // Aggressive races go to war sooner, and only one war at a time.
            // A non-aggression treaty is kept for a while, and broken only
            // by those who think even less of the other.
            const bool breakable = rel.treaty <= Treaty::Neutral ||
                                   (rel.treaty == Treaty::NonAggression && g.turn - rel.since >= kKeepTreaty && mine < kWarBelow - 10);
            if (!fighting && g.turn >= 20 && breakable && mine < kWarBelow * aggression(g, e) / 100 &&
                warships(g, e) >= kStrength[level]) {
                declareWar(g, e, m, events);
                fighting = true;
                continue;
            }
            for (const Treaty t : {Treaty::NonAggression, Treaty::Friendship, Treaty::Affiliation, Treaty::Alliance}) {
                if (canPropose(g, e, m, t) && mine >= kNeeded[static_cast<int>(t)] * aggression(g, e) / 100 + 10 &&
                    (m == g.player || wouldAccept(g, e, m, t))) {
                    propose(g, e, m, t, events);
                    break;
                }
            }
        }
        // Minor races: gifts to the one most taken with the empire, then
        // treaties up to membership.
        int best = -1;
        for (int r = kEmpires; r < races(g); ++r) {
            if (!met(g, e, r) || treaty(g, e, r) == Treaty::Membership || treaty(g, e, r) == Treaty::War) {
                continue;
            }
            for (const Treaty t : {Treaty::Friendship, Treaty::Affiliation, Treaty::Membership}) {
                if (wouldAccept(g, e, r, t)) {
                    propose(g, e, r, t, events);
                    break;
                }
            }
            if (best < 0 || regard(g, r, e) > regard(g, best, e)) {
                best = r;
            }
        }
        auto& credits = g.empires[static_cast<std::size_t>(e)].credits;
        if (best >= 0 && credits > 600 && g.turn % 5 == e) {
            giveGift(g, e, best, 200, events);
        }
    }
    // The player hears of what happened to them.
    std::erase_if(events, [&](const DiplomacyEvent& ev) {
        return ev.text != kTextMet && ev.a != g.player && ev.b != g.player && ev.text != 278;
    });
    return events;
}

} // namespace nagus
