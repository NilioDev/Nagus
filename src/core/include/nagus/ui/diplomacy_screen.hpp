// SPDX-License-Identifier: GPL-3.0-or-later
//
// The diplomacy screen: the races the player has met in a list at the
// bottom, the chosen one on the left, and on the right what to do with it:
// answer its proposals, propose treaties, give gifts, declare war, or read
// about the race.
#pragma once

#include "nagus/game/diplomacy.hpp"
#include "nagus/ui/menus.hpp"

#include <memory>

namespace nagus {

// The turn report's line for a diplomacy event.
std::string diplomacyText(const Lexicon& lexicon, const Game& game, const DiplomacyEvent& e);
// The name of a treaty, from the game's texts.
std::string treatyName(const Lexicon& lexicon, Treaty t);

class DiplomacyScreen : public MenuScreen {
public:
    // Element numbers in the diplomacy layout (<x>diplomt.wdf).
    static constexpr std::uint32_t kSummary = 0x1394;
    static constexpr std::uint32_t kActive = 0x1388;
    static constexpr std::uint32_t kPropose = 0x1389;
    static constexpr std::uint32_t kEvents = 0x138a;
    static constexpr std::uint32_t kRaceInfo = 0x138b;
    static constexpr std::uint32_t kUp = 0x1390;
    static constexpr std::uint32_t kDown = 0x1391;
    static constexpr std::uint32_t kName = 0x1395;
    static constexpr std::uint32_t kStatus = 0x152f;
    static constexpr std::uint32_t kPortrait = 0x138c;
    static constexpr std::uint32_t kRegard = 0x1530;
    static constexpr std::uint32_t kArea = 0x1393;
    static constexpr std::uint32_t kTop = 0x138e;
    static constexpr std::uint32_t kList = 0x138f;
    // The proposal buttons (<x>dmajbtn.wdf for empires, <x>dminbtn.wdf for
    // minor races).
    static constexpr std::uint32_t kPeaceTreaty = 0x1396;
    static constexpr std::uint32_t kRequest = 0x1397;
    static constexpr std::uint32_t kAlliance = 0x1398;
    static constexpr std::uint32_t kWarPact = 0x1399;
    static constexpr std::uint32_t kDeclareWar = 0x139a;
    static constexpr std::uint32_t kSurrender = 0x139b;
    static constexpr std::uint32_t kGift = 0x139c;
    static constexpr std::uint32_t kFriendship = 0x139d;
    static constexpr std::uint32_t kAffiliation = 0x139e;
    static constexpr std::uint32_t kMembership = 0x139f;
    static constexpr std::uint32_t kMinorRequest = 0x13a0;
    static constexpr std::uint32_t kNonAggression = 0x13a2;
    // In the dialogs: send and cancel, the amount of a gift.
    static constexpr std::uint32_t kSend = 0x1450;
    static constexpr std::uint32_t kCancel = 0x1451;
    static constexpr std::uint32_t kLess = 0x1554;
    static constexpr std::uint32_t kMore = 0x1553;
    static constexpr std::uint32_t kAmount = 0x154f;
    static constexpr std::uint32_t kDialogTitle = 0x1455;
    static constexpr std::uint32_t kDialogText = 0x13ec;
    // Answering a proposal (<x>d_actev.wdf).
    static constexpr std::uint32_t kAccept = 0x1452;
    static constexpr std::uint32_t kReject = 0x1453;
    static constexpr std::uint32_t kOfferTitle = 0x13ec;
    static constexpr std::uint32_t kOfferText = 0x13ed;
    // Race information (<x>d_rinfo.wdf).
    static constexpr std::uint32_t kInfoTitle = 0x1485;
    static constexpr std::uint32_t kInfoText = 0x1482;
    static constexpr std::uint32_t kInfoMore = 0x1484;

    enum class Tab { Active, Propose, Events, RaceInfo };
    enum class Dialog { None, Peace, Gift, War };

    DiplomacyScreen(Assets& assets, Game& game);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    Tab tab() const { return tab_; }
    void showTab(Tab t);
    Dialog dialog() const { return dialog_; }
    // The races in the list, and the chosen one.
    const std::vector<int>& races() const { return races_; }
    int chosen() const { return chosen_; }
    void choose(int race);
    // Where a line of the list is, when shown.
    std::optional<Rect> raceLine(int i) const;
    // The answer to the last proposal, as shown.
    const std::string& reply() const { return reply_; }
    // Clicks a button of the screen, its panel or its dialog (for tests).
    void click(std::uint32_t id);
    // Where a button is now, on whichever panel shows it.
    std::optional<Rect> button(std::uint32_t id) const;
    // What happened here, for the turn report.
    std::vector<DiplomacyEvent> takeEvents();

private:
    void refresh();
    void openSub();
    void send(Treaty t);
    char prefix() const;
    const Font& font(int size) const;

    Assets& assets_;
    Game& game_;
    Panel panel_;
    std::unique_ptr<Panel> sub_; // the panel in the area on the right
    Tab tab_ = Tab::Active;
    Dialog dialog_ = Dialog::None;
    Treaty peace_ = Treaty::NonAggression; // chosen in the peace dialog
    int gift_ = 100;
    std::vector<int> races_;
    int chosen_ = -1;
    int first_ = 0; // the first line shown
    std::string reply_;
    std::vector<DiplomacyEvent> events_;
    Point pointer_;
};

} // namespace nagus
