// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/shell.hpp"

#include "nagus/game/victory.hpp"

#include <algorithm>
#include <ctime>
#include <random>

namespace nagus {

namespace {

constexpr const char* kPointer = "generic.cur";
constexpr const char* kNoticeFont = "human20.fnt";
constexpr std::size_t kTurnText = 1370; // "Turn"

std::string today() {
    const std::time_t now = std::time(nullptr);
    std::tm t{};
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    char text[32];
    std::strftime(text, sizeof text, "%d-%m-%Y %H.%M", &t);
    return text;
}


} // namespace

namespace {
// Tracks of music.snd and sounds of sfx.snd (our reading; see
// docs/formats/sound.md): the main theme, then a theme for each empire;
// a set of five button clicks, one for each empire.
constexpr int kMenuTrack = 0;
constexpr int kEmpireTracks = 1;
constexpr int kClickSounds = 76;
} // namespace

Shell::Shell(std::shared_ptr<Resources> resources, std::filesystem::path settingsFile)
    : assets_(resources), jukebox_(*resources), settingsFile_(std::move(settingsFile)) {
    settings_ = loadSettings(settingsFile_);
    show(Showing::MainMenu);
}

void Shell::show(Showing s) {
    showing_ = s;
    mainMenu_.reset();
    setup_.reset();
    empires_.reset();
    system_.reset();
    research_.reset();
    diplomacy_.reset();
    intel_.reset();
    loadSave_.reset();
    marker_.reset();
    if (s != Showing::Film) {
        film_.reset();
    }
    if (s != Showing::Ending) {
        ending_.reset();
    }
    fame_.reset();
    switch (s) {
    case Showing::Film:
    case Showing::Ending: break;
    case Showing::Fame:
        fame_ = std::make_unique<FameScreen>(assets_, loadFame(fameFile()), fameRow_);
        fameRow_ = -1;
        break;
    case Showing::MainMenu: mainMenu_ = std::make_unique<MainMenu>(assets_, game_ != nullptr); break;
    case Showing::Galaxy:
        if (!galaxy_) {
            galaxy_ = std::make_unique<GalaxyScreen>(assets_, *game_);
        }
        break;
    case Showing::System: system_ = std::make_unique<SystemScreen>(assets_, *game_, playerColony()); break;
    case Showing::Research: research_ = std::make_unique<ResearchScreen>(assets_, *game_); break;
    case Showing::Diplomacy: diplomacy_ = std::make_unique<DiplomacyScreen>(assets_, *game_); break;
    case Showing::Intel: intel_ = std::make_unique<IntelScreen>(assets_, *game_); break;
    case Showing::GameSetup: setup_ = std::make_unique<GameSetup>(assets_, settings_); break;
    case Showing::EmpireSelect: empires_ = std::make_unique<EmpireSelect>(assets_, settings_); break;
    case Showing::Save:
    case Showing::Load: {
        std::array<SaveSlot, kSaveSlots> slots;
        for (int i = 0; i < kSaveSlots; ++i) {
            slots[static_cast<std::size_t>(i)] = readSaveSlot(saveSlotFile(savesFolder(), i));
        }
        loadSave_ = std::make_unique<LoadSaveScreen>(assets_, s == Showing::Save ? LoadSaveScreen::Mode::Save : LoadSaveScreen::Mode::Load, slots);
        break;
    }
    }
    screen().pointerMoved(pointer_);
}

MenuScreen& Shell::screen() {
    switch (showing_) {
    case Showing::GameSetup: return *setup_;
    case Showing::EmpireSelect: return *empires_;
    case Showing::Galaxy: return *galaxy_;
    case Showing::System: return *system_;
    case Showing::Research: return *research_;
    case Showing::Diplomacy: return *diplomacy_;
    case Showing::Intel: return *intel_;
    case Showing::Save:
    case Showing::Load: return *loadSave_;
    case Showing::Film: return *film_;
    case Showing::Ending: return *ending_;
    case Showing::Fame: return *fame_;
    case Showing::MainMenu: break;
    }
    return *mainMenu_;
}

void Shell::finishGame() {
    const Game& g = *game_;
    static const char* const kHow[kEndings] = {"", "Victory by conquest", "Victory by domination", "Victory with allies",
                                              "Defeat", "Retired"};
    FameEntry entry;
    entry.score = score(g);
    const int me = std::clamp(g.player, 0, kEmpires - 1);
    entry.name = static_cast<std::size_t>(me) < g.raceNames.size() ? g.raceNames[static_cast<std::size_t>(me)] : "?";
    entry.description = std::string(kHow[static_cast<int>(g.ending)]) + " on turn " + std::to_string(g.turn - 1);
    auto entries = loadFame(fameFile());
    fameRow_ = addFame(entries, entry);
    saveFame(fameFile(), entries);
    // The ending's pictures and speech, from the original's script.
    std::vector<ScriptStep> script;
    const std::string kind = endingName(g.ending);
    if (!kind.empty()) {
        const std::string name = std::string(1, "chfkr"[me]) + "eog" + kind + ".scr";
        try {
            const Bytes text = assets_.resources().file(name);
            script = readEndingScript(std::string_view(reinterpret_cast<const char*>(text.data()), text.size()));
        } catch (const FormatError&) {
        }
    }
    game_.reset();
    galaxy_.reset();
    if (script.empty()) {
        show(Showing::Fame);
        return;
    }
    ending_ = std::make_unique<EndingScreen>(assets_, jukebox_, std::move(script));
    show(Showing::Ending);
}

void Shell::enableFilms() {
    films_ = true;
    if (showing_ == Showing::MainMenu && !game_) {
        playFilm("open.avi", "intro.wav", Showing::MainMenu);
    }
}

void Shell::playFilm(const std::string& film, const std::string& sound, Showing next) {
    if (films_) {
        try {
            film_ = std::make_unique<FilmScreen>(assets_.resources(), jukebox_, film, sound);
            afterFilm_ = next;
            show(Showing::Film);
            return;
        } catch (const FormatError&) {
            // No film: go straight on.
        }
    }
    show(next);
}

void Shell::settle() {
    const Outcome o = screen().takeOutcome();
    switch (showing_) {
    case Showing::Film:
        if (o != Outcome::None) {
            show(afterFilm_);
        }
        break;
    case Showing::Ending:
        if (o != Outcome::None) {
            show(Showing::Fame);
        }
        break;
    case Showing::Fame:
        if (o == Outcome::Back) {
            show(Showing::MainMenu);
        }
        break;
    case Showing::MainMenu:
        if (o == Outcome::NewGame) {
            show(Showing::GameSetup);
        } else if (o == Outcome::Continue && game_) {
            show(Showing::Galaxy);
        } else if (o == Outcome::Save && game_) {
            show(Showing::Save);
        } else if (o == Outcome::Load) {
            show(Showing::Load);
        } else if (o == Outcome::Quit) {
            quit_ = true;
        } else if (o == Outcome::HallOfFame) {
            show(Showing::Fame);
        } else if (o == Outcome::Retire && game_) {
            game_->ending = Ending::Retired;
            finishGame();
        }
        break;
    case Showing::Save:
    case Showing::Load:
        if (o == Outcome::Accept) {
            const int slot = loadSave_->chosen();
            if (showing_ == Showing::Save) {
                saveTo(slot);
            } else {
                loadFrom(slot);
            }
        } else if (o == Outcome::Back) {
            show(Showing::MainMenu);
        }
        break;
    case Showing::GameSetup:
        if (o == Outcome::Accept) {
            settings_ = setup_->settings();
            saveSettings(settingsFile_, settings_);
            show(Showing::EmpireSelect);
        } else if (o == Outcome::Back) {
            show(Showing::MainMenu);
        }
        break;
    case Showing::EmpireSelect:
        if (o == Outcome::Accept) {
            settings_ = empires_->settings();
            saveSettings(settingsFile_, settings_);
            const std::uint32_t seed = seed_ ? *seed_ : std::random_device{}();
            galaxy_.reset();
            game_ = std::make_unique<Game>(newGame(settings_, loadGalaxyData(assets_.resources(), assets_.lexicon()), loadEconomyData(assets_.resources()), seed));
            // The chosen empire's film.
            playFilm(std::string(1, "chfkr"[std::clamp(game_->player, 0, kEmpires - 1)]) + ".avi", "", Showing::Galaxy);
        } else if (o == Outcome::Back) {
            show(Showing::GameSetup);
        }
        break;
    case Showing::Galaxy:
        if (o == Outcome::Open && galaxy_->chosenColony() >= 0) {
            show(Showing::System);
        }
        break;
    case Showing::System:
    case Showing::Research:
    case Showing::Diplomacy:
    case Showing::Intel:
        if (o == Outcome::Back) {
            openMarker();
        }
        break;
    }
}

int Shell::wantedTrack() const {
    if (showing_ == Showing::Film || showing_ == Showing::Ending) {
        return -1;
    }
    const bool inGame = game_ && showing_ != Showing::MainMenu && showing_ != Showing::GameSetup &&
                        showing_ != Showing::EmpireSelect && showing_ != Showing::Save && showing_ != Showing::Load;
    return inGame ? kEmpireTracks + std::clamp(game_->player, 0, kEmpires - 1) : kMenuTrack;
}

int Shell::clickSound() const {
    const int empire = game_ ? game_->player : settings_.empire;
    return kClickSounds + std::clamp(empire, 0, kEmpires - 1);
}

void Shell::updateSound() {
    if (assets_.clicks() != clicksHeard_) {
        clicksHeard_ = assets_.clicks();
        jukebox_.playEffect(clickSound());
    }
    jukebox_.playMusic(wantedTrack());
}

void Shell::draw(Canvas& canvas, std::uint64_t ms) {
    // A game that has ended goes to its end.
    if (game_ && game_->ending != Ending::None && marker_ == nullptr &&
        (showing_ == Showing::Galaxy || showing_ == Showing::System || showing_ == Showing::Research ||
         showing_ == Showing::Diplomacy || showing_ == Showing::Intel)) {
        finishGame();
    }
    updateSound();
    canvas.clear();
    screen().draw(canvas, ms);
    if ((showing_ == Showing::Film && film_->done()) || (showing_ == Showing::Ending && ending_->done())) {
        settle();
        return draw(canvas, ms);
    }
    if (!notice_.empty()) {
        canvas.blend({0, 0, canvas.width(), canvas.height()}, rgb(0, 0, 0), 160);
        const Rect box{150, 220, 500, 160};
        canvas.fill(box, rgb(0, 0, 0));
        canvas.frame(box, rgb(255, 153, 0), 3);
        drawParagraph(canvas, assets_.font(kNoticeFont), notice_, {box.x + 20, box.y + 20, box.w - 40, box.h - 40},
                      rgb(255, 204, 102));
    }
    if (marker_) {
        marker_->draw(canvas);
    }
    if (pointerVisible_ && showing_ != Showing::Film && showing_ != Showing::Ending) {
        const Cursor& c = assets_.cursor(kPointer);
        canvas.draw(c.frame(ms), pointer_.x - c.hotX, pointer_.y - c.hotY);
    }
}

int Shell::playerColony() const {
    if (galaxy_ && galaxy_->chosenColony() >= 0) {
        return galaxy_->chosenColony();
    }
    // Else the player's home system, or their first colony.
    int first = -1;
    for (std::size_t i = 0; game_ && i < game_->colonies.size(); ++i) {
        const Colony& c = game_->colonies[i];
        if (c.owner == game_->player) {
            if (c.home) {
                return static_cast<int>(i);
            }
            first = first < 0 ? static_cast<int>(i) : first;
        }
    }
    return first;
}

void Shell::saveTo(int slot) {
    const int player = game_->player;
    SaveInfo info;
    info.name = player >= 0 && static_cast<std::size_t>(player) < game_->raceNames.size()
                    ? game_->raceNames[static_cast<std::size_t>(player)]
                    : std::string();
    info.turn = game_->turn;
    info.description = assets_.lexicon()[kTurnText] + " " + std::to_string(game_->turn) + ", " + today();
    if (writeSaveFile(saveSlotFile(savesFolder(), slot), saveGame(*game_, info))) {
        show(Showing::Galaxy);
    } else {
        notice_ = "The game could not be saved.";
    }
}

void Shell::loadFrom(int slot) {
    try {
        Game loaded = loadGame(readSaveFile(saveSlotFile(savesFolder(), slot)), loadEconomyData(assets_.resources()));
        galaxy_.reset();
        game_ = std::make_unique<Game>(std::move(loaded));
        show(Showing::Galaxy);
    } catch (const std::exception&) {
        notice_ = "This saved game cannot be read. It may be damaged, or from another version of Nagus.";
    }
}

void Shell::openMarker() {
    marker_ = std::make_unique<Panel>(assets_, std::string(1, "chfkr"[std::clamp(game_->player, 0, kEmpires - 1)]) + "marker.wdf");
    const Rect r = marker_->layout().rect;
    int w = 0;
    int h = 0;
    for (const auto& e : marker_->layout().elements) {
        w = std::max(w, e.rect.x + e.rect.w);
        h = std::max(h, e.rect.y + e.rect.h);
    }
    w = std::max(w, r.w);
    h = std::max(h, r.h);
    // Centred on the pointer, inside the screen.
    marker_->setOrigin({std::clamp(pointer_.x - w / 2, 0, kScreenWidth - w), std::clamp(pointer_.y - h / 2, 0, kScreenHeight - h)});
    marker_->setEnabled(kMarkerSystem, playerColony() >= 0);
    marker_->pointerMoved(pointer_);
}

std::optional<Rect> Shell::markerButton(std::uint32_t id) const {
    if (!marker_ || marker_->layout().find(id) == nullptr) {
        return std::nullopt;
    }
    return marker_->rect(id);
}

void Shell::markerChose(std::uint32_t id) {
    switch (id) {
    case kMarkerGalaxy: show(Showing::Galaxy); break;
    case kMarkerSystem: show(Showing::System); break;
    case kMarkerResearch: show(Showing::Research); break;
    case kMarkerDiplomacy: show(Showing::Diplomacy); break;
    case kMarkerIntel: show(Showing::Intel); break;
    case kMarkerMenu: show(Showing::MainMenu); break;
    default: break;
    }
}

void Shell::pointerMoved(Point p) {
    pointer_ = p;
    if (marker_) {
        marker_->pointerMoved(p);
        return;
    }
    if (notice_.empty()) {
        screen().pointerMoved(p);
    }
}

void Shell::pointerPressed(Point p) {
    pointer_ = p;
    if (marker_) {
        marker_->pointerPressed(p);
        return;
    }
    if (notice_.empty()) {
        screen().pointerPressed(p);
    }
}

void Shell::pointerReleased(Point p) {
    pointer_ = p;
    if (!notice_.empty()) {
        notice_.clear();
        show(Showing::MainMenu);
        return;
    }
    if (marker_) {
        const auto id = marker_->pointerReleased(p);
        if (id) {
            markerChose(*id);
        } else if (!marker_->layout().elements.empty()) {
            // A click beside the window closes it.
            bool inside = false;
            for (const auto& e : marker_->layout().elements) {
                inside = inside || marker_->rect(e.id).contains(p.x, p.y);
            }
            if (!inside) {
                marker_.reset();
            }
        }
        return;
    }
    screen().pointerReleased(p);
    settle();
}

void Shell::clickAtPointer() {
    pointerPressed(pointer_);
    pointerReleased(pointer_);
}

void Shell::command(Command c) {
    if (!notice_.empty()) {
        notice_.clear();
        show(Showing::MainMenu);
        return;
    }
    if (marker_) {
        if (c == Command::Back) {
            marker_.reset();
        } else if (const auto jump = marker_->step(c, pointer_)) {
            pointer_ = *jump;
            marker_->pointerMoved(pointer_);
        }
        return;
    }
    if (showing_ == Showing::Galaxy && c == Command::Back) {
        if (!galaxy_->cancel()) {
            openMarker();
        }
        return;
    }
    if (const auto jump = screen().command(c)) {
        pointer_ = *jump;
        screen().pointerMoved(pointer_);
    }
    settle();
}

} // namespace nagus
