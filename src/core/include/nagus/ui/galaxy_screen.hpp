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

// Draws a star system's star and its turning planets in a row, with their
// names, into `strip` (as below the galaxy map and the system screen).
void drawPlanetStrip(Canvas& canvas, Assets& assets, const Game& game, const StarSystem& s, const Rect& strip,
                     const Font& small, std::uint64_t ms, const std::vector<std::string>& animations);

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
    // The fleet panel below the map, and its orders menu.
    static constexpr std::uint32_t kFleetCloak = 0x4b9;
    static constexpr std::uint32_t kFleetRedeploy = 0x4ba;
    static constexpr std::uint32_t kFleetOrders = 0x4bb;
    static constexpr std::uint32_t kFleetShips = 0x4bc;
    static constexpr std::uint32_t kFleetText = 0x4c0;
    static constexpr std::uint32_t kOrderColonize = 0x4b2;
    static constexpr std::uint32_t kOrderAttack = 0x4ce;
    static constexpr int kFleetLine = 14; // height of a fleet's line in the list on the left

    GalaxyScreen(Assets& assets, Game& game);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    Sector chosen() const { return chosen_; }
    void choose(Sector s);
    // The player's colony in the chosen sector (an index into Game::colonies), or -1.
    int chosenColony() const;
    // What happened in the last turn, for the player.
    const std::vector<std::pair<std::string, Pixel>>& eventLines() const { return events_; }
    std::vector<std::string> events() const {
        std::vector<std::string> out;
        for (const auto& e : events_) {
            out.push_back(e.first);
        }
        return out;
    }
    bool wholeMap() const { return whole_; }
    // Where a sector's middle is on the screen, when it is in view.
    std::optional<Point> sectorCentre(Sector s) const;
    std::optional<Sector> sectorAt(Point p) const;

    // Fleets: those seen in the chosen sector are listed on the left; a
    // click on one chooses it (the panel below then shows its ships), and
    // for the player's own the next click on the map sends it there.
    std::vector<int> fleetsHere() const;
    std::optional<Rect> fleetLine(int i) const;
    int chosenFleet() const { return fleet_; }
    void chooseFleet(int id);
    bool moving() const { return moving_; }
    bool ordersOpen() const { return ordersOpen_; }
    std::optional<Rect> orderButton(std::uint32_t id) const;
    // Back: stops sending a fleet, or closes the orders, or lets go of the
    // chosen fleet. Returns false when there was nothing to stop.
    bool cancel();

    // Ends the turn (the Turn button). With tactical combat, the turn
    // stops halfway when the player has battles to fight: the outcome is
    // Combat, battlesWaiting() says where, and finishTurn completes it.
    void endPlayerTurn();
    const std::vector<Sector>& battlesWaiting() const { return waiting_; }
    void finishTurn(const std::vector<Battle>& fought);
    // Shows what happened in a turn ended elsewhere (a multiplayer game).
    void report(const TurnSummary& summary);

private:
    TurnSummary pending_;
    std::vector<Sector> waiting_;
    int sectorSize() const;
    void keepInView();
    std::string sectorName(Sector s) const;
    std::string battleText(const Battle& b) const;
    std::string assaultText(const Assault& a) const;
    void drawSector(Canvas& canvas, int at);
    void drawList(Canvas& canvas);
    void drawSystem(Canvas& canvas, const StarSystem& s, std::uint64_t ms);
    void drawFleets(Canvas& canvas);
    void drawFleetPanel(Canvas& canvas);
    void refreshFleet();
    std::string fleetName(const Fleet& f) const;

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
    std::vector<std::pair<std::string, Pixel>> events_; // what happened last turn, and its colour
    Panel fleetPanel_;
    Panel orders_;
    int fleet_ = -1;      // the chosen fleet's id
    bool moving_ = false; // the next click on the map sends the chosen fleet
    bool ordersOpen_ = false;
};

} // namespace nagus
