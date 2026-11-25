// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/diplomacy.hpp"
#include "nagus/game/intel.hpp"
#include "nagus/game/save.hpp"
#include "nagus/ui/intel_screen.hpp"

#include <algorithm>

using namespace nagus;
using namespace nagus::test;

NGS_TEST("intelligence: shares stay within bounds") {
    EmpireState e;
    setSecurity(e, 140);
    NGS_CHECK(e.security == 100);
    setSecurity(e, -5);
    NGS_CHECK(e.security == 0);
    setIntelShare(e, 2, false, 60);
    setIntelShare(e, 3, true, 70);
    NGS_CHECK(e.intel[2].espionage == 60 && e.intel[3].sabotage == 40);
    setIntelShare(e, 9, false, 10); // no such empire
    setIntelShare(e, -1, true, 10);
    setIntelFocus(e, 3, IntelFocus::Military);
    NGS_CHECK(e.intel[3].focus == IntelFocus::Military);

    // Nothing planned: every point guards the empire.
    EmpireState quiet;
    quiet.intelPoints = 80;
    quiet.security = 25;
    NGS_CHECK(securityPoints(quiet) == 80 && operationPoints(quiet, 2, false) == 0);
    // Otherwise security takes its share and the plan shares the rest.
    e.intelPoints = 200;
    e.security = 50;
    NGS_CHECK(securityPoints(e) == 100);
    NGS_CHECK(operationPoints(e, 2, false) == 60 && operationPoints(e, 3, true) == 40);
    NGS_CHECK(operationPoints(e, 1, false) == 0);
}

NGS_TEST("intelligence: operations happen, are reported and saved") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    const GalaxyData data = loadGalaxyData(res, lex);
    GameSettings settings;
    settings.empire = 1;
    Game g = newGame(settings, data, economy, 8);
    std::fill(g.explored.begin(), g.explored.end(), std::uint8_t{0x1f});
    (void)diplomacyTurn(g, {}, {});
    NGS_CHECK(met(g, 1, 2));

    // The Federation steals from the Ferengi, who keep no security.
    auto& fed = g.empires[1];
    auto& fer = g.empires[2];
    fer.security = 0;
    fer.intelPoints = 0;
    fed.security = 0;
    setIntelShare(fed, 2, false, 100);
    setIntelFocus(fed, 2, IntelFocus::Economy);
    fed.intelPoints = kOperationCost * 4;
    fer.credits = 1000;
    const int credits = fed.credits;
    const auto heard = intelTurn(g);
    NGS_CHECK(fed.lastIntel == kOperationCost * 4 && fed.intelPoints == 0);
    NGS_CHECK(fed.espionagePool[2] == kOperationCost * 3);
    NGS_CHECK(g.intelReports.size() == 1 && heard.size() == 1);
    const IntelReport& r = g.intelReports.back();
    NGS_CHECK(r.attacker == 1 && r.target == 2 && !r.sabotage && r.focus == IntelFocus::Economy);
    if (r.success) {
        NGS_CHECK(r.value == 100 && fed.credits == credits + 100 && fer.credits == 900);
    }
    NGS_CHECK(!intelText(lex, g, r).empty());

    // Sabotage of the Klingons' research, a few times.
    auto& kli = g.empires[3];
    setIntelShare(fed, 2, false, 0);
    setIntelShare(fed, 3, true, 100);
    setIntelFocus(fed, 3, IntelFocus::Science);
    kli.security = 0;
    for (int t = 0; t < 5; ++t) {
        g.turn += 1;
        kli.intelPoints = 0;
        fed.intelPoints = kOperationCost;
        (void)intelTurn(g);
    }
    NGS_CHECK(std::count_if(g.intelReports.begin(), g.intelReports.end(), [](const IntelReport& x) { return x.sabotage; }) == 5);

    // The reports are kept to a limit.
    for (int t = 0; t < 80; ++t) {
        g.turn += 1;
        fed.intelPoints = kOperationCost;
        (void)intelTurn(g);
    }
    NGS_CHECK(g.intelReports.size() == kMaxReports);

    // The plan and the reports are saved.
    const Game back = loadGame(saveGame(g, {}), economy);
    NGS_CHECK(back.intelReports.size() == g.intelReports.size());
    NGS_CHECK(back.intelReports.back().turn == g.intelReports.back().turn);
    NGS_CHECK(back.empires[1].intel[3].sabotage == 100 && back.empires[1].intel[3].focus == IntelFocus::Science);
    NGS_CHECK(back.empires[1].sabotagePool == fed.sabotagePool && back.empires[1].lastIntel == fed.lastIntel);
}
