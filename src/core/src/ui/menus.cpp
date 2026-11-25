// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/menus.hpp"

#include <array>

namespace nagus {

namespace {

// Lexicon numbers of the values shown on the set-up screen
// (docs/rules/menus.md).
constexpr std::array<std::size_t, GameSettings::kShapes> kShapeText{555, 264, 1056, 1172};
constexpr std::array<std::size_t, GameSettings::kSizes> kSizeText{1158, 1156, 596};
constexpr std::array<std::size_t, GameSettings::kMinorRaces> kMinorText{872, 358, 1161, 622};
constexpr std::array<std::size_t, GameSettings::kDifficulties> kDifficultyText{1150, 256, 873, 433, 482};
constexpr std::array<std::size_t, GameSettings::kCivilizations> kCivilizationText{88, 252, 234, 312, 4};
constexpr std::array<std::size_t, GameSettings::kVictories> kVictoryText{248, 1253, 1407};
constexpr std::size_t kOn = 882;
constexpr std::size_t kOff = 880;
constexpr std::size_t kManual = 621;
constexpr std::size_t kAutomatic = 52;

constexpr std::array<const char*, GameSettings::kShapes> kShapePicture{"cc_gsir.tga", "cc_gsel.tga", "cc_gsrg.tga",
                                                                      "cc_gssp.tga"};
constexpr std::array<const char*, GameSettings::kSizes> kSizePicture{"cc_gzsm.tga", "cc_gzmd.tga", "cc_gzlg.tga"};

// Set-up screen elements.
constexpr std::uint32_t kShapeButton = 0x2412;
constexpr std::uint32_t kShapeText_ = 0x23f6;
constexpr std::uint32_t kShapePictureSlot = 0x23f4;
constexpr std::uint32_t kSizeButton = 0x2411;
constexpr std::uint32_t kSizeText_ = 0x23f5;
constexpr std::uint32_t kSizePictureSlot = 0x23f3;
constexpr std::uint32_t kMinorButton = 0x23f7;
constexpr std::uint32_t kMinorText_ = 0x23f8;
constexpr std::uint32_t kDifficultyButton = 0x23f9;
constexpr std::uint32_t kDifficultyText_ = 0x23fa;
constexpr std::array<std::uint32_t, 4> kTimerArrows{0x23fb, 0x23fc, 0x23fe, 0x23ff};
constexpr std::uint32_t kStrategicTimerText = 0x23fd;
constexpr std::uint32_t kTacticalTimerText = 0x2400;
constexpr std::uint32_t kEventsButton = 0x2401;
constexpr std::uint32_t kEventsText = 0x2402;
constexpr std::uint32_t kCombatButton = 0x2403;
constexpr std::uint32_t kCombatText = 0x2404;
constexpr std::uint32_t kVictoryButton = 0x2405;
constexpr std::uint32_t kVictoryText_ = 0x2406;
constexpr std::uint32_t kFirstCivilizationButton = 0x2407; // then every second number
constexpr std::uint32_t kFirstCivilizationText = 0x2408;

// Empire selection elements, in the order Cardassian, Federation, Ferengi,
// Klingon, Romulan.
constexpr std::array<std::uint32_t, kEmpires> kEmpireButton{0x2391, 0x2392, 0x2393, 0x2394, 0x2395};
constexpr std::array<std::uint32_t, kEmpires> kEmpireIcon{0x238c, 0x238d, 0x238e, 0x238f, 0x2390};
constexpr std::array<std::uint32_t, kEmpires> kTeamButton{0x23a4, 0x23a5, 0x23a6, 0x23a7, 0x23a8};
constexpr std::array<char, kEmpires> kEmpireLetter{'c', 'h', 'f', 'k', 'r'};
constexpr std::uint32_t kDescriptionText = 0x2398;
constexpr std::uint32_t kPortraitSlot = 0x2399;

int next(int value, int count) { return (value + 1) % count; }

// The colours for text fields whose layout leaves the colour to the code.
constexpr Pixel kValueColour = rgb(0, 255, 0);
constexpr Pixel kDescriptionColour = rgb(255, 204, 102);

void drawField(Canvas& canvas, Assets& assets, const Panel& panel, std::uint32_t id, const std::string& value) {
    const auto* e = panel.layout().find(id);
    if (e == nullptr || e->font.empty()) {
        return;
    }
    const Rect r{panel.layout().rect.x + e->rect.x, panel.layout().rect.y + e->rect.y, e->rect.w, e->rect.h};
    canvas.text(assets.font(e->font), value, r, e->ownColour ? e->colour : kValueColour, Align::Center);
}

void drawPicture(Canvas& canvas, Assets& assets, const Panel& panel, std::uint32_t slot, const std::string& name) {
    const Rect r = panel.rect(slot);
    const Image& img = assets.image(name);
    canvas.draw(img, panel.layout().rect.x + r.x + (r.w - img.width) / 2, panel.layout().rect.y + r.y + (r.h - img.height) / 2);
}

} // namespace

// --- Main menu --------------------------------------------------------------

MainMenu::MainMenu(Assets& assets) : panel_(assets, "opening.wdf") {
    // Only what Nagus can do yet can be chosen; the rest is drawn darker.
    for (const std::uint32_t id : {kContinue, kNewMultiPlayer, kLoad, kHallOfFame, kSave, kOptions, kRetire}) {
        panel_.setEnabled(id, false);
    }
}

void MainMenu::draw(Canvas& canvas, std::uint64_t) { panel_.draw(canvas); }

void MainMenu::pointerReleased(Point p) {
    last_ = p;
    const auto clicked = panel_.pointerReleased(p);
    if (clicked == kNewSinglePlayer) {
        outcome_ = Outcome::NewGame;
    } else if (clicked == kQuit) {
        outcome_ = Outcome::Quit;
    }
}

std::optional<Point> MainMenu::command(Command c) {
    if (c == Command::Start) {
        outcome_ = Outcome::NewGame;
        return std::nullopt;
    }
    return panel_.step(c, last_);
}

// --- Game set-up ------------------------------------------------------------

GameSetup::GameSetup(Assets& assets, GameSettings settings)
    : assets_(assets), panel_(assets, "gmesetup.wdf"), settings_(settings) {
    settings_.clamp();
    // The turn timers belong to multiplayer games: they stay off.
    for (const std::uint32_t id : kTimerArrows) {
        panel_.setEnabled(id, false);
    }
}

void GameSetup::draw(Canvas& canvas, std::uint64_t) {
    panel_.draw(canvas);
    const Lexicon& text = assets_.lexicon();
    const auto at = [](const auto& table, int i) { return table[static_cast<std::size_t>(i)]; };
    drawPicture(canvas, assets_, panel_, kShapePictureSlot, at(kShapePicture, settings_.galaxyShape));
    drawPicture(canvas, assets_, panel_, kSizePictureSlot, at(kSizePicture, settings_.galaxySize));
    drawField(canvas, assets_, panel_, kShapeText_, text[at(kShapeText, settings_.galaxyShape)]);
    drawField(canvas, assets_, panel_, kSizeText_, text[at(kSizeText, settings_.galaxySize)]);
    drawField(canvas, assets_, panel_, kMinorText_, text[at(kMinorText, settings_.minorRaces)]);
    drawField(canvas, assets_, panel_, kDifficultyText_, text[at(kDifficultyText, settings_.difficulty)]);
    drawField(canvas, assets_, panel_, kStrategicTimerText, text[kOff]);
    drawField(canvas, assets_, panel_, kTacticalTimerText, text[kOff]);
    drawField(canvas, assets_, panel_, kEventsText, text[settings_.randomEvents ? kOn : kOff]);
    drawField(canvas, assets_, panel_, kCombatText, text[settings_.automaticCombat ? kAutomatic : kManual]);
    drawField(canvas, assets_, panel_, kVictoryText_, text[at(kVictoryText, settings_.victory)]);
    for (int i = 0; i < kEmpires; ++i) {
        drawField(canvas, assets_, panel_, kFirstCivilizationText + 2u * static_cast<std::uint32_t>(i),
                  text[at(kCivilizationText, settings_.civilization[static_cast<std::size_t>(i)])]);
    }
}

void GameSetup::pointerMoved(Point p) {
    last_ = p;
    panel_.pointerMoved(p);
}

void GameSetup::pointerReleased(Point p) {
    last_ = p;
    if (const auto clicked = panel_.pointerReleased(p)) {
        click(*clicked);
    }
}

void GameSetup::click(std::uint32_t id) {
    GameSettings& s = settings_;
    switch (id) {
    case kAccept: outcome_ = Outcome::Accept; break;
    case kCancel: outcome_ = Outcome::Back; break;
    case kShapeButton: s.galaxyShape = next(s.galaxyShape, GameSettings::kShapes); break;
    case kSizeButton: s.galaxySize = next(s.galaxySize, GameSettings::kSizes); break;
    case kMinorButton: s.minorRaces = next(s.minorRaces, GameSettings::kMinorRaces); break;
    case kDifficultyButton: s.difficulty = next(s.difficulty, GameSettings::kDifficulties); break;
    case kEventsButton: s.randomEvents = !s.randomEvents; break;
    case kCombatButton: s.automaticCombat = !s.automaticCombat; break;
    case kVictoryButton: s.victory = next(s.victory, GameSettings::kVictories); break;
    default:
        for (int i = 0; i < kEmpires; ++i) {
            if (id == kFirstCivilizationButton + 2u * static_cast<std::uint32_t>(i)) {
                int& c = s.civilization[static_cast<std::size_t>(i)];
                c = next(c, GameSettings::kCivilizations);
            }
        }
        break;
    }
}

std::optional<Point> GameSetup::command(Command c) {
    if (c == Command::Back) {
        outcome_ = Outcome::Back;
        return std::nullopt;
    }
    if (c == Command::Start) {
        outcome_ = Outcome::Accept;
        return std::nullopt;
    }
    return panel_.step(c, last_);
}

// --- Empire selection -------------------------------------------------------

EmpireSelect::EmpireSelect(Assets& assets, GameSettings settings)
    : assets_(assets), panel_(assets, "emppick.wdf"), settings_(settings) {
    settings_.clamp();
    choose(settings_.empire);
}

void EmpireSelect::choose(int empire) {
    settings_.empire = empire;
    for (int i = 0; i < kEmpires; ++i) {
        panel_.setOn(kEmpireButton[static_cast<std::size_t>(i)], i == empire);
        // Teams only matter when the victory condition is team play.
        panel_.setVisible(kTeamButton[static_cast<std::size_t>(i)], settings_.victory == 1);
    }
}

void EmpireSelect::draw(Canvas& canvas, std::uint64_t) {
    // The chosen empire's portrait lies under the description.
    const auto empire = static_cast<std::size_t>(settings_.empire);
    const Layout& l = panel_.layout();
    panel_.draw(canvas);
    drawPicture(canvas, assets_, panel_, kPortraitSlot, std::string("es_") + kEmpireLetter[empire] + "0.tga");
    for (std::size_t i = 0; i < kEmpires; ++i) {
        drawPicture(canvas, assets_, panel_, kEmpireIcon[i],
                    std::string("es_") + kEmpireLetter[i] + "icn" + (i == empire ? "1" : "0") + ".tga");
    }
    if (const auto* e = l.find(kDescriptionText); e != nullptr && !e->font.empty()) {
        const auto& descriptions = assets_.texts("racedesc.rst");
        if (empire < descriptions.size()) {
            const Rect r{l.rect.x + e->rect.x, l.rect.y + e->rect.y, e->rect.w, e->rect.h};
            drawParagraph(canvas, assets_.font(e->font), descriptions[empire], r,
                          e->ownColour ? e->colour : kDescriptionColour);
        }
    }
}

void EmpireSelect::pointerMoved(Point p) {
    last_ = p;
    panel_.pointerMoved(p);
}

void EmpireSelect::pointerReleased(Point p) {
    last_ = p;
    if (const auto clicked = panel_.pointerReleased(p)) {
        click(*clicked);
    }
}

void EmpireSelect::click(std::uint32_t id) {
    if (id == kAccept) {
        outcome_ = Outcome::Accept;
    } else if (id == kCancel) {
        outcome_ = Outcome::Back;
    }
    for (int i = 0; i < kEmpires; ++i) {
        if (id == kEmpireButton[static_cast<std::size_t>(i)]) {
            choose(i);
        }
    }
}

std::optional<Point> EmpireSelect::command(Command c) {
    if (c == Command::Back) {
        outcome_ = Outcome::Back;
        return std::nullopt;
    }
    if (c == Command::Start) {
        outcome_ = Outcome::Accept;
        return std::nullopt;
    }
    if (c == Command::Next || c == Command::Previous) {
        choose((settings_.empire + (c == Command::Next ? 1 : kEmpires - 1)) % kEmpires);
        return std::nullopt;
    }
    return panel_.step(c, last_);
}

} // namespace nagus
