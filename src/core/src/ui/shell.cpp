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
    jukebox_.setVolumes(settings_.heard(0), settings_.heard(1), settings_.heard(2));
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
    options_.reset();
    if (s != Showing::Combat) {
        combat_.reset();
    }
    switch (s) {
    case Showing::Options: options_ = std::make_unique<OptionsScreen>(assets_, jukebox_, settings_); break;
    case Showing::Film:
    case Showing::Ending:
    case Showing::Combat: break;
    case Showing::Fame:
        fame_ = std::make_unique<FameScreen>(assets_, loadFame(fameFile()), fameRow_);
        fameRow_ = -1;
        break;
    case Showing::MainMenu: {
        std::error_code ec;
        mainMenu_ = std::make_unique<MainMenu>(assets_, game_ != nullptr, std::filesystem::exists(autosaveFile(), ec),
                                               game_ != nullptr && multiplayer());
        break;
    }
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
    case Showing::EmpireSelect:
        empires_ = std::make_unique<EmpireSelect>(assets_, settings_);
        if (mpStep_ == MultiplayerStep::Empire) {
            // The empires the other players have cannot be chosen.
            std::array<bool, kEmpires> taken{};
            const Lobby& lobby = host_ ? host_->lobby() : client_->lobby();
            const int me = host_ ? 0 : client_->place();
            for (std::size_t i = 0; i < lobby.players.size(); ++i) {
                const int e = lobby.players[i].empire;
                if (static_cast<int>(i) != me && e >= 0 && e < kEmpires) {
                    taken[static_cast<std::size_t>(e)] = true;
                }
            }
            empires_->setTaken(taken);
        }
        break;
    case Showing::Multiplayer:
        if (!mp_) {
            mp_ = std::make_unique<MultiplayerScreen>(assets_, settings_, empireNames());
        }
        break;
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
    assets_.clearHover();
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
    case Showing::Options: return *options_;
    case Showing::Combat: return *combat_;
    case Showing::Multiplayer: return *mp_;
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
    host_.reset();
    client_.reset();
    waitingTurn_ = false;
    // A game that has ended cannot be continued.
    std::error_code gone;
    std::filesystem::remove(autosaveFile(), gone);
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
    case Showing::Options:
        if (o == Outcome::Accept) {
            settings_ = options_->settings();
            saveSettings(settingsFile_, settings_);
            // The game being played follows the switches too.
            if (game_) {
                game_->settings.automaticCombat = settings_.automaticCombat;
                game_->settings.randomEvents = settings_.randomEvents;
            }
            show(Showing::MainMenu);
        } else if (o == Outcome::Back) {
            show(Showing::MainMenu);
        }
        break;
    case Showing::MainMenu:
        if (o == Outcome::NewGame) {
            show(Showing::GameSetup);
        } else if (o == Outcome::Multiplayer) {
            if (!(game_ && multiplayer())) {
                leaveMultiplayer();
            }
            show(Showing::Multiplayer);
        } else if (o == Outcome::Continue && game_) {
            show(Showing::Galaxy);
        } else if (o == Outcome::Continue) {
            loadFrom(-1);
        } else if (o == Outcome::Save && game_) {
            show(Showing::Save);
        } else if (o == Outcome::Load) {
            show(Showing::Load);
        } else if (o == Outcome::Quit) {
            quit_ = true;
        } else if (o == Outcome::Options) {
            show(Showing::Options);
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
        if (mpStep_ == MultiplayerStep::Settings) {
            // The host's game set-up: then to the lobby.
            if (o != Outcome::Accept && o != Outcome::Back) {
                break;
            }
            mpStep_ = MultiplayerStep::None;
            if (o == Outcome::Accept) {
                settings_ = setup_->settings();
                saveSettings(settingsFile_, settings_);
                host_->setSettings(settings_);
            } else {
                host_.reset();
            }
            show(Showing::Multiplayer);
            break;
        }
        if (o == Outcome::Accept) {
            settings_ = setup_->settings();
            saveSettings(settingsFile_, settings_);
            show(Showing::EmpireSelect);
        } else if (o == Outcome::Back) {
            show(Showing::MainMenu);
        }
        break;
    case Showing::EmpireSelect:
        if (mpStep_ == MultiplayerStep::Empire) {
            if (o != Outcome::Accept && o != Outcome::Back) {
                break;
            }
            mpStep_ = MultiplayerStep::None;
            if (o == Outcome::Accept) {
                const int e = empires_->settings().empire;
                if (host_) {
                    host_->chooseEmpire(e);
                } else if (client_) {
                    client_->chooseEmpire(e);
                }
            }
            show(Showing::Multiplayer);
            break;
        }
        if (o == Outcome::Accept) {
            leaveMultiplayer();
            settings_ = empires_->settings();
            saveSettings(settingsFile_, settings_);
            const std::uint32_t seed = seed_ ? *seed_ : std::random_device{}();
            galaxy_.reset();
            game_ = std::make_unique<Game>(newGame(settings_, loadGalaxyData(assets_.resources(), assets_.lexicon()), loadEconomyData(assets_.resources()), seed));
            autosavedTurn_ = game_->turn;
            // The chosen empire's film.
            playFilm(std::string(1, "chfkr"[std::clamp(game_->player, 0, kEmpires - 1)]) + ".avi", "", Showing::Galaxy);
        } else if (o == Outcome::Back) {
            show(Showing::GameSetup);
        }
        break;
    case Showing::Galaxy:
        if (o == Outcome::Open && galaxy_->chosenColony() >= 0) {
            show(Showing::System);
        } else if (o == Outcome::Combat) {
            fought_.clear();
            nextBattle_ = 0;
            nextCombat();
        } else if (o == Outcome::EndTurn && multiplayer()) {
            if (host_) {
                host_->submit(*game_);
            } else {
                client_->submit(*game_);
            }
            waitingTurn_ = true;
        }
        break;
    case Showing::Multiplayer:
        if (o == Outcome::Back) {
            leaveMultiplayer();
            show(Showing::MainMenu);
        } else if (o == Outcome::Host) {
            rememberNames();
            host_ = HostSession::open(mp_->gameName(), mp_->password(), mp_->playerName(), loadEconomyData(assets_.resources()), gamePort_);
            if (!host_) {
                mp_->setStatus("This computer cannot host a game: port " + std::to_string(gamePort_) + " is in use.");
            } else {
                hostAddress_ = localAddress();
                if (host_->port() != kGamePort && !hostAddress_.empty()) {
                    hostAddress_ += ":" + std::to_string(host_->port());
                }
                mpStep_ = MultiplayerStep::Settings;
                show(Showing::GameSetup);
            }
        } else if (o == Outcome::Join) {
            rememberNames();
            client_ = ClientSession::connect(mp_->address(), mp_->playerName(), mp_->password(), loadEconomyData(assets_.resources()));
        } else if (o == Outcome::ChooseEmpire) {
            mpStep_ = MultiplayerStep::Empire;
            show(Showing::EmpireSelect);
        } else if (o == Outcome::Start && host_) {
            if (host_->canStart()) {
                startMultiplayerGame();
            } else {
                mp_->setStatus(host_->lobby().players.size() < 2 ? assets_.lexicon()[441] : "Every player needs an empire");
            }
        }
        break;
    case Showing::Combat:
        if (o == Outcome::Accept) {
            fought_.push_back(combat_->battle().finish(*game_));
            nextBattle_ += 1;
            nextCombat();
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

std::array<std::string, kEmpires> Shell::empireNames() {
    std::array<std::string, kEmpires> names;
    try {
        const GalaxyData data = loadGalaxyData(assets_.resources(), assets_.lexicon());
        for (std::size_t e = 0; e < names.size() && e < data.races.size(); ++e) {
            names[e] = data.races[e].name;
        }
    } catch (const std::exception&) {
        // Names stay empty.
    }
    return names;
}

void Shell::rememberNames() {
    settings_.playerName = mp_->playerName();
    settings_.gameName = mp_->gameName();
    settings_.hostAddress = mp_->address();
    saveSettings(settingsFile_, settings_);
}

void Shell::leaveMultiplayer() {
    host_.reset();
    client_.reset();
    mp_.reset();
    mpStep_ = MultiplayerStep::None;
    waitingTurn_ = false;
    if (game_ && game_->multiplayer()) {
        game_.reset();
        galaxy_.reset();
    }
}

void Shell::startMultiplayerGame() {
    GameSettings s = host_->lobby().settings;
    s.automaticCombat = true; // nobody waits for another's battle
    s.empire = host_->lobby().players.front().empire;
    const std::uint32_t seed = seed_ ? *seed_ : std::random_device{}();
    Game g = newGame(s, loadGalaxyData(assets_.resources(), assets_.lexicon()), loadEconomyData(assets_.resources()), seed);
    for (const auto& p : host_->lobby().players) {
        if (p.empire >= 0 && p.empire < kEmpires) {
            g.humans[static_cast<std::size_t>(p.empire)] = true;
        }
    }
    host_->start(g);
    galaxy_.reset();
    game_ = std::make_unique<Game>(std::move(g));
    mp_.reset();
    waitingTurn_ = false;
    playFilm(std::string(1, "chfkr"[std::clamp(game_->player, 0, kEmpires - 1)]) + ".avi", "", Showing::Galaxy);
}

void Shell::pollNetwork() {
    if (host_) {
        host_->poll();
        for (const auto& n : host_->takeNotices()) {
            toast_ = n;
            toastUntil_ = now_ + 6000;
        }
        if (mp_ && showing_ == Showing::Multiplayer) {
            mp_->showLobby(host_->lobby(), 0, true, hostAddress_);
        }
        if (waitingTurn_ && game_) {
            if (auto t = host_->finishTurn()) {
                *game_ = std::move(t->game);
                waitingTurn_ = false;
                if (galaxy_) {
                    galaxy_->report(t->summary);
                }
            }
        }
    }
    if (client_) {
        client_->poll();
        switch (client_->state()) {
        case ClientSession::State::Refused:
            if (mp_) {
                mp_->setStatus(assets_.lexicon()[client_->refusedText()]);
            }
            client_.reset();
            break;
        case ClientSession::State::Lost:
            if (game_ && game_->multiplayer()) {
                // The game cannot go on without the host.
                client_.reset();
                leaveMultiplayer();
                show(Showing::MainMenu);
                notice_ = assets_.lexicon()[439];
            } else {
                if (mp_) {
                    mp_->setStatus(assets_.lexicon()[182] + ": " + client_->error());
                }
                client_.reset();
            }
            break;
        case ClientSession::State::Lobby:
            if (mp_ && showing_ == Showing::Multiplayer) {
                mp_->showLobby(client_->lobby(), client_->place(), false, "");
            }
            break;
        case ClientSession::State::Playing:
            if (auto g = client_->takeStart()) {
                galaxy_.reset();
                game_ = std::make_unique<Game>(std::move(*g));
                mp_.reset();
                waitingTurn_ = false;
                playFilm(std::string(1, "chfkr"[std::clamp(game_->player, 0, kEmpires - 1)]) + ".avi", "", Showing::Galaxy);
            } else if (auto t = client_->takeTurn(); t && game_) {
                *game_ = std::move(t->game);
                waitingTurn_ = false;
                if (galaxy_) {
                    galaxy_->report(t->summary);
                }
            }
            break;
        case ClientSession::State::Connecting: break;
        }
    }
}

void Shell::typed(std::string_view text) {
    if (marker_ == nullptr && notice_.empty()) {
        screen().typed(text);
    }
}

bool Shell::wantsText() const {
    return marker_ == nullptr && notice_.empty() && const_cast<Shell*>(this)->screen().wantsText();
}

void Shell::nextCombat() {
    const auto& waiting = galaxy_->battlesWaiting();
    for (; nextBattle_ < waiting.size(); ++nextBattle_) {
        // An earlier battle may have ended this one (ships that retreated).
        auto screen = std::make_unique<CombatScreen>(assets_, *game_, waiting[nextBattle_]);
        if (!screen->battle().over()) {
            combat_ = std::move(screen);
            show(Showing::Combat);
            return;
        }
    }
    galaxy_->finishTurn(fought_);
    fought_.clear();
    show(Showing::Galaxy);
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
    now_ = ms;
    pollNetwork();
    // Saved at the end of each turn, once the turn's report shows.
    if (game_ && !multiplayer() && settings_.autosave && showing_ == Showing::Galaxy && game_->ending == Ending::None &&
        game_->turn != autosavedTurn_) {
        autosave();
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
    if (waitingTurn_ && game_) {
        // Whose orders the turn waits for.
        std::vector<int> waiting = host_ ? host_->waitingFor() : client_ ? client_->waitingFor() : std::vector<int>{};
        std::string names;
        for (const int e : waiting) {
            if (e != game_->player && static_cast<std::size_t>(e) < game_->raceNames.size()) {
                names += (names.empty() ? "" : ", ") + game_->raceNames[static_cast<std::size_t>(e)];
            }
        }
        std::string line = assets_.lexicon()[1419];
        if (const auto cut = line.find(':'); cut != std::string::npos) {
            line = line.substr(0, cut + 1);
        }
        const Rect box{200, 250, 400, 70};
        canvas.fill(box, rgb(0, 0, 0));
        canvas.frame(box, rgb(255, 153, 0), 2);
        drawParagraph(canvas, assets_.font(kNoticeFont), names.empty() ? assets_.lexicon()[1371] : line + " " + names,
                      {box.x + 12, box.y + 10, box.w - 24, box.h - 20}, rgb(255, 204, 102));
    }
    if (!toast_.empty() && ms < toastUntil_) {
        const Rect box{250, 8, 300, 26};
        canvas.fill(box, rgb(0, 0, 0));
        canvas.frame(box, rgb(255, 153, 0), 1);
        canvas.text(assets_.font(kNoticeFont), toast_, box, rgb(255, 204, 102), Align::Center);
    }
    if (marker_) {
        marker_->draw(canvas);
    }
    drawTooltip(canvas, ms);
    if (pointerVisible_ && showing_ != Showing::Film && showing_ != Showing::Ending) {
        const Cursor& c = assets_.cursor(kPointer);
        canvas.draw(c.frame(ms), pointer_.x - c.hotX, pointer_.y - c.hotY);
    }
}

void Shell::drawTooltip(Canvas& canvas, std::uint64_t ms) {
    constexpr std::uint64_t kDelay = 700;
    const auto button = assets_.hovered();
    if (button != tipButton_) {
        tipButton_ = button;
        tipSince_ = ms;
        tipHidden_ = false;
    }
    tipShown_.reset();
    if (!settings_.tooltips || !pointerVisible_ || !button || tipHidden_ || ms - tipSince_ < kDelay ||
        !notice_.empty() || blocked() || showing_ == Showing::Film || showing_ == Showing::Ending) {
        return;
    }
    const auto& lines = assets_.tips()[*button];
    if (lines.empty()) {
        return;
    }
    const Font& font = assets_.font("human15.fnt");
    const int lineHeight = font.height() + 1;
    int w = 0;
    for (const auto& l : lines) {
        w = std::max(w, font.width(l));
    }
    w += 12;
    const int h = static_cast<int>(lines.size()) * lineHeight + 8;
    // Below and right of the pointer, kept on the screen.
    int x = pointer_.x + 12;
    int y = pointer_.y + 20;
    if (x + w > kScreenWidth) {
        x = std::max(0, pointer_.x - w - 4);
    }
    if (y + h > kScreenHeight) {
        y = std::max(0, pointer_.y - h - 4);
    }
    const Rect box{x, y, w, h};
    tipShown_ = button;
    canvas.fill(box, rgb(0, 0, 0));
    canvas.frame(box, rgb(255, 153, 0), 1);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        canvas.text(font, lines[i], x + 6, y + 4 + static_cast<int>(i) * lineHeight, rgb(255, 204, 102));
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

void Shell::autosave() {
    const int player = game_->player;
    SaveInfo info;
    info.name = player >= 0 && static_cast<std::size_t>(player) < game_->raceNames.size()
                    ? game_->raceNames[static_cast<std::size_t>(player)]
                    : std::string();
    info.turn = game_->turn;
    info.description = assets_.lexicon()[kTurnText] + " " + std::to_string(game_->turn) + ", " + today();
    (void)writeSaveFile(autosaveFile(), saveGame(*game_, info));
    autosavedTurn_ = game_->turn;
}

void Shell::loadFrom(int slot) {
    leaveMultiplayer();
    try {
        // Slot -1: the game saved at the end of the last turn.
        const auto file = slot < 0 ? autosaveFile() : saveSlotFile(savesFolder(), slot);
        Game loaded = loadGame(readSaveFile(file), loadEconomyData(assets_.resources()));
        galaxy_.reset();
        game_ = std::make_unique<Game>(std::move(loaded));
        autosavedTurn_ = game_->turn;
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
    assets_.clearHover();
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
    // The panels under the pointer say which button it is on.
    assets_.clearHover();
    if (marker_) {
        marker_->pointerMoved(p);
        return;
    }
    if (notice_.empty()) {
        screen().pointerMoved(p);
    }
}

bool Shell::blocked() const {
    // While the other players give their orders, the game waits.
    return waitingTurn_ && (showing_ == Showing::Galaxy || showing_ == Showing::System || showing_ == Showing::Research ||
                            showing_ == Showing::Diplomacy || showing_ == Showing::Intel);
}

void Shell::pointerPressed(Point p) {
    pointer_ = p;
    tipHidden_ = true;
    if (blocked()) {
        return;
    }
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
    if (blocked()) {
        return;
    }
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
    tipHidden_ = true;
    if (blocked()) {
        return;
    }
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
            assets_.clearHover();
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
        assets_.clearHover();
        screen().pointerMoved(pointer_);
    }
    settle();
}

} // namespace nagus
