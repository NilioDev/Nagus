// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/intel.hpp"

#include "nagus/game/diplomacy.hpp"
#include "nagus/game/research.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr int kCaughtRegard = 10;   // lost by the target's regard when an operation is caught
constexpr int kSecurityWeight = 2;  // a point of security weighs this many of operations
constexpr int kStealShare = 10;     // percent of credits taken
constexpr int kMaxSteal = 500;
constexpr int kMaxResearchSteal = 200;
constexpr int kUnrestShare = 10;    // percent of a colony's people

std::uint32_t mix(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    std::uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu ^ (c + 0x165667b1u) * 0xc2b2ae35u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return h;
}

int planned(const EmpireState& e) {
    int sum = 0;
    for (const auto& p : e.intel) {
        sum += p.espionage + p.sabotage;
    }
    return sum;
}

std::vector<int> coloniesOf(const Game& g, int owner) {
    std::vector<int> out;
    for (int i = 0; i < static_cast<int>(g.colonies.size()); ++i) {
        if (g.colonies[static_cast<std::size_t>(i)].owner == owner) {
            out.push_back(i);
        }
    }
    return out;
}

void reveal(Game& g, int empire, Sector centre, int radius) {
    for (int y = centre.y - radius; y <= centre.y + radius; ++y) {
        for (int x = centre.x - radius; x <= centre.x + radius; ++x) {
            if (g.galaxy.inside({x, y})) {
                const auto i = static_cast<std::size_t>(y * g.galaxy.width + x);
                if (i < g.explored.size()) {
                    g.explored[i] |= static_cast<std::uint8_t>(1u << empire);
                }
            }
        }
    }
}

// What a successful operation does. Returns its value for the report.
int carryOut(Game& g, IntelReport& r, std::uint32_t h) {
    EmpireState& me = g.empires[static_cast<std::size_t>(r.attacker)];
    EmpireState& them = g.empires[static_cast<std::size_t>(r.target)];
    const auto colonies = coloniesOf(g, r.target);
    if (!r.sabotage) {
        switch (r.focus) {
        case IntelFocus::General:
            for (const int c : colonies) {
                reveal(g, r.attacker, g.galaxy.systems[static_cast<std::size_t>(g.colonies[static_cast<std::size_t>(c)].system)].sector, 1);
            }
            return static_cast<int>(colonies.size());
        case IntelFocus::Science: {
            // The field where they are furthest ahead.
            int best = static_cast<int>(h % kResearchFields);
            for (int f = 0; f < kResearchFields; ++f) {
                const auto i = static_cast<std::size_t>(f);
                if (them.tech[i] - me.tech[i] > them.tech[static_cast<std::size_t>(best)] - me.tech[static_cast<std::size_t>(best)]) {
                    best = f;
                }
            }
            const int cost = nextLevelCost(me, static_cast<TechField>(best), g.economy.techs);
            const int gain = std::min(kMaxResearchSteal, std::max(cost / 2, 10));
            me.progress[static_cast<std::size_t>(best)] += gain;
            return gain;
        }
        case IntelFocus::Economy: {
            const int taken = std::clamp(them.credits * kStealShare / 100, 0, kMaxSteal);
            them.credits -= taken;
            me.credits += taken;
            return taken;
        }
        case IntelFocus::Military: {
            int seen = 0;
            for (const auto& f : g.fleets) {
                if (f.owner == r.target) {
                    reveal(g, r.attacker, f.at, 1);
                    ++seen;
                }
            }
            return seen;
        }
        }
        return 0;
    }
    switch (r.focus) {
    case IntelFocus::General: {
        if (colonies.empty()) {
            return 0;
        }
        Colony& c = g.colonies[static_cast<std::size_t>(colonies[h % colonies.size()])];
        const int killed = std::min(c.population - 1, std::max(1, c.population * kUnrestShare / 100));
        c.population -= std::max(0, killed);
        r.system = c.system;
        return std::max(0, killed);
    }
    case IntelFocus::Science: {
        int best = 0;
        for (int f = 1; f < kResearchFields; ++f) {
            if (them.progress[static_cast<std::size_t>(f)] > them.progress[static_cast<std::size_t>(best)]) {
                best = f;
            }
        }
        const int lost = them.progress[static_cast<std::size_t>(best)];
        them.progress[static_cast<std::size_t>(best)] = 0;
        return lost;
    }
    case IntelFocus::Economy: {
        std::vector<int> building;
        for (const int c : colonies) {
            if (g.colonies[static_cast<std::size_t>(c)].progress > 0) {
                building.push_back(c);
            }
        }
        if (building.empty()) {
            return 0;
        }
        Colony& c = g.colonies[static_cast<std::size_t>(building[h % building.size()])];
        const int lost = c.progress;
        c.progress = 0;
        r.system = c.system;
        return lost;
    }
    case IntelFocus::Military: {
        std::vector<Ship*> ships;
        for (auto& f : g.fleets) {
            if (f.owner == r.target) {
                for (auto& s : f.ships) {
                    ships.push_back(&s);
                }
            }
        }
        if (ships.empty()) {
            return 0;
        }
        Ship& s = *ships[h % ships.size()];
        const int lost = s.hull / 2;
        s.hull -= lost;
        return lost;
    }
    }
    return 0;
}

} // namespace

void setSecurity(EmpireState& e, int percent) { e.security = std::clamp(percent, 0, 100); }

void setIntelShare(EmpireState& e, int target, bool sabotage, int percent) {
    if (target < 0 || target >= kEmpires) {
        return;
    }
    IntelPlan& p = e.intel[static_cast<std::size_t>(target)];
    int& value = sabotage ? p.sabotage : p.espionage;
    // The shares of all operations add up to at most 100.
    const int others = planned(e) - value;
    value = std::clamp(percent, 0, std::max(0, 100 - others));
}

void setIntelFocus(EmpireState& e, int target, IntelFocus focus) {
    if (target >= 0 && target < kEmpires) {
        e.intel[static_cast<std::size_t>(target)].focus = focus;
    }
}

int securityPoints(const EmpireState& e) {
    const int points = std::max(0, e.intelPoints);
    const int sum = planned(e);
    if (sum == 0) {
        return points;
    }
    return points * e.security / 100;
}

int operationPoints(const EmpireState& e, int target, bool sabotage) {
    const int sum = planned(e);
    if (sum == 0 || target < 0 || target >= kEmpires) {
        return 0;
    }
    const int ops = std::max(0, e.intelPoints) - securityPoints(e);
    const IntelPlan& p = e.intel[static_cast<std::size_t>(target)];
    return ops * (sabotage ? p.sabotage : p.espionage) / sum;
}

std::vector<IntelReport> intelTurn(Game& g) {
    std::vector<IntelReport> heard;
    std::array<int, kEmpires> security{};
    for (int e = 0; e < kEmpires; ++e) {
        auto& st = g.empires[static_cast<std::size_t>(e)];
        security[static_cast<std::size_t>(e)] = securityPoints(st);
        for (int t = 0; t < kEmpires; ++t) {
            if (t == e) {
                continue;
            }
            st.espionagePool[static_cast<std::size_t>(t)] += operationPoints(st, t, false);
            st.sabotagePool[static_cast<std::size_t>(t)] += operationPoints(st, t, true);
        }
        st.lastIntel = std::max(0, st.intelPoints);
        st.lastSecurity = security[static_cast<std::size_t>(e)];
        st.intelPoints = 0;
    }
    for (int e = 0; e < kEmpires; ++e) {
        for (int t = 0; t < kEmpires; ++t) {
            if (t == e || !met(g, e, t)) {
                continue;
            }
            for (const bool sabotage : {false, true}) {
                auto& st = g.empires[static_cast<std::size_t>(e)];
                int& pool = (sabotage ? st.sabotagePool : st.espionagePool)[static_cast<std::size_t>(t)];
                if (pool < kOperationCost) {
                    continue;
                }
                const int attack = pool;
                pool -= kOperationCost;
                const int defence = security[static_cast<std::size_t>(t)] * kSecurityWeight;
                const int chance = std::clamp(100 * attack / std::max(1, attack + defence), 10, 90);
                const std::uint32_t h = mix(static_cast<std::uint32_t>(g.turn), static_cast<std::uint32_t>(e * 8 + t),
                                            sabotage ? 1u : 0u);
                IntelReport r;
                r.turn = g.turn;
                r.attacker = e;
                r.target = t;
                r.sabotage = sabotage;
                r.focus = st.intel[static_cast<std::size_t>(t)].focus;
                r.success = static_cast<int>(h % 100) < chance;
                if (r.success) {
                    r.value = carryOut(g, r, h >> 8);
                } else {
                    changeRegard(g, t, e, -kCaughtRegard);
                }
                g.intelReports.push_back(r);
                // The player hears of their own operations, of those caught
                // at home, and of sabotage felt at home.
                if (e == g.player || (t == g.player && (!r.success || sabotage))) {
                    heard.push_back(r);
                }
            }
        }
    }
    if (g.intelReports.size() > kMaxReports) {
        g.intelReports.erase(g.intelReports.begin(),
                             g.intelReports.begin() + static_cast<std::ptrdiff_t>(g.intelReports.size() - kMaxReports));
    }
    return heard;
}

void planIntel(Game& g, int e) {
    if (e < 0 || e >= kEmpires) {
        return;
    }
    EmpireState& st = g.empires[static_cast<std::size_t>(e)];
    st.security = 40;
    for (int t = 0; t < kEmpires; ++t) {
        IntelPlan& p = st.intel[static_cast<std::size_t>(t)];
        p = IntelPlan{};
        if (t == e || !met(g, e, t)) {
            continue;
        }
        if (atWar(g, e, t)) {
            p.espionage = 10;
            p.sabotage = 20;
            p.focus = g.turn % 2 == 0 ? IntelFocus::Military : IntelFocus::Economy;
        } else {
            p.espionage = 10;
            p.focus = IntelFocus::Science;
        }
    }
    // Keep the shares within 100.
    int sum = 0;
    for (const auto& p : st.intel) {
        sum += p.espionage + p.sabotage;
    }
    if (sum > 100) {
        for (auto& p : st.intel) {
            p.espionage = p.espionage * 100 / sum;
            p.sabotage = p.sabotage * 100 / sum;
        }
    }
}

} // namespace nagus
