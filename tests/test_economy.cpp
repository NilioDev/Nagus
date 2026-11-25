// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/game.hpp"
#include "nagus/game/research.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

using namespace nagus;
using namespace nagus::test;

namespace {

// A small set of buildings: one facility for each output, a better farm and
// a structure.
EconomyData sampleEconomy() {
    EconomyData d;
    auto facility = [&](const char* name, Output o, int level, int amount, int id, int from) {
        Building b;
        b.name = name;
        b.output = o;
        b.level = level;
        b.amount = amount;
        b.workers = 10;
        b.cost = static_cast<std::uint32_t>(50 * (level + 1));
        b.id = id;
        b.upgradesFrom = from;
        d.buildings.push_back(b);
    };
    facility("Farm", Output::Food, 0, 20, 1, -1);
    facility("Factory", Output::Industry, 1, 10, 2, -1);
    facility("Power Plant", Output::Energy, 1, 20, 3, -1);
    facility("Lab", Output::Research, 1, 10, 4, -1);
    facility("Spy Den", Output::Intelligence, 1, 10, 5, -1);
    facility("Better Farm", Output::Food, 2, 30, 6, 1);
    Building yard;
    yard.name = "Shipyard";
    yard.kind = Building::Class::Structure;
    yard.bonus = bonus::kShipyard;
    yard.excluded = 0x1d; // the Federation only
    yard.cost = 300;
    yard.energy = 20;
    yard.id = 7;
    d.buildings.push_back(yard);
    d.growth = {{10, 50, 1'000'000}, {50, 1000, 100'000}};
    d.habitats = {{6, 10, {10, 10, 10, 10, 10, 10}}};
    return d;
}

StarSystem sampleSystem() {
    StarSystem s;
    s.name = "Home";
    for (int i = 0; i < 2; ++i) {
        Planet p;
        p.type = 6;
        p.atmosphere = 10;
        p.room = 100;
        s.planets.push_back(p);
    }
    return s;
}

} // namespace

NGS_TEST("a home colony feeds its people and has work for them") {
    const EconomyData data = sampleEconomy();
    const StarSystem s = sampleSystem();
    EmpireState e;
    e.tech.fill(1);
    Colony c = homeColony(0, s, 1, e, data);
    NGS_CHECK(c.maxPopulation == 200 && c.population == 140);
    const ColonyOutput out = colonyOutput(c, data);
    NGS_CHECK(out.made[static_cast<std::size_t>(Output::Food)] >= out.foodNeeded);
    NGS_CHECK(out.made[static_cast<std::size_t>(Output::Industry)] > 0);
    NGS_CHECK(out.shipyard && out.energyNeeded == 20);
    int working = 0;
    for (const int w : c.working) {
        working += w;
    }
    NGS_CHECK(working <= c.population / 10);
    // Fed people grow, up to the room there is.
    for (int t = 0; t < 200; ++t) {
        const TurnReport r = colonyTurn(c, data);
        NGS_CHECK(!r.starving);
    }
    NGS_CHECK(c.population > 140 && c.population <= 200);
    // Without farmers they go hungry.
    setWorking(c, Output::Food, 0);
    c.food = 0;
    const int before = c.population;
    NGS_CHECK(colonyTurn(c, data).starving && c.population < before);
}

NGS_TEST("industry builds the queue, then makes credits") {
    const EconomyData data = sampleEconomy();
    const StarSystem s = sampleSystem();
    EmpireState e;
    e.tech.fill(1);
    Colony c = homeColony(0, s, 1, e, data);
    auto options = buildOptions(c, e, s, data);
    // The Federation already has its shipyard; another empire's is not offered.
    for (const auto& o : options) {
        NGS_CHECK(o.kind != BuildItem::Kind::Structure);
        NGS_CHECK(o.kind != BuildItem::Kind::Upgrade);
    }
    e.tech.fill(2);
    options = buildOptions(c, e, s, data);
    const BuildItem upgrade{BuildItem::Kind::Upgrade, 5};
    NGS_CHECK(std::find(options.begin(), options.end(), upgrade) != options.end());
    c.queue.push_back(upgrade);
    NGS_CHECK(buyCost(c, data) == static_cast<int>(itemCost(upgrade, c, data)) * 2);
    const int farms = c.facilities[static_cast<std::size_t>(Output::Food)];
    std::vector<std::string> done;
    for (int t = 0; t < 100 && done.empty(); ++t) {
        done = colonyTurn(c, data).finished;
    }
    NGS_CHECK(done.size() == 1 && done[0] == "Better Farm");
    NGS_CHECK(c.facility[static_cast<std::size_t>(Output::Food)] == 5);
    NGS_CHECK(c.facilities[static_cast<std::size_t>(Output::Food)] == farms);
    NGS_CHECK(c.queue.empty() && c.progress == 0);
    NGS_CHECK(colonyTurn(c, data).credits > c.population / 5);
}

NGS_TEST("planet bonuses add food and energy, dilithium speeds up ships") {
    EconomyData data = sampleEconomy();
    ShipClass ship;
    ship.name = "Cruiser";
    ship.cost = 100000;
    data.ships.push_back(ship);
    Game g;
    g.economy = data;
    g.galaxy.systems.push_back(sampleSystem());
    StarSystem& s = g.galaxy.systems[0];
    s.planets[0].bonus = kFoodBonus;
    s.planets[1].bonus = kEnergyBonus;
    s.planets[1].terraformed = false; // counts only once terraformed
    s.dilithium = true;
    EmpireState e;
    e.tech.fill(1);
    g.colonies.push_back(homeColony(0, s, 1, e, data));
    Colony& c = g.colonies[0];
    const ColonyOutput before = colonyOutput(c, data);
    refreshBonuses(g);
    NGS_CHECK(c.foodBonus == kPlanetBonusPercent && c.energyBonus == 0 && c.dilithium);
    const auto food = static_cast<std::size_t>(Output::Food);
    NGS_CHECK(colonyOutput(c, data).made[food] == before.made[food] * (100 + kPlanetBonusPercent) / 100);
    s.planets[1].terraformed = true;
    refreshBonuses(g);
    NGS_CHECK(c.energyBonus == kPlanetBonusPercent);
    const auto energy = static_cast<std::size_t>(Output::Energy);
    NGS_CHECK(colonyOutput(c, data).made[energy] == before.made[energy] * (100 + kPlanetBonusPercent) / 100);
    // The same ship gets more industry with dilithium.
    c.queue.push_back({BuildItem::Kind::Ship, 0});
    Colony without = c;
    without.dilithium = false;
    (void)colonyTurn(c, data);
    (void)colonyTurn(without, data);
    NGS_CHECK(c.progress > without.progress && c.progress == without.progress * (100 + kDilithiumPercent) / 100);
}

NGS_TEST("the game's buildings: every empire's home starts out working") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    NGS_CHECK(economy.buildings.size() == 318);
    NGS_CHECK(economy.buildings[0].name == "Primitive Farm" && economy.buildings[0].output == Output::Food);
    NGS_CHECK(economy.buildings[0].workers == 10 && economy.buildings[0].amount == 20);
    NGS_CHECK(economy.growth.size() == 10);
    GameSettings settings;
    Game g = newGame(settings, loadGalaxyData(res, lex), economy, 3);
    NGS_CHECK(g.colonies.size() == kEmpires);
    // Bonuses: dilithium at every home and elsewhere, food and energy
    // planets.
    int dilithium = 0;
    int bonuses[3] = {};
    for (const auto& s : g.galaxy.systems) {
        dilithium += s.dilithium ? 1 : 0;
        NGS_CHECK(s.star || (!s.dilithium && s.planets.empty()));
        for (const auto& p : s.planets) {
            ++bonuses[p.bonus];
        }
    }
    NGS_CHECK(dilithium > kEmpires && bonuses[kFoodBonus] > 0 && bonuses[kEnergyBonus] > 0);
    for (const auto& c : g.colonies) {
        NGS_CHECK(c.dilithium);
    }
    for (int e = 0; e < kEmpires; ++e) {
        const Colony* c = g.colonyAt(g.galaxy.homes[static_cast<std::size_t>(e)]);
        NGS_CHECK(c != nullptr && c->owner == e);
        const ColonyOutput out = colonyOutput(*c, g.economy);
        NGS_CHECK(out.made[static_cast<std::size_t>(Output::Food)] >= out.foodNeeded);
        NGS_CHECK(out.made[static_cast<std::size_t>(Output::Energy)] >= out.energyNeeded);
        NGS_CHECK(out.shipyard);
        for (int o = 0; o < kOutputs; ++o) {
            NGS_CHECK(out.made[static_cast<std::size_t>(o)] > 0);
        }
        NGS_CHECK(!buildOptions(*c, g.empires[static_cast<std::size_t>(e)], g.galaxy.systems[static_cast<std::size_t>(c->system)], g.economy).empty());
        if (std::getenv("NAGUS_PRINT_ECONOMY") != nullptr) {
            std::printf("%s: pop %d/%d food %d/%d industry %d energy %d/%d research %d intel %d credits %d\n",
                        g.galaxy.systems[static_cast<std::size_t>(c->system)].name.c_str(), c->population, c->maxPopulation,
                        out.made[2], out.foodNeeded, out.made[1], out.made[3], out.energyNeeded, out.made[0], out.made[4], out.credits);
        }
    }
    for (int t = 0; t < 50; ++t) {
        endTurn(g);
    }
    NGS_CHECK(g.turn == 51);
    for (const auto& c : g.colonies) {
        NGS_CHECK(c.population > 0);
    }
}

NGS_TEST("research shares add up to 100 and fields reach new levels") {
    EmpireState e;
    e.tech.fill(1);
    auto sum = [&] {
        int s = 0;
        for (const int v : e.share) {
            s += v;
        }
        return s;
    };
    NGS_CHECK(sum() == 100);
    setShare(e, 0, 50);
    NGS_CHECK(e.share[0] == 50 && sum() == 100);
    e.locked[1] = true;
    const int kept = e.share[1];
    setShare(e, 2, 100);
    NGS_CHECK(e.share[1] == kept && e.share[2] == 100 - kept && sum() == 100);
    setShare(e, 2, 0);
    NGS_CHECK(e.share[2] == 0 && e.share[1] == kept && sum() == 100);

    std::vector<Tech> techs;
    for (int f = 0; f < kTechFields; ++f) {
        for (int level = 0; level <= kMaxTechLevel; ++level) {
            techs.push_back({"T" + std::to_string(f) + "." + std::to_string(level), static_cast<TechField>(f), level,
                             static_cast<std::uint32_t>(level * 100), 0});
        }
    }
    EmpireState r;
    r.tech.fill(1);
    r.share = {100, 0, 0, 0, 0, 0};
    r.researchPoints = 450 / kFieldBoost;
    const auto reached = researchTurn(r, techs);
    NGS_CHECK(reached.size() == 1 && reached[0]->name == "T0.2");
    NGS_CHECK(r.tech[0] == 2 && r.progress[0] == 250 && r.researchPoints == 0);
    r.researchPoints = 50 / kFieldBoost + 1;
    NGS_CHECK(researchTurn(r, techs).size() == 1 && r.tech[0] == 3 && r.progress[0] < kFieldBoost);
    // Sociology follows the average of the six fields.
    r.tech = {7, 7, 7, 7, 7, 7, 0};
    updateSociology(r);
    NGS_CHECK(r.tech[static_cast<std::size_t>(TechField::Sociology)] == 7);
}
