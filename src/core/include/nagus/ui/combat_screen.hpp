// SPDX-License-Identifier: GPL-3.0-or-later
//
// The tactical combat screen: the player chooses to let the battle be
// fought automatically or to give orders, then gives their ships a
// maneuver each round. The original shows the battle in 3D; we show the
// sides facing each other from above, with their weapons' fire.
#pragma once

#include "nagus/game/tactical.hpp"
#include "nagus/ui/menus.hpp"

#include <map>
#include <string>

namespace nagus {

class CombatScreen : public MenuScreen {
public:
    // <x>tactscn.wdf
    static constexpr std::uint32_t kTurn = 0x1770;
    static constexpr std::uint32_t kOrders = 0x1771;
    static constexpr std::uint32_t kFinish = 0x1772;
    static constexpr std::uint32_t kOptions = 0x1773;
    static constexpr std::uint32_t kVisual = 0x1774;
    static constexpr std::uint32_t kTactical = 0x1775;
    static constexpr std::uint32_t kText = 0x1779;
    // tactical.wdf: how to fight, asked first.
    static constexpr std::uint32_t kAutomatic = 0x1fd9;
    static constexpr std::uint32_t kManual = 0x1fda;
    // Where the battle is shown.
    static constexpr Rect kView{185, 6, 611, 489};

    enum class Phase { Choose, Orders, Over };

    CombatScreen(Assets& assets, const Game& game, Sector at);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    Phase phase() const { return phase_; }
    const TacticalBattle& battle() const { return battle_; }
    Maneuver chosen() const { return chosen_; }
    void choose(Maneuver m) { chosen_ = m; }
    // Where things are, for the pointer and tests.
    Rect maneuverRect(Maneuver m) const;
    std::optional<Rect> button(std::uint32_t id) const;
    // A unit's place in the view.
    Rect unitRect(std::size_t unit) const;
    // The line shown when the battle is over.
    std::string resultText() const;
    // A maneuver's name as shown.
    std::string maneuverText(Maneuver m) const;

private:
    void fightRound();
    void refresh();
    Point buttonCentre(std::uint32_t id) const;
    const Image& mirrored(const std::string& name);

    Assets& assets_;
    const Game& game_;
    TacticalBattle battle_;
    Panel panel_;
    Panel question_;
    Phase phase_ = Phase::Choose;
    Maneuver chosen_ = Maneuver::Assault;
    std::vector<Rect> places_; // for each unit
    bool newShots_ = false;
    std::uint64_t shotsAt_ = 0;
    Point pointer_{};
    std::map<std::string, Image> mirrored_;
};

} // namespace nagus
