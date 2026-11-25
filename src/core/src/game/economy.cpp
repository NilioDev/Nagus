// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/economy.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr int kPeoplePerFacility = 10;
constexpr int kFoodStoreTurns = 5;   // food kept in store: up to five turns' need
constexpr int kTaxDivisor = 5;       // each 5 million people pay a credit a turn
constexpr int kPeoplePerFood = 4;    // each 4 million people eat one food a turn
constexpr int kBuyFactor = 2;        // credits per missing point of industry
constexpr std::uint32_t kGrowthUnit = 10'000'000;

const Building* buildingAt(const EconomyData& data, int index) {
    return index >= 0 && static_cast<std::size_t>(index) < data.buildings.size() ? &data.buildings[static_cast<std::size_t>(index)]
                                                                                  : nullptr;
}

int habitability(const EconomyData& data, int type, int atmosphere, int race) {
    for (const auto& h : data.habitats) {
        if (h.type == type && h.atmosphere == atmosphere) {
            return h.values[static_cast<std::size_t>(race >= 0 && race < kEmpires ? race : 5)];
        }
    }
    return 0;
}

int indexOf(Output o) { return static_cast<int>(o); }

bool hasStructure(const Colony& c, int building) {
    return std::any_of(c.structures.begin(), c.structures.end(), [&](const Structure& s) { return s.building == building; });
}

bool queued(const Colony& c, const BuildItem& item) { return std::find(c.queue.begin(), c.queue.end(), item) != c.queue.end(); }

bool available(const Building& b, const EmpireState& empire, int race) {
    return b.buildableBy(race) && b.level <= empire.tech[static_cast<std::size_t>(b.field)];
}

} // namespace

int systemRoom(const StarSystem& s, int race, const EconomyData& data) {
    int room = 0;
    for (const auto& p : s.planets) {
        room += p.room * habitability(data, p.type, p.atmosphere, race) / 10;
    }
    return room;
}

int bestFacility(Output o, const EmpireState& empire, int race, const EconomyData& data) {
    int best = -1;
    for (std::size_t i = 0; i < data.buildings.size(); ++i) {
        const Building& b = data.buildings[i];
        if (b.kind != Building::Class::Facility || b.output != o || b.workers <= 0 || !available(b, empire, race)) {
            continue;
        }
        if (best < 0 || b.level > data.buildings[static_cast<std::size_t>(best)].level) {
            best = static_cast<int>(i);
        }
    }
    return best;
}

int facilityLimit(const Colony& c) { return std::max(1, c.maxPopulation / kPeoplePerFacility); }

ColonyOutput colonyOutput(const Colony& c, const EconomyData& data) {
    ColonyOutput out;
    std::array<int, kOutputs> percent{};
    for (int o = 0; o < kOutputs; ++o) {
        if (const Building* f = buildingAt(data, c.facility[static_cast<std::size_t>(o)])) {
            out.made[static_cast<std::size_t>(o)] = c.working[static_cast<std::size_t>(o)] * f->amount;
        }
    }
    int creditsPercent = 0;
    // Structures: those that need no energy always work; the others work
    // while switched on and while there is energy for them, in order.
    int energyLeft = out.made[static_cast<std::size_t>(indexOf(Output::Energy))];
    for (const auto& s : c.structures) {
        const Building* b = buildingAt(data, s.building);
        if (b == nullptr) {
            continue;
        }
        if (b->energy > 0) {
            if (!s.on) {
                continue;
            }
            out.energyNeeded += static_cast<int>(b->energy);
            if (static_cast<int>(b->energy) > energyLeft) {
                continue;
            }
            energyLeft -= static_cast<int>(b->energy);
        }
        auto add = [&](Output o, int v) { out.made[static_cast<std::size_t>(indexOf(o))] += v; };
        auto pct = [&](Output o, int v) { percent[static_cast<std::size_t>(indexOf(o))] += v; };
        switch (b->bonus) {
        case bonus::kFood:
        case bonus::kFoodToo: add(Output::Food, b->amount); break;
        case bonus::kFoodPercent: pct(Output::Food, b->amount); break;
        case bonus::kEnergy:
        case bonus::kEnergyToo: add(Output::Energy, b->amount); break;
        case bonus::kEnergyPercent: pct(Output::Energy, b->amount); break;
        case bonus::kCredits: out.credits += b->amount; break;
        case bonus::kCreditsPercent:
        case bonus::kTotalCreditsPercent: creditsPercent += b->amount; break;
        case bonus::kResearch:
        case bonus::kResearchPoints: add(Output::Research, b->amount); break;
        case bonus::kResearchPercent:
        case bonus::kResearchEmpirePercent: pct(Output::Research, b->amount); break;
        case bonus::kIntel:
        case bonus::kIntelPoints: add(Output::Intelligence, b->amount); break;
        case bonus::kIntelPercent: pct(Output::Intelligence, b->amount); break;
        case bonus::kIndustry: add(Output::Industry, b->amount); break;
        case bonus::kIndustryPercent: pct(Output::Industry, b->amount); break;
        case bonus::kScanRange: out.scanRange = std::max(out.scanRange, b->amount); break;
        case bonus::kShipyard:
        case bonus::kSpecialShipyard: out.shipyard = true; break;
        default: break;
        }
    }
    for (int o = 0; o < kOutputs; ++o) {
        auto& v = out.made[static_cast<std::size_t>(o)];
        v = v * (100 + percent[static_cast<std::size_t>(o)]) / 100;
    }
    out.foodNeeded = (c.population + kPeoplePerFood - 1) / kPeoplePerFood;
    out.credits += c.population / kTaxDivisor;
    out.credits = out.credits * (100 + creditsPercent) / 100;
    return out;
}

std::vector<BuildItem> buildOptions(const Colony& c, const EmpireState& empire, const StarSystem& s,
                                    const EconomyData& data) {
    std::vector<BuildItem> items;
    int total = 0;
    for (const int n : c.facilities) {
        total += n;
    }
    for (int q = 0; q < static_cast<int>(c.queue.size()); ++q) {
        total += c.queue[static_cast<std::size_t>(q)].kind == BuildItem::Kind::Facility ? 1 : 0;
    }
    for (int o = 0; o < kOutputs; ++o) {
        const int best = bestFacility(static_cast<Output>(o), empire, c.owner, data);
        if (best < 0) {
            continue;
        }
        const int current = c.facility[static_cast<std::size_t>(o)];
        const Building* now = buildingAt(data, current);
        if (now != nullptr && data.buildings[static_cast<std::size_t>(best)].level > now->level) {
            const BuildItem upgrade{BuildItem::Kind::Upgrade, best};
            if (!queued(c, upgrade)) {
                items.push_back(upgrade);
            }
        }
        if (total < facilityLimit(c)) {
            items.push_back({BuildItem::Kind::Facility, now != nullptr ? current : best});
        }
    }
    // Structures: one of each, where the planets allow it.
    for (std::size_t i = 0; i < data.buildings.size(); ++i) {
        const Building& b = data.buildings[i];
        if (b.kind == Building::Class::Facility || b.cost == 0 || !available(b, empire, c.owner)) {
            continue;
        }
        // Not yet: buildings of minor races, for special planets or for
        // conquered systems.
        if (b.minorRaces != 0xffffffffu || b.planetType > kPlanetTypes || (b.systemKind != 9 && b.systemKind > 1) ||
            (b.systemKind == 0 && !c.home)) {
            continue;
        }
        // One shipyard to a system.
        if (b.bonus == bonus::kShipyard && std::any_of(c.structures.begin(), c.structures.end(), [&](const Structure& st) {
                const Building* other = buildingAt(data, st.building);
                return other != nullptr && other->bonus == bonus::kShipyard;
            })) {
            continue;
        }
        if (b.planetType < kPlanetTypes &&
            std::none_of(s.planets.begin(), s.planets.end(), [&](const Planet& p) { return p.type == b.planetType; })) {
            continue;
        }
        const BuildItem item{BuildItem::Kind::Structure, static_cast<int>(i)};
        if (hasStructure(c, item.building) || queued(c, item)) {
            continue;
        }
        // Structures that replace an older one need it.
        if (b.upgradesFrom >= 0) {
            const bool hasOld = std::any_of(c.structures.begin(), c.structures.end(), [&](const Structure& st) {
                const Building* old = buildingAt(data, st.building);
                return old != nullptr && old->id == b.upgradesFrom;
            });
            if (!hasOld) {
                continue;
            }
        }
        items.push_back(item);
    }
    return items;
}

std::uint32_t itemCost(const BuildItem& item, const Colony& c, const EconomyData& data) {
    const Building* b = buildingAt(data, item.building);
    if (b == nullptr) {
        return 0;
    }
    if (item.kind == BuildItem::Kind::Upgrade) {
        // Replaces every facility of its kind at once.
        const int n = c.facilities[static_cast<std::size_t>(indexOf(b->output))];
        return b->cost * static_cast<std::uint32_t>(std::max(1, n));
    }
    return std::max<std::uint32_t>(1, b->cost);
}

std::string itemName(const BuildItem& item, const EconomyData& data) {
    const Building* b = buildingAt(data, item.building);
    return b != nullptr ? b->name : std::string();
}

int buyCost(const Colony& c, const EconomyData& data) {
    if (c.queue.empty()) {
        return 0;
    }
    const int left = static_cast<int>(itemCost(c.queue.front(), c, data)) - c.progress;
    return std::max(0, left) * kBuyFactor;
}

void setWorking(Colony& c, Output o, int count) {
    const auto i = static_cast<std::size_t>(indexOf(o));
    int others = 0;
    for (std::size_t k = 0; k < c.working.size(); ++k) {
        others += k == i ? 0 : c.working[k];
    }
    const int free = c.population / kPeoplePerFacility - others;
    c.working[i] = std::clamp(count, 0, std::min(c.facilities[i], std::max(0, free)));
}

Colony homeColony(int system, const StarSystem& s, int empire, const EmpireState& state, const EconomyData& data) {
    Colony c;
    c.system = system;
    c.owner = empire;
    c.home = true;
    c.maxPopulation = std::max(kPeoplePerFacility, systemRoom(s, empire, data));
    c.population = std::max(kPeoplePerFacility, c.maxPopulation * 7 / 10);
    for (int o = 0; o < kOutputs; ++o) {
        c.facility[static_cast<std::size_t>(o)] = bestFacility(static_cast<Output>(o), state, empire, data);
    }
    // A shipyard of the empire's own kind.
    for (std::size_t i = 0; i < data.buildings.size(); ++i) {
        const Building& b = data.buildings[i];
        if (b.bonus == bonus::kShipyard && b.kind == Building::Class::Structure && b.excluded != 0 && b.buildableBy(empire)) {
            c.structures.push_back({static_cast<int>(i), true});
            break;
        }
    }
    // People on enough farms to feed everyone and enough power plants for
    // the structures; the others split between industry, research and
    // intelligence. One farm and one factory more than there are workers (our choice).
    const ColonyOutput none = colonyOutput(c, data);
    auto needed = [&](Output o, int amount) {
        const Building* f = buildingAt(data, c.facility[static_cast<std::size_t>(indexOf(o))]);
        return f == nullptr || f->amount <= 0 ? 0 : (amount + f->amount - 1) / f->amount;
    };
    const int workers = c.population / kPeoplePerFacility;
    std::array<int, kOutputs> work{};
    auto at = [&](Output o) -> int& { return work[static_cast<std::size_t>(indexOf(o))]; };
    at(Output::Food) = std::max(1, needed(Output::Food, none.foodNeeded));
    at(Output::Energy) = needed(Output::Energy, none.energyNeeded);
    const int rest = std::max(0, workers - at(Output::Food) - at(Output::Energy));
    at(Output::Industry) = rest * 5 / 10;
    at(Output::Research) = rest * 3 / 10;
    at(Output::Intelligence) = rest - at(Output::Industry) - at(Output::Research);
    int spare = std::max(0, facilityLimit(c) - workers);
    for (int o = 0; o < kOutputs; ++o) {
        const auto i = static_cast<std::size_t>(o);
        if (c.facility[i] < 0) {
            continue;
        }
        const int extra = std::min(spare, o == indexOf(Output::Industry) || o == indexOf(Output::Food) ? 1 : 0);
        spare -= extra;
        c.facilities[i] = work[i] + extra;
        c.working[i] = work[i];
    }
    c.food = c.population;
    return c;
}

void assignWorkers(Colony& c, const EconomyData& data) {
    auto freeWorkers = [&] {
        int busy = 0;
        for (const int w : c.working) {
            busy += w;
        }
        return c.population / kPeoplePerFacility - busy;
    };
    auto add = [&](Output o) {
        const auto i = static_cast<std::size_t>(indexOf(o));
        if (c.working[i] < c.facilities[i]) {
            c.working[i] += 1;
            return true;
        }
        return false;
    };
    while (freeWorkers() > 0) {
        const ColonyOutput out = colonyOutput(c, data);
        const bool hungry = out.made[static_cast<std::size_t>(indexOf(Output::Food))] < out.foodNeeded;
        const bool dark = out.made[static_cast<std::size_t>(indexOf(Output::Energy))] < out.energyNeeded;
        if ((hungry && add(Output::Food)) || (dark && add(Output::Energy)) || add(Output::Industry) ||
            add(Output::Research) || add(Output::Intelligence) || add(Output::Energy) || add(Output::Food)) {
            continue;
        }
        break;
    }
}

TurnReport colonyTurn(Colony& c, const EconomyData& data) {
    TurnReport report;
    const ColonyOutput out = colonyOutput(c, data);
    report.credits = out.credits;
    report.research = out.made[static_cast<std::size_t>(indexOf(Output::Research))];
    report.intel = out.made[static_cast<std::size_t>(indexOf(Output::Intelligence))];

    // Food: the surplus goes into store, a shortage comes out of it.
    c.food += out.made[static_cast<std::size_t>(indexOf(Output::Food))] - out.foodNeeded;
    c.food = std::min(c.food, out.foodNeeded * kFoodStoreTurns);
    if (c.food < 0) {
        report.starving = true;
        c.population = std::max(1, c.population - std::max(1, -c.food / 10));
        c.food = 0;
    } else if (c.population < c.maxPopulation) {
        std::uint32_t rate = 0;
        for (const auto& g : data.growth) {
            if (c.population >= g.from && c.population < g.to) {
                rate = g.rate;
            }
        }
        const auto grown = static_cast<int>(static_cast<std::uint64_t>(c.population) * rate / kGrowthUnit);
        c.population = std::min(c.maxPopulation, c.population + std::max(1, grown));
    }
    // Fewer people may leave facilities without workers; more people go to
    // work where they are needed.
    for (int o = 0; o < kOutputs; ++o) {
        setWorking(c, static_cast<Output>(o), c.working[static_cast<std::size_t>(o)]);
    }
    assignWorkers(c, data);

    // Industry builds the queue; with nothing to build it makes credits
    // (as trade goods).
    int industry = out.made[static_cast<std::size_t>(indexOf(Output::Industry))];
    if (c.queue.empty()) {
        report.credits += industry;
        return report;
    }
    c.progress += industry;
    while (!c.queue.empty()) {
        const BuildItem item = c.queue.front();
        const int cost = static_cast<int>(itemCost(item, c, data));
        if (c.progress < cost) {
            break;
        }
        c.progress -= cost;
        c.queue.erase(c.queue.begin());
        const Building* b = buildingAt(data, item.building);
        if (b == nullptr) {
            continue;
        }
        report.finished.push_back(b->name);
        const auto o = static_cast<std::size_t>(indexOf(b->output));
        switch (item.kind) {
        case BuildItem::Kind::Facility:
            if (c.facility[o] < 0) {
                c.facility[o] = item.building;
            }
            c.facilities[o] += 1;
            break;
        case BuildItem::Kind::Upgrade: c.facility[o] = item.building; break;
        case BuildItem::Kind::Structure: {
            // A newer structure replaces the one it upgrades.
            for (auto& s : c.structures) {
                const Building* old = buildingAt(data, s.building);
                if (old != nullptr && b->upgradesFrom >= 0 && old->id == b->upgradesFrom) {
                    s.building = -1;
                }
            }
            std::erase_if(c.structures, [](const Structure& s) { return s.building < 0; });
            c.structures.push_back({item.building, true});
            break;
        }
        }
    }
    if (c.queue.empty()) {
        // Left-over industry is not kept.
        c.progress = 0;
    }
    return report;
}

} // namespace nagus
