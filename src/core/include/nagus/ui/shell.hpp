// SPDX-License-Identifier: GPL-3.0-or-later
//
// Runs the screens: which one is shown, where the pointer is, and what
// happens when a screen is done. The app feeds it input and shows what it
// draws.
#pragma once

#include "nagus/game/game.hpp"
#include "nagus/settings.hpp"
#include "nagus/sound/jukebox.hpp"
#include "nagus/ui/diplomacy_screen.hpp"
#include "nagus/ui/ending_screen.hpp"
#include "nagus/ui/fame_screen.hpp"
#include "nagus/ui/film_screen.hpp"
#include "nagus/ui/galaxy_screen.hpp"
#include "nagus/ui/intel_screen.hpp"
#include "nagus/ui/load_save_screen.hpp"
#include "nagus/ui/menus.hpp"
#include "nagus/ui/research_screen.hpp"
#include "nagus/ui/system_screen.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace nagus {

class Shell {
public:
    Shell(std::shared_ptr<Resources> resources, std::filesystem::path settingsFile);

    void draw(Canvas& canvas, std::uint64_t ms);

    void pointerMoved(Point p);
    void pointerPressed(Point p);
    void pointerReleased(Point p);
    void command(Command c);
    // The gamepad's A button: a click where the pointer is.
    void clickAtPointer();

    Point pointer() const { return pointer_; }
    // The original's pointer is drawn while a mouse or gamepad is in use,
    // and hidden for touch.
    void setPointerVisible(bool visible) { pointerVisible_ = visible; }
    bool quit() const { return quit_; }

    enum class Showing { MainMenu, GameSetup, EmpireSelect, Galaxy, System, Research, Diplomacy, Intel, Save, Load, Film, Ending, Fame };
    Showing showing() const { return showing_; }
    bool noticeOpen() const { return !notice_.empty(); }
    // The game being played, if any.
    const Game* game() const { return game_.get(); }
    GalaxyScreen* galaxyScreen() { return galaxy_.get(); }
    SystemScreen* systemScreen() { return system_.get(); }
    ResearchScreen* researchScreen() { return research_.get(); }
    DiplomacyScreen* diplomacyScreen() { return diplomacy_.get(); }
    IntelScreen* intelScreen() { return intel_.get(); }
    LoadSaveScreen* loadSaveScreen() { return loadSave_.get(); }
    FilmScreen* filmScreen() { return film_.get(); }
    EndingScreen* endingScreen() { return ending_.get(); }
    FameScreen* fameScreen() { return fame_.get(); }
    // The hall of fame: next to the settings file.
    std::filesystem::path fameFile() const { return settingsFile_.parent_path() / "fame.txt"; }
    // Turns the films on (the app does; tests leave them off) and plays the
    // opening film.
    void enableFilms();
    // Where saved games are kept: next to the settings file.
    std::filesystem::path savesFolder() const { return settingsFile_.parent_path() / "saves"; }

    // The marker window: in the game, Back (right click, Esc, B) opens it
    // where the pointer is; its buttons go to the game's screens.
    static constexpr std::uint32_t kMarkerGalaxy = 0x1f41;
    static constexpr std::uint32_t kMarkerSystem = 0x1f42;
    static constexpr std::uint32_t kMarkerDiplomacy = 0x1f43;
    static constexpr std::uint32_t kMarkerIntel = 0x1f44;
    static constexpr std::uint32_t kMarkerResearch = 0x1f45;
    static constexpr std::uint32_t kMarkerMenu = 0x1f46;
    bool markerOpen() const { return marker_ != nullptr; }
    // Where a button of the marker window is, while it is open.
    std::optional<Rect> markerButton(std::uint32_t id) const;
    // The music and sounds; the app takes the mixed samples from it.
    Jukebox& jukebox() { return jukebox_; }
    // The music track for what is shown: the main theme in the menus, the
    // player's empire's theme in the game (our choice, see
    // docs/formats/sound.md).
    int wantedTrack() const;
    // The click sound of a button, for the player's empire.
    int clickSound() const;
    // Plays the click for buttons pressed and changes the music to suit
    // what is shown. Called as each frame is drawn.
    void updateSound();

    // The seed for the next new galaxy (random unless set, for tests).
    void setSeed(std::uint32_t seed) { seed_ = seed; }

private:
    void settle();
    void show(Showing s);
    MenuScreen& screen();

    Assets assets_;
    Jukebox jukebox_;
    std::uint64_t clicksHeard_ = 0;
    std::filesystem::path settingsFile_;
    GameSettings settings_;
    Showing showing_ = Showing::MainMenu;
    std::unique_ptr<MainMenu> mainMenu_;
    std::unique_ptr<GameSetup> setup_;
    std::unique_ptr<EmpireSelect> empires_;
    std::unique_ptr<GalaxyScreen> galaxy_; // kept while another screen of the game shows
    std::unique_ptr<SystemScreen> system_;
    std::unique_ptr<ResearchScreen> research_;
    std::unique_ptr<DiplomacyScreen> diplomacy_;
    std::unique_ptr<IntelScreen> intel_;
    std::unique_ptr<LoadSaveScreen> loadSave_;
    std::unique_ptr<Panel> marker_;
    std::unique_ptr<FilmScreen> film_;
    std::unique_ptr<EndingScreen> ending_;
    std::unique_ptr<FameScreen> fame_;
    int fameRow_ = -1;
    // The game has ended: its score goes into the hall of fame, the
    // ending's pictures show (unless retired), then the hall of fame.
    void finishGame();
    Showing afterFilm_ = Showing::MainMenu;
    bool films_ = false;
    // Plays a film, then shows `next`; shows `next` at once if the film
    // cannot be played.
    void playFilm(const std::string& film, const std::string& sound, Showing next);
    void openMarker();
    void markerChose(std::uint32_t id);
    int playerColony() const;
    void saveTo(int slot);
    void loadFrom(int slot);
    std::unique_ptr<Game> game_;
    std::optional<std::uint32_t> seed_;
    std::string notice_;
    Point pointer_{kScreenWidth / 2, kScreenHeight / 2};
    bool pointerVisible_ = true;
    bool quit_ = false;
};

} // namespace nagus
