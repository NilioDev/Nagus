// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/shell.hpp"

#include <algorithm>
#include <random>

namespace nagus {

namespace {

constexpr const char* kPointer = "generic.cur";
constexpr const char* kNoticeFont = "human20.fnt";


} // namespace

Shell::Shell(std::shared_ptr<Resources> resources, std::filesystem::path settingsFile)
    : assets_(std::move(resources)), settingsFile_(std::move(settingsFile)) {
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
    marker_.reset();
    switch (s) {
    case Showing::MainMenu: mainMenu_ = std::make_unique<MainMenu>(assets_, game_ != nullptr); break;
    case Showing::Galaxy:
        if (!galaxy_) {
            galaxy_ = std::make_unique<GalaxyScreen>(assets_, *game_);
        }
        break;
    case Showing::System: system_ = std::make_unique<SystemScreen>(assets_, *game_, playerColony()); break;
    case Showing::Research: research_ = std::make_unique<ResearchScreen>(assets_, *game_); break;
    case Showing::GameSetup: setup_ = std::make_unique<GameSetup>(assets_, settings_); break;
    case Showing::EmpireSelect: empires_ = std::make_unique<EmpireSelect>(assets_, settings_); break;
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
    case Showing::MainMenu: break;
    }
    return *mainMenu_;
}

void Shell::settle() {
    const Outcome o = screen().takeOutcome();
    switch (showing_) {
    case Showing::MainMenu:
        if (o == Outcome::NewGame) {
            show(Showing::GameSetup);
        } else if (o == Outcome::Continue && game_) {
            show(Showing::Galaxy);
        } else if (o == Outcome::Quit) {
            quit_ = true;
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
            show(Showing::Galaxy);
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
        if (o == Outcome::Back) {
            openMarker();
        }
        break;
    }
}

void Shell::draw(Canvas& canvas, std::uint64_t ms) {
    canvas.clear();
    screen().draw(canvas, ms);
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
    if (pointerVisible_) {
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
    marker_->setEnabled(kMarkerDiplomacy, false);
    marker_->setEnabled(kMarkerIntel, false);
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
