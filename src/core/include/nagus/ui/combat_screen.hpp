// SPDX-License-Identifier: GPL-3.0-or-later
//
// The tactical combat screen: the player chooses to fight, to let the
// battle be fought automatically or to hail, then gives their ships a
// maneuver each round. The battle shows in 3D with the original's models
// (Visual), or as the sides facing each other from above (Tactical).
#pragma once

#include "nagus/data/model.hpp"
#include "nagus/game/tactical.hpp"
#include "nagus/render/scene.hpp"
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
    static constexpr std::uint32_t kUtilArea = 0x1777;
    static constexpr std::uint32_t kViewsArea = 0x177a;
    // <x>fiskdlg.wdf: how to meet the enemy, asked first.
    static constexpr std::uint32_t kManual = 0x26b5;    // Fight
    static constexpr std::uint32_t kAutomatic = 0x26b6; // Skip: fought automatically
    static constexpr std::uint32_t kHail = 0x26b7;
    static constexpr std::uint32_t kQuestionText = 0x2649;
    // <x>butil.wdf: the camera, on the left in the 3D view.
    static constexpr std::uint32_t kCameraUp = 0x17d4;
    static constexpr std::uint32_t kCameraLeft = 0x17d5;
    static constexpr std::uint32_t kCameraDown = 0x17d6;
    static constexpr std::uint32_t kCameraRight = 0x17d7;
    static constexpr std::uint32_t kZoomIn = 0x17d8;
    static constexpr std::uint32_t kZoomOut = 0x17d9;
    static constexpr std::uint32_t kPanorama = 0x17da;
    static constexpr std::uint32_t kViews = 0x17db;
    static constexpr std::uint32_t kStatus = 0x17dc;
    static constexpr std::uint32_t kAuto = 0x17dd;
    // <x>b_views.wdf: where the camera is.
    static constexpr std::uint32_t kTopView = 0x1af4;
    static constexpr std::uint32_t kChaseView = 0x1af5;
    static constexpr std::uint32_t kFrontView = 0x1af6;
    static constexpr std::uint32_t kEnemyView = 0x1af7;
    static constexpr std::uint32_t kShipView = 0x1af8;
    // Where the battle is shown.
    static constexpr Rect kView{185, 6, 611, 489};

    enum class Phase { Choose, Orders, Over };
    enum class View { Visual, Tactical };
    // Where the camera of the 3D view is: swinging above the battle, high
    // above it, behind one of the player's ships, before the player's
    // ships, behind the enemy's, or on a ship looking at its target.
    enum class CameraView { Panorama, Top, Chase, Front, Enemy, Ship };

    CombatScreen(Assets& assets, const Game& game, Sector at);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    Phase phase() const { return phase_; }
    const TacticalBattle& battle() const { return battle_; }
    Maneuver chosen() const { return chosen_; }
    View view() const { return view_; }
    CameraView cameraView() const { return cameraView_; }
    // The camera's turn and height (radians) and its distance (1 normal).
    float cameraTurn() const { return turn_; }
    float cameraTilt() const { return tilt_; }
    float cameraZoom() const { return zoom_; }
    bool viewsOpen() const { return viewsOpen_; }
    bool statusShown() const { return hud_; }
    // Whether a unit shows as a 3D model (the game has it).
    bool hasModel(std::size_t unit);
    void choose(Maneuver m) { chosen_ = m; }
    // Where things are, for the pointer and tests.
    Rect maneuverRect(Maneuver m) const;
    std::optional<Rect> button(std::uint32_t id) const;
    // A unit's place in the view (in the Visual view, where it showed last).
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
    // The unit's model, or nothing when the game lacks it.
    const Model* model(std::size_t unit);
    void drawTactical(Canvas& canvas, std::uint64_t ms, const Font& small);
    void drawVisual(Canvas& canvas, std::uint64_t ms, const Font& small);
    void drawLabel(Canvas& canvas, std::size_t unit, const Rect& r, std::uint64_t ms, const Font& small);
    Camera camera(std::uint64_t ms) const;
    // Handles a button of the camera panels; false when it is none of theirs.
    bool cameraButton(std::uint32_t id);
    void fightToEnd(bool hail);

    Assets& assets_;
    const Game& game_;
    TacticalBattle battle_;
    Panel panel_;
    Panel question_;
    Panel util_;
    Panel views_;
    CameraView cameraView_ = CameraView::Panorama;
    float turn_ = 0;
    float tilt_ = 0;
    float zoom_ = 1;
    bool viewsOpen_ = false;
    bool hud_ = true;
    std::optional<Point> drag_;
    Phase phase_ = Phase::Choose;
    Maneuver chosen_ = Maneuver::Assault;
    View view_ = View::Visual;
    std::vector<Rect> places_; // for each unit, in the Tactical view
    std::vector<Vec3> spots_;  // for each unit, in the Visual view
    std::vector<Rect> shown_;  // where each showed last in the Visual view
    bool newShots_ = false;
    std::uint64_t shotsAt_ = 0;
    Point pointer_{};
    std::map<std::string, Image> mirrored_;
};

} // namespace nagus
