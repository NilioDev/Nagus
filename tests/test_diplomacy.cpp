// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/combat.hpp"
#include "nagus/game/diplomacy.hpp"
#include "nagus/game/save.hpp"

#include <algorithm>

using namespace nagus;
using namespace nagus::test;

NGS_TEST("diplomacy: meeting, treaties, war and peace, gifts and minor races joining") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    const GalaxyData data = loadGalaxyData(res, lex);
    // The race table's character: the Klingons are the most warlike.
    NGS_CHECK(data.races.size() == 35);
    NGS_CHECK(data.races[3].aggression == 120 && data.races[2].aggression == 80 && data.races[1].aggression == 90);
    NGS_CHECK(data.races[1].attitude[1] == 100 && data.races[1].attitude[3] == 75);
    NGS_CHECK(textAt(res.file("racedesc.rst"), data.races[2].description).rfind("The Ferengi", 0) == 0);
    NGS_CHECK(textAt(res.file("racedesc.rst"), 1u << 30).empty());

    GameSettings settings;
    settings.empire = 1;
    Game g = newGame(settings, data, economy, 4);
    NGS_CHECK(g.relations.size() == kEmpires * 35);
    NGS_CHECK(!met(g, 1, 2) && !hostile(g, 1, 3));
    std::vector<DiplomacyEvent> events;
    NGS_CHECK(!canPropose(g, 1, 2, Treaty::Friendship));

    // Once the whole map is known, everyone meets.
    std::fill(g.explored.begin(), g.explored.end(), std::uint8_t{0x1f});
    events = diplomacyTurn(g, {}, {});
    NGS_CHECK(met(g, 1, 2) && treaty(g, 1, 2) == Treaty::Neutral && treaty(g, 2, 1) == Treaty::Neutral);
    NGS_CHECK(std::any_of(events.begin(), events.end(), [](const DiplomacyEvent& e) { return e.text == kTextMet; }));

    // A treaty needs regard: the Ferengi refuse, then agree.
    auto setRegard = [&](int race, int empire, int value) {
        g.relations[static_cast<std::size_t>(empire * 35 + race)].regard = value;
    };
    setRegard(2, 1, 10);
    events.clear();
    NGS_CHECK(propose(g, 1, 2, Treaty::Friendship, events) == Reply::Rejected && treaty(g, 1, 2) == Treaty::Neutral);
    NGS_CHECK(!events.empty() && events.back().text == 386);
    setRegard(2, 1, 150);
    NGS_CHECK(propose(g, 1, 2, Treaty::Friendship, events) == Reply::Accepted && treaty(g, 2, 1) == Treaty::Friendship);
    NGS_CHECK(!canPropose(g, 1, 2, Treaty::NonAggression) && canPropose(g, 1, 2, Treaty::Alliance));

    // War, and peace once the war has gone on.
    NGS_CHECK(canDeclareWar(g, 1, 3));
    declareWar(g, 1, 3, events);
    NGS_CHECK(atWar(g, 3, 1) && hostile(g, 1, 3) && events.back().text == 278);
    setRegard(3, 1, 0);
    NGS_CHECK(!wouldAccept(g, 1, 3, Treaty::NonAggression));
    g.turn += 60;
    NGS_CHECK(propose(g, 1, 3, Treaty::NonAggression, events) == Reply::Accepted);
    NGS_CHECK(treaty(g, 1, 3) == Treaty::NonAggression && regard(g, 3, 1) >= 50);

    // Gifts move credits and win regard.
    const int credits = g.empires[1].credits;
    const int before = regard(g, 4, 1);
    NGS_CHECK(giveGift(g, 1, 4, 100, events));
    NGS_CHECK(g.empires[1].credits == credits - 100 && regard(g, 4, 1) == before + 4);
    NGS_CHECK(!giveGift(g, 1, 4, 1000000, events));

    // A minor race: friendship, affiliation, then it joins.
    int minor = -1;
    for (int r = kEmpires; r < 35 && minor < 0; ++r) {
        minor = minorHome(g, r) >= 0 ? r : -1;
    }
    NGS_CHECK(minor >= kEmpires);
    setRegard(minor, 1, 200);
    NGS_CHECK(!canPropose(g, 1, minor, Treaty::Membership));
    NGS_CHECK(propose(g, 1, minor, Treaty::Friendship, events) == Reply::Accepted);
    NGS_CHECK(propose(g, 1, minor, Treaty::Affiliation, events) == Reply::Accepted);
    NGS_CHECK(!wouldAccept(g, 1, minor, Treaty::Membership)); // not long enough
    g.turn += 20;
    NGS_CHECK(propose(g, 1, minor, Treaty::Membership, events) == Reply::Accepted);
    const Colony* joined = g.colonyAt(minorHome(g, minor));
    NGS_CHECK(joined != nullptr && joined->owner == 1 && joined->population > 0);
    NGS_CHECK(events.back().text == kTextJoined);
    // Nobody else can win it over now.
    NGS_CHECK(!canPropose(g, 3, minor, Treaty::Friendship));

    // A computer player's proposal waits for the player's answer.
    setRegard(1, 0, 200);
    g.offers.clear();
    NGS_CHECK(treaty(g, 0, 1) == Treaty::Neutral);
    NGS_CHECK(propose(g, 0, 1, Treaty::Friendship, events) == Reply::Waiting);
    NGS_CHECK(g.offers.size() == 1 && treaty(g, 0, 1) == Treaty::Neutral);
    answerOffer(g, 0, true, events);
    NGS_CHECK(g.offers.empty() && treaty(g, 0, 1) == Treaty::Friendship);

    // All of it is saved.
    const Bytes saved = saveGame(g, {});
    const Game back = loadGame(saved, economy);
    NGS_CHECK(treaty(back, 1, 2) == Treaty::Friendship && regard(back, minor, 1) == regard(g, minor, 1));
    NGS_CHECK(back.races.size() == 35 && back.races[3].aggression == 120);
    NGS_CHECK(saveGame(back, {}) == saved);
}
