// SPDX-License-Identifier: GPL-3.0-or-later
//
// The main game screen: the galaxy map with the empire's panels around it.
// The map shows every sector's star or other object; the chosen sector is
// framed and described below the map. The d-pad and arrow keys move the
// chosen sector when the pointer is on the map; the Map button switches
// between the close view and the whole galaxy.
#pragma once

#include "nagus/game/game.hpp"
#include "nagus/ui/menus.hpp"

namespace nagus {

class GalaxyScreen : public MenuScreen {
public:
    // Element numbers in the main screen's layout (the same for all empires).
    static constexpr std::uint32_t kTurn = 0x3e9;
    static constexpr std::uint32_t kSummary = 0x3ee;
    static constexpr std::uint32_t kWholeMap = 0x3ea;
    static constexpr std::uint32_t kEconomy = 0x3eb;
    static constexpr std::uint32_t kMilitary = 0x3ec;
    static constexpr std::uint32_t kEvents = 0x3ed;
    static constexpr std::uint32_t kOverlay = 0x403;
    static constexpr std::uint32_t kMap = 0x3f9;
    static constexpr std::uint32_t kInfo = 0x3f6;
    static constexpr std::uint32_t kList = 0x3fc;
    static constexpr std::uint32_t kTitle = 0x439;
    static constexpr std::uint32_t kTurnText = 0x3fd;
    // In the panel below the map.
    static constexpr std::uint32_t kInfoSector = 0x44c;
    static constexpr std::uint32_t kInfoDetails = 0x44f;
    static constexpr std::uint32_t kInfoName = 0x44d;
    static constexpr std::uint32_t kInfoPicture = 0x3fc;
    static constexpr std::uint32_t kSystemDetails = 0x44e;

    GalaxyScreen(Assets& assets, Game& game);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    Sector chosen() const { return chosen_; }
    bool wholeMap() const { return whole_; }
    // Where a sector's middle is on the screen, when it is in view.
    std::optional<Point> sectorCentre(Sector s) const;
    std::optional<Sector> sectorAt(Point p) const;

private:
    int sectorSize() const;
    void choose(Sector s);
    void keepInView();
    std::string sectorName(Sector s) const;
    void drawSector(Canvas& canvas, int at);
    void drawList(Canvas& canvas);
    void drawSystem(Canvas& canvas, const StarSystem& s, std::uint64_t ms);

    Assets& assets_;
    Game& game_;
    char prefix_;
    Panel panel_;
    Panel info_;   // the panel below the map, about the chosen sector
    Panel system_; // the same for a star system, with its planets
    Rect map_;
    bool whole_ = false;
    Sector chosen_;
    Sector origin_; // the sector in the map's top left corner (close view)
    Point pointer_;
    std::vector<std::string> kindNames_;
    std::vector<std::string> animations_; // the names of the game's animations
};

} // namespace nagus
