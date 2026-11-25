// SPDX-License-Identifier: GPL-3.0-or-later
//
// The system screen: one of the player's colonies. Tabs on the left choose
// what the middle shows (a summary of the planets, the people at work,
// energy for the structures, what can be built, the structures); the build
// queue is on the right and the system's planets along the bottom.
#pragma once

#include "nagus/game/game.hpp"
#include "nagus/ui/menus.hpp"

#include <memory>

namespace nagus {

class SystemScreen : public MenuScreen {
public:
    // Element numbers in the system screen's layout.
    static constexpr std::uint32_t kSummary = 0x7d1;
    static constexpr std::uint32_t kProduction = 0x7d2;
    static constexpr std::uint32_t kStructures = 0x7d3;
    static constexpr std::uint32_t kBuildList = 0x7d4;
    static constexpr std::uint32_t kEnergy = 0x7d5;
    static constexpr std::uint32_t kShipbuilding = 0x7d7;
    static constexpr std::uint32_t kBuy = 0x7d9;
    static constexpr std::uint32_t kBuild = 0x808;
    static constexpr std::uint32_t kSystems = 0x806;
    static constexpr std::uint32_t kMiddle = 0x7f6;
    static constexpr std::uint32_t kBottom = 0x7dc;
    static constexpr std::uint32_t kTitle = 0x7f4;
    static constexpr std::uint32_t kName = 0x7fb;
    static constexpr std::uint32_t kQueueTitle = 0x7fd;
    static constexpr std::uint32_t kCurrentPicture = 0x7da;
    static constexpr std::uint32_t kCurrentName = 0x7ef;
    static constexpr std::uint32_t kCurrentTurns = 0x7ee;
    static constexpr std::uint32_t kQueueSlots[4] = {0x803, 0x804, 0x805, 0x807};
    static constexpr std::uint32_t kFacts[9] = {0x7f3, 0x7e4, 0x7fe, 0x7e5, 0x7e2, 0x7e0, 0x7e3, 0x7eb, 0x7e1};
    // In the labor panel: for each row the picture, title, less, more and bar.
    static constexpr std::uint32_t kLaborRows[5][5] = {{0x8fc, 0x8fd, 0x900, 0x901, 0x902},
                                                       {0x906, 0x907, 0x90a, 0x90b, 0x90c},
                                                       {0x910, 0x911, 0x914, 0x915, 0x916},
                                                       {0x91a, 0x91b, 0x91e, 0x91f, 0x920},
                                                       {0x924, 0x925, 0x928, 0x929, 0x92a}};
    static constexpr Output kLaborOrder[5] = {Output::Food, Output::Industry, Output::Energy, Output::Intelligence,
                                              Output::Research};
    static constexpr std::uint32_t kLaborTitle = 0x92e;
    static constexpr std::uint32_t kUnassignedText = 0x92f;
    static constexpr std::uint32_t kUnassignedBar = 0x930;
    // In the build list panel.
    static constexpr std::uint32_t kBuildTitle = 0x8a8;
    static constexpr std::uint32_t kBuildItems = 0x899;
    static constexpr std::uint32_t kBuildPicture = 0x89a;
    static constexpr std::uint32_t kBuildDetails = 0x8a0;
    static constexpr std::uint32_t kBuildUp = 0x7f7;
    static constexpr std::uint32_t kBuildDown = 0x7f8;
    // In the structures panel: six places, each picture, name, line, line.
    static constexpr std::uint32_t kStructurePlaces[6][2] = {{0x837, 0x838}, {0x83b, 0x83c}, {0x840, 0x841},
                                                             {0x845, 0x846}, {0x84a, 0x84b}, {0x84f, 0x850}};
    static constexpr std::uint32_t kStructuresTitle = 0x8c2;
    static constexpr std::uint32_t kStructuresBack = 0x896;
    static constexpr std::uint32_t kStructuresNext = 0x897;
    // <x>numdlg.wdf: how many structures of a kind to scrap.
    static constexpr std::uint32_t kScrapUp = 0x52;
    static constexpr std::uint32_t kScrapDown = 0x53;
    static constexpr std::uint32_t kScrapCount = 0x54;
    static constexpr std::uint32_t kScrapText = 0x55;
    static constexpr std::uint32_t kScrapApply = 0x56;
    static constexpr std::uint32_t kScrapCancel = 0x57;
    static constexpr std::uint32_t kScrapTitle = 0x264a;
    // In the energy panel: fourteen places for structures that use energy.
    static constexpr std::uint32_t kEnergyTitle = 0x960;
    static constexpr std::uint32_t kEnergyBar = 0x964;
    static constexpr std::uint32_t kEnergyText = 0x966;

    enum class Tab { Summary, Production, Energy, BuildList, Shipbuilding, Structures };

    SystemScreen(Assets& assets, Game& game, int colony);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    Tab tab() const { return tab_; }
    void showTab(Tab t);
    int colony() const { return colony_; }
    // The build list: what can be built and which line is chosen.
    const std::vector<BuildItem>& options() const { return options_; }
    int chosenOption() const { return chosenOption_; }
    void chooseOption(int i);
    // Puts the chosen line of the build list at the end of the queue.
    void addChosen();
    // Where a line of the build list is, when shown.
    std::optional<Rect> optionRect(int i) const;
    // The structures tab: where the picture of a place (0-5) on this page
    // is, when a structure is there; clicking it asks how many of its kind
    // to scrap.
    std::optional<Rect> structureRect(int place) const;
    bool scrapOpen() const { return scrapBuilding_ >= 0; }
    int scrapCount() const { return scrapCount_; }
    std::optional<Rect> scrapButton(std::uint32_t id) const;

private:
    Colony& col() { return game_.colonies[static_cast<std::size_t>(colony_)]; }
    const Colony& col() const { return game_.colonies[static_cast<std::size_t>(colony_)]; }
    const StarSystem& system() const;
    Panel* sub();
    void refresh();
    void drawFacts(Canvas& canvas);
    void drawQueue(Canvas& canvas);
    void drawSummary(Canvas& canvas, std::uint64_t ms);
    void drawLabor(Canvas& canvas);
    void drawBuildList(Canvas& canvas);
    void drawStructures(Canvas& canvas);
    void drawEnergy(Canvas& canvas);
    void clicked(std::uint32_t id);
    std::vector<int> poweredStructures() const; // indices into the colony's structures
    std::string turnsText(int turns) const;
    const Font& font(int size) const;

    Assets& assets_;
    Game& game_;
    int colony_;
    char prefix_;
    Panel panel_;
    Panel labor_;
    Panel build_;
    Panel structures_;
    Panel energy_;
    Panel strip_;
    Tab tab_ = Tab::Summary;
    std::vector<BuildItem> options_;
    int chosenOption_ = 0;
    int firstOption_ = 0; // the first line shown
    int structurePage_ = 0;
    Panel scrap_;
    int scrapBuilding_ = -1; // the kind being scrapped, -1 for none
    int scrapCount_ = 1;
    int scrapMost() const;
    void scrapClicked(std::uint32_t id);
    Point pointer_;
    std::vector<std::string> animations_;
};

} // namespace nagus
