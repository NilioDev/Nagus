// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/game/save.hpp"

using namespace nagus;
using namespace nagus::test;

namespace {

Game smallGame() {
    Game g;
    g.galaxy.width = 10;
    g.galaxy.height = 8;
    g.galaxy.homes.fill(-1);
    return g;
}

bool rejected(ByteView data, const EconomyData& economy) {
    try {
        (void)loadGame(data, economy);
    } catch (const FormatError&) {
        return true;
    }
    return false;
}

} // namespace

NGS_TEST("an empty game saves and loads") {
    Game g = smallGame();
    g.turn = 3;
    const Bytes saved = saveGame(g, {"Federation", "Turn 3", 3});
    const SaveInfo info = readSaveInfo(saved);
    NGS_CHECK(info.name == "Federation" && info.description == "Turn 3" && info.turn == 3);
    const Game back = loadGame(saved, EconomyData{});
    NGS_CHECK(back.turn == 3);
    NGS_CHECK(saveGame(back, {"Federation", "Turn 3", 3}) == saved);
}

NGS_TEST("saved games that are damaged are refused") {
    const EconomyData none;
    const Bytes saved = saveGame(smallGame(), {"a", "b", 1});
    NGS_CHECK(!rejected(saved, none));
    for (std::size_t n = 0; n < saved.size(); ++n) {
        NGS_CHECK(rejected(ByteView(saved.data(), n), none));
    }
    Bytes longer = saved;
    longer.push_back(0);
    NGS_CHECK(rejected(longer, none));
    Bytes wrong = saved;
    wrong[0] = 'X';
    NGS_CHECK(rejected(wrong, none));
    NGS_CHECK(rejected(ByteView{}, none));
}

NGS_TEST("a game in progress saves and loads the same") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const Lexicon lex(res.file("lexicon.dic"));
    const EconomyData economy = loadEconomyData(res);
    GameSettings settings;
    settings.empire = 2;
    Game g = newGame(settings, loadGalaxyData(res, lex), economy, 5);
    for (int i = 0; i < 4; ++i) {
        (void)endTurn(g);
    }
    const SaveInfo info{"Ferengi", "Turn 5", g.turn};
    const Bytes saved = saveGame(g, info);
    Game back = loadGame(saved, economy);
    NGS_CHECK(back.turn == g.turn && back.player == g.player);
    NGS_CHECK(back.colonies.size() == g.colonies.size() && back.fleets.size() == g.fleets.size());
    NGS_CHECK(back.galaxy.systems.size() == g.galaxy.systems.size());
    NGS_CHECK(saveGame(back, info) == saved);
    // Both go on the same way.
    (void)endTurn(g);
    (void)endTurn(back);
    NGS_CHECK(saveGame(back, info) == saveGame(g, info));
    // Every damaged copy is refused, none crashes.
    for (std::size_t n = 0; n < saved.size(); n += 7) {
        NGS_CHECK(rejected(ByteView(saved.data(), n), economy));
    }
    // A smaller ship table than the game was saved with does not fit.
    EconomyData fewer = economy;
    fewer.ships.resize(1);
    fewer.buildings.resize(1);
    NGS_CHECK(rejected(saved, fewer));
}
