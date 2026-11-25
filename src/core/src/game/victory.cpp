// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/victory.hpp"

#include "nagus/game/diplomacy.hpp"

#include <algorithm>

namespace nagus {

bool outOfGame(const Game& g, int empire) {
    return std::none_of(g.colonies.begin(), g.colonies.end(), [&](const Colony& c) { return c.owner == empire; });
}

int colonyShare(const Game& g, int empire) {
    // Every colony, and the home of every minor race that has not joined
    // anyone.
    long total = static_cast<long>(g.colonies.size());
    for (int race = kEmpires; race < static_cast<int>(g.races.size()); ++race) {
        const int home = minorHome(g, race);
        if (home >= 0 && g.colonyAt(home) == nullptr) {
            ++total;
        }
    }
    if (total == 0) {
        return 0;
    }
    const auto mine = std::count_if(g.colonies.begin(), g.colonies.end(), [&](const Colony& c) { return c.owner == empire; });
    return static_cast<int>(mine * 100 / total);
}

std::vector<int> removeFallenEmpires(Game& g) {
    std::vector<int> fallen;
    for (int e = 0; e < kEmpires; ++e) {
        auto& out = g.out[static_cast<std::size_t>(e)];
        if (!out && outOfGame(g, e)) {
            out = true;
            fallen.push_back(e);
            std::erase_if(g.fleets, [&](const Fleet& f) { return f.owner == e; });
        }
    }
    return fallen;
}

Ending checkEnding(const Game& g) {
    const int me = g.player;
    if (g.out[static_cast<std::size_t>(std::clamp(me, 0, kEmpires - 1))]) {
        return Ending::Loss;
    }
    // Another empire that has won ends the game too.
    for (int e = 0; e < kEmpires; ++e) {
        if (e != me && !g.out[static_cast<std::size_t>(e)] && colonyShare(g, e) >= kDominationShare) {
            return Ending::Loss;
        }
    }
    bool othersOut = true;
    bool othersAllied = true;
    for (int e = 0; e < kEmpires; ++e) {
        if (e == me || g.out[static_cast<std::size_t>(e)]) {
            continue;
        }
        othersOut = false;
        othersAllied = othersAllied && treaty(g, me, e) == Treaty::Alliance;
    }
    const int kind = g.settings.victory; // domination, team play, vendetta
    if (othersOut) {
        return Ending::Annihilation;
    }
    if (kind != 2 && colonyShare(g, me) >= kDominationShare) {
        return Ending::Domination;
    }
    if (kind == 1 && othersAllied) {
        return Ending::Alliance;
    }
    return Ending::None;
}

int score(const Game& g) {
    const int me = std::clamp(g.player, 0, kEmpires - 1);
    const EmpireState& e = g.empires[static_cast<std::size_t>(me)];
    long long points = 0;
    for (const auto& c : g.colonies) {
        if (c.owner == me) {
            points += 100 + c.population;
        }
    }
    for (const int t : e.tech) {
        points += 25 * t;
    }
    points += std::max(0, e.credits) / 20;
    switch (g.ending) {
    case Ending::Annihilation: points += 3000; break;
    case Ending::Domination: points += 2000; break;
    case Ending::Alliance: points += 1500; break;
    default: break;
    }
    // Harder games count more: simple ×0.6 … impossible ×1.4.
    points = points * (6 + 2 * std::clamp(g.settings.difficulty, 0, 4)) / 10;
    return static_cast<int>(std::min<long long>(points, 99999999));
}

std::string endingName(Ending ending) {
    switch (ending) {
    case Ending::Alliance: return "alli";
    case Ending::Annihilation: return "anni";
    case Ending::Domination: return "domi";
    case Ending::Loss: return "loss";
    default: return {};
    }
}

} // namespace nagus
