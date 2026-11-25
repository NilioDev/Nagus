// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/research.hpp"

#include <algorithm>
#include <numeric>

namespace nagus {

const Tech* findTech(const std::vector<Tech>& techs, TechField field, int level) {
    for (const auto& t : techs) {
        if (t.field == field && t.level == level) {
            return &t;
        }
    }
    return nullptr;
}

int nextLevelCost(const EmpireState& empire, TechField field, const std::vector<Tech>& techs) {
    const int level = empire.tech[static_cast<std::size_t>(field)];
    if (level >= kMaxTechLevel) {
        return 0;
    }
    const Tech* t = findTech(techs, field, level + 1);
    return t != nullptr ? static_cast<int>(std::max<std::uint32_t>(1, t->cost)) : 0;
}

void setShare(EmpireState& empire, int field, int percent) {
    if (field < 0 || field >= kResearchFields) {
        return;
    }
    auto& share = empire.share;
    int fixed = 0; // what the locked fields hold
    std::vector<int> free;
    for (int f = 0; f < kResearchFields; ++f) {
        if (f == field) {
            continue;
        }
        if (empire.locked[static_cast<std::size_t>(f)]) {
            fixed += share[static_cast<std::size_t>(f)];
        } else {
            free.push_back(f);
        }
    }
    percent = std::clamp(percent, 0, 100 - fixed);
    if (free.empty()) {
        percent = 100 - fixed;
    }
    share[static_cast<std::size_t>(field)] = percent;
    // The rest goes to the free fields in proportion to what they had.
    int rest = 100 - fixed - percent;
    int before = 0;
    for (const int f : free) {
        before += share[static_cast<std::size_t>(f)];
    }
    int given = 0;
    for (std::size_t i = 0; i < free.size(); ++i) {
        auto& s = share[static_cast<std::size_t>(free[i])];
        const int n = static_cast<int>(free.size());
        s = before > 0 ? s * rest / before : rest / n;
        given += s;
    }
    // Rounding: the remainder goes to the free fields one point at a time.
    for (std::size_t i = 0; given < rest && !free.empty(); i = (i + 1) % free.size()) {
        share[static_cast<std::size_t>(free[i])] += 1;
        given += 1;
    }
}

void updateSociology(EmpireState& empire) {
    int sum = 0;
    for (int f = 0; f < kResearchFields; ++f) {
        sum += empire.tech[static_cast<std::size_t>(f)];
    }
    empire.tech[static_cast<std::size_t>(TechField::Sociology)] = sum / kResearchFields;
}

std::vector<const Tech*> researchTurn(EmpireState& empire, const std::vector<Tech>& techs) {
    std::vector<const Tech*> reached;
    const int points = std::max(0, empire.researchPoints);
    empire.researchPoints = 0;
    for (int f = 0; f < kResearchFields; ++f) {
        const auto i = static_cast<std::size_t>(f);
        const auto field = static_cast<TechField>(f);
        if (empire.tech[i] >= kMaxTechLevel) {
            continue;
        }
        empire.progress[i] += points * kFieldBoost * std::clamp(empire.share[i], 0, 100) / 100;
        for (int cost = nextLevelCost(empire, field, techs); cost > 0 && empire.progress[i] >= cost;
             cost = nextLevelCost(empire, field, techs)) {
            empire.progress[i] -= cost;
            empire.tech[i] += 1;
            if (const Tech* t = findTech(techs, field, empire.tech[i])) {
                reached.push_back(t);
            }
        }
        if (empire.tech[i] >= kMaxTechLevel) {
            empire.progress[i] = 0;
        }
    }
    updateSociology(empire);
    return reached;
}

} // namespace nagus
