// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/diplomacy.hpp"
#include "nagus/game/multiplayer.hpp"
#include "nagus/game/research.hpp"
#include "nagus/game/save.hpp"

#include <algorithm>

using namespace nagus;
using namespace nagus::test;

NGS_TEST("multiplayer: turn reports go to the players intact") {
    TurnSummary s;
    s.events = {"Sol: Shipyard"};
    s.researched = {"Weapons 2"};
    s.battles.push_back({{3, 4}, {1, 3}, {2, 1}, {0, 1}});
    s.assaults.push_back({7, 1, 3, 12, true});
    s.diplomacy.push_back({kTextMet, 1, 2});
    s.diplomacy.push_back({278, 3, 1});
    IntelReport ir;
    ir.turn = 5;
    ir.attacker = 1;
    ir.target = 4;
    ir.sabotage = true;
    ir.focus = IntelFocus::Military;
    ir.value = 3;
    s.intel.push_back(ir);
    s.fallen = {2};
    s.happenings.push_back({909, {"Sol", "4"}, 1, 7});
    s.ending = Ending::Domination;
    const Bytes data = saveSummary(s);
    const TurnSummary t = loadSummary(data);
    NGS_CHECK(t.events == s.events && t.researched == s.researched && t.colonized.empty());
    NGS_CHECK(t.battles.size() == 1 && t.battles[0].at == Sector{3, 4} && t.battles[0].lost == std::vector<int>{0, 1});
    NGS_CHECK(t.assaults.size() == 1 && t.assaults[0].captured && t.assaults[0].killed == 12);
    NGS_CHECK(t.diplomacy.size() == 2 && t.diplomacy[0].text == kTextMet && t.diplomacy[1].text == 278);
    NGS_CHECK(t.intel.size() == 1 && t.intel[0].focus == IntelFocus::Military && t.intel[0].sabotage);
    NGS_CHECK(t.fallen == std::vector<int>{2} && t.happenings.size() == 1 && t.happenings[0].values.size() == 2);
    NGS_CHECK(t.ending == Ending::Domination);
    // Damaged data is refused, not read past its end.
    for (std::size_t cut = 0; cut < data.size(); cut += 7) {
        bool refused = false;
        try {
            (void)loadSummary(ByteView(data.data(), cut));
        } catch (const FormatError&) {
            refused = true;
        }
        NGS_CHECK(refused);
    }
}

NGS_TEST("multiplayer: each player's orders come together, and the turn ends the same for all") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    GameSettings settings;
    settings.empire = 1;
    settings.randomEvents = true;
    Game start = newGame(settings, loadGalaxyData(res, lex), economy, 9);
    start.humans[3] = true; // the Klingons are played by someone too
    NGS_CHECK(start.isHuman(1) && start.isHuman(3) && !start.isHuman(0));
    for (int i = 0; i < 3; ++i) {
        (void)endTurn(start);
    }
    // Meet, so they can make treaties.
    std::vector<DiplomacyEvent> ev;
    // Each plays their own copy.
    Game fed = start;
    Game kli = start;
    kli.player = 3;
    setShare(fed.empires[1], 0, 40);
    setShare(kli.empires[3], 5, 50);
    fed.empires[1].credits -= 100; // bought something
    const int fedFleet = [&] {
        for (const auto& f : fed.fleets) {
            if (f.owner == 1) {
                return f.id;
            }
        }
        return -1;
    }();
    NGS_CHECK(fedFleet > 0);
    fed.fleet(fedFleet)->destination = Sector{0, 0};
    // The Klingons give the Federation a gift, if they have met.
    const bool met12 = met(kli, 3, 1);
    if (met12) {
        NGS_CHECK(giveGift(kli, 3, 1, 50, ev));
    }

    Game merged = start;
    mergeOrders(merged, start, fed, 1);
    mergeOrders(merged, start, kli, 3);
    NGS_CHECK(merged.empires[1].share[0] == 40);
    NGS_CHECK(merged.empires[3].share[5] == 50);
    NGS_CHECK(merged.fleet(fedFleet)->destination == Sector{0, 0});
    NGS_CHECK(merged.empires[1].credits == start.empires[1].credits - 100 + (met12 ? 50 : 0));
    NGS_CHECK(merged.empires[3].credits == start.empires[3].credits - (met12 ? 50 : 0));

    // The turn ends once for each player, the galaxy the same each time.
    const auto turns = endMultiplayerTurn(merged);
    NGS_CHECK(turns.size() == 2 && turns[0].empire == 1 && turns[1].empire == 3);
    NGS_CHECK(turns[0].game.player == 1 && turns[1].game.player == 3);
    Game a = turns[0].game;
    Game b = turns[1].game;
    b.player = a.player;
    b.ending = a.ending;
    SaveInfo info;
    NGS_CHECK(saveGame(a, info) == saveGame(b, info));
    NGS_CHECK(a.turn == start.turn + 1);
    // And it goes through a saved game and back, humans included.
    const Game again = loadGame(saveGame(turns[1].game, info), economy);
    NGS_CHECK(again.player == 3 && again.humans[3] && again.isHuman(1) && !again.isHuman(0));
}

NGS_TEST("multiplayer: a proposal from one player waits for the other") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    GameSettings settings;
    settings.empire = 1;
    Game g = newGame(settings, loadGalaxyData(res, lex), economy, 9);
    g.humans[3] = true;
    for (auto& r : g.relations) {
        if (r.treaty == Treaty::None) {
            r.treaty = Treaty::Neutral;
        }
    }
    std::vector<DiplomacyEvent> ev;
    const Game start = g;
    NGS_CHECK(propose(g, 1, 3, Treaty::NonAggression, ev) == Reply::Waiting);
    NGS_CHECK(g.offers.size() == 1 && g.offers[0].to == 3);
    Game merged = start;
    mergeOrders(merged, start, g, 1);
    NGS_CHECK(merged.offers.size() == 1);
    // The Klingon player answers on their copy.
    Game kli = merged;
    kli.player = 3;
    const Game kliStart = kli;
    answerOffer(kli, 0, true, ev);
    NGS_CHECK(treaty(kli, 1, 3) == Treaty::NonAggression);
    Game next = merged;
    mergeOrders(next, kliStart, kli, 3);
    NGS_CHECK(next.offers.empty() && treaty(next, 1, 3) == Treaty::NonAggression && treaty(next, 3, 1) == Treaty::NonAggression);
}

#include "nagus/net/session.hpp"

#include <chrono>
#include <thread>

NGS_TEST("multiplayer over the network: joining, choosing empires and a turn") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    auto host = HostSession::open("Test game", "secret", "Picard", economy, 0);
    if (!host) {
        NGS_SKIP("no network here");
    }
    const std::string address = "127.0.0.1:" + std::to_string(host->port());
    auto riker = ClientSession::connect(address, "Riker", "secret", economy);
    auto worf = ClientSession::connect(address, "Worf\x01\x02", "secret", economy);
    auto spy = ClientSession::connect(address, "Spy", "wrong", economy);
    auto pump = [&](auto done) {
        for (int i = 0; i < 2000 && !done(); ++i) {
            host->poll();
            for (auto* c : {riker.get(), worf.get(), spy.get()}) {
                if (c != nullptr) {
                    c->poll();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return done();
    };
    NGS_CHECK(pump([&] {
        return riker->state() == ClientSession::State::Lobby && worf->state() == ClientSession::State::Lobby &&
               spy->state() == ClientSession::State::Refused && worf->lobby().players.size() == 3;
    }));
    NGS_CHECK(spy->refusedText() == kTextBadPassword);
    NGS_CHECK(host->lobby().players.size() == 3 && host->lobby().players[2].name == "Worf");
    NGS_CHECK(worf->lobby().game == "Test game");

    // Empires: one each; one already taken is refused.
    NGS_CHECK(host->chooseEmpire(1));
    riker->chooseEmpire(1);
    worf->chooseEmpire(3);
    NGS_CHECK(pump([&] { return worf->lobby().players.size() == 3 && worf->lobby().players[2].empire == 3; }));
    NGS_CHECK(host->lobby().players[1].empire == -1);
    NGS_CHECK(!host->canStart());
    riker->chooseEmpire(0);
    NGS_CHECK(pump([&] { return host->canStart(); }));

    GameSettings settings;
    settings.empire = 1;
    Game game = newGame(settings, loadGalaxyData(res, lex), economy, 4);
    for (const int e : {0, 1, 3}) {
        game.humans[static_cast<std::size_t>(e)] = true;
    }
    host->start(game);
    std::optional<Game> rikerGame;
    std::optional<Game> worfGame;
    NGS_CHECK(pump([&] {
        if (!rikerGame) {
            rikerGame = riker->takeStart();
        }
        if (!worfGame) {
            worfGame = worf->takeStart();
        }
        return rikerGame && worfGame;
    }));
    NGS_CHECK(rikerGame->player == 0 && worfGame->player == 3 && worfGame->humans[1]);

    // A turn: everyone sends orders, the host ends it.
    setShare(worfGame->empires[3], 2, 60);
    worf->submit(*worfGame);
    host->submit(game);
    NGS_CHECK(pump([&] { return host->waitingFor() == std::vector<int>{0}; }));
    NGS_CHECK(!host->finishTurn());
    riker->submit(*rikerGame);
    NGS_CHECK(pump([&] { return host->waitingFor().empty(); }));
    const auto mine = host->finishTurn();
    NGS_CHECK(mine && mine->game.turn == game.turn + 1 && mine->empire == 1);
    std::optional<PlayerTurn> worfTurn;
    NGS_CHECK(pump([&] {
        if (!worfTurn) {
            worfTurn = worf->takeTurn();
        }
        return worfTurn.has_value();
    }));
    NGS_CHECK(worfTurn->game.player == 3 && worfTurn->game.turn == game.turn + 1);
    NGS_CHECK(worfTurn->game.empires[3].share[2] == 60);

    // A player who leaves: the computer plays their empire.
    riker.reset();
    NGS_CHECK(pump([&] { return host->lobby().players.size() == 2; }));
    const auto notices = host->takeNotices();
    NGS_CHECK(notices.size() == 1 && notices[0] == "Riker has left");
    host->submit(mine->game);
    worf->submit(worfTurn->game);
    NGS_CHECK(pump([&] { return host->waitingFor().empty(); }));
    const auto next = host->finishTurn();
    NGS_CHECK(next && !next->game.humans[0]);
}
