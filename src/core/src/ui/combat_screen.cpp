// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/combat_screen.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace nagus {

namespace {

constexpr const char* kPrefixes = "chfkr";
constexpr const char* kFaces[kEmpires] = {"cardsn", "human", "ferngi", "klingn", "romuln"};
constexpr Pixel kEmpireColours[kEmpires] = {0xffe0c040u, 0xff60a0ffu, 0xffffa040u, 0xffff5050u, 0xff40e070u};
constexpr Pixel kMinorColour = 0xffd0d0d0u;
constexpr Pixel kInfoColour = 0xffffcc66u;
constexpr Pixel kNameColour = 0xff9098b0u;
constexpr Pixel kChosen = 0xffffffffu;
constexpr Pixel kBadColour = 0xffff4040u;
constexpr Pixel kHullEmpty = 0xff303848u;
constexpr Pixel kHullFull = 0xff40e070u;
// How long the fire of a round shows, in milliseconds.
constexpr std::uint64_t kShotTime = 900;
// Lexicon numbers.
constexpr std::size_t kCombatIn = 167;  // "Combat occurring in sector %1"
constexpr std::size_t kTurnWord = 1370; // "Turn"
constexpr std::size_t kVictorious = 1410;
constexpr std::size_t kDefeated = 215;
constexpr std::size_t kRetreat = 1053;
constexpr std::size_t kOrdersWord = 892;
// The cells of the units in the view.
constexpr int kCellW = 100;
constexpr int kCellH = 100;
constexpr int kHeader = 44;
constexpr int kColumns = 3;

std::string capitalised(std::string s) {
    if (!s.empty()) {
        s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    }
    return s;
}

// The lexicon's short name for hailing; the maneuver's own is long.
constexpr std::size_t kHail = 429;

Pixel ownerColour(int owner) { return owner >= 0 && owner < kEmpires ? kEmpireColours[owner] : kMinorColour; }

// A line of small squares, for weapons' fire.
void beam(Canvas& canvas, Point a, Point b, Pixel colour) {
    const int steps = std::max(std::abs(b.x - a.x), std::abs(b.y - a.y)) / 2 + 1;
    for (int i = 0; i <= steps; ++i) {
        canvas.fill({a.x + (b.x - a.x) * i / steps, a.y + (b.y - a.y) * i / steps, 2, 2}, colour);
    }
}

} // namespace

CombatScreen::CombatScreen(Assets& assets, const Game& game, Sector at)
    : assets_(assets), game_(game), battle_(game, at),
      panel_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "tactscn.wdf"),
      question_(assets, "tactical.wdf") {
    question_.setOrigin({kView.x + (kView.w - 144) / 2, kView.y + (kView.h - 96) / 2});
    // Each side's ships in a grid: the player's on the left, the others'
    // on the right.
    const auto& units = battle_.units();
    places_.resize(units.size());
    std::vector<std::size_t> halves[2];
    for (std::size_t i = 0; i < units.size(); ++i) {
        halves[units[i].owner == game.player ? 0 : 1].push_back(i);
    }
    // Both sides' rows the same height, the whole in the middle of the view.
    const int rows = std::max(1, static_cast<int>((std::max(halves[0].size(), halves[1].size()) + kColumns - 1) / kColumns));
    const int space = kView.h - kHeader - 10;
    const int rowH = std::max(30, std::min(kCellH, space / rows));
    const int top = kView.y + kHeader + (space - rows * rowH) / 2;
    for (int half = 0; half < 2; ++half) {
        const auto& mine = halves[half];
        const int left = kView.x + 5 + half * (kView.w / 2);
        for (std::size_t n = 0; n < mine.size(); ++n) {
            const int col = static_cast<int>(n) % kColumns;
            const int row = static_cast<int>(n) / kColumns;
            // The player's front line faces right, the enemy's left.
            const int x = half == 0 ? left + (kColumns - 1 - col) * kCellW : left + col * kCellW;
            places_[mine[n]] = {x, top + row * rowH, kCellW, 80};
        }
    }
    if (battle_.over()) {
        phase_ = Phase::Over;
    }
    refresh();
}

Rect CombatScreen::maneuverRect(Maneuver m) const {
    const int i = static_cast<int>(m);
    return {58 + (i % 5) * 115, 504 + (i / 5) * 20, 112, 18};
}

std::optional<Rect> CombatScreen::button(std::uint32_t id) const {
    if (id == kAutomatic || id == kManual) {
        return phase_ == Phase::Choose ? std::optional<Rect>(question_.rect(id)) : std::nullopt;
    }
    if (!panel_.usable(id)) {
        return std::nullopt;
    }
    return panel_.rect(id);
}

Rect CombatScreen::unitRect(std::size_t unit) const { return unit < places_.size() ? places_[unit] : Rect{}; }

Point CombatScreen::buttonCentre(std::uint32_t id) const {
    const Rect r = id == kAutomatic || id == kManual ? question_.rect(id) : panel_.rect(id);
    return {r.centerX(), r.centerY()};
}

void CombatScreen::refresh() {
    for (const auto& e : panel_.layout().elements) {
        panel_.setVisible(e.id, false);
    }
    panel_.setVisible(kTurn, phase_ == Phase::Orders);
    panel_.setVisible(kFinish, phase_ == Phase::Over);
}

std::string CombatScreen::resultText() const {
    if (battle_.avoided()) {
        return "Combat avoided";
    }
    const auto& units = battle_.units();
    bool mine = false;
    bool theirs = false;
    bool fled = false;
    for (std::size_t i = 0; i < units.size(); ++i) {
        const bool here = units[i].hull > 0 && !battle_.gone(i);
        if (units[i].owner == game_.player) {
            mine = mine || here;
            fled = fled || (units[i].hull > 0 && battle_.gone(i));
        } else if (hostile(game_, game_.player, units[i].owner)) {
            theirs = theirs || here;
        }
    }
    const Lexicon& lex = assets_.lexicon();
    if (mine && !theirs) {
        return lex[kVictorious];
    }
    if (fled) {
        return lex[kRetreat];
    }
    if (!mine) {
        return lex[kDefeated];
    }
    return "Combat ended";
}

std::string CombatScreen::maneuverText(Maneuver m) const {
    return capitalised(assets_.lexicon()[m == Maneuver::Hail ? kHail : maneuverName(m)]);
}

const Image& CombatScreen::mirrored(const std::string& name) {
    auto it = mirrored_.find(name);
    if (it == mirrored_.end()) {
        const Image& from = assets_.image(name);
        Image to(from.width, from.height);
        for (int y = 0; y < from.height; ++y) {
            for (int x = 0; x < from.width; ++x) {
                to.at(from.width - 1 - x, y) = from.at(x, y);
            }
        }
        it = mirrored_.emplace(name, std::move(to)).first;
    }
    return it->second;
}

void CombatScreen::fightRound() {
    if (phase_ != Phase::Orders) {
        return;
    }
    battle_.fight(game_, chosen_);
    newShots_ = true;
    if (battle_.over()) {
        phase_ = Phase::Over;
    }
    refresh();
}

void CombatScreen::draw(Canvas& canvas, std::uint64_t ms) {
    if (newShots_) {
        shotsAt_ = ms;
        newShots_ = false;
    }
    const int player = std::clamp(game_.player, 0, kEmpires - 1);
    const Font& small = assets_.font(std::string(kFaces[player]) + "9.fnt");
    const Font& font = assets_.font(std::string(kFaces[player]) + "15.fnt");
    const Lexicon& lex = assets_.lexicon();
    panel_.draw(canvas);
    canvas.fill(kView, 0xff000000u);
    // A few stars, the same each time.
    for (int i = 0; i < 90; ++i) {
        const std::uint32_t h = combatRandom(static_cast<std::uint32_t>(i), 7u, 11u, 13u);
        canvas.fill({kView.x + static_cast<int>(h % static_cast<std::uint32_t>(kView.w)),
                     kView.y + static_cast<int>((h >> 12) % static_cast<std::uint32_t>(kView.h)), 1, 1},
                    (h >> 28) > 10 ? 0xffc0c8ffu : 0xff606880u);
    }
    canvas.text(font, fillText(lex[kCombatIn], {std::to_string(battle_.at().x + 1) + "." + std::to_string(battle_.at().y + 1)}),
                {kView.x, kView.y + 4, kView.w, 20}, kInfoColour, Align::Center);
    // Each side's name and its maneuver last round.
    for (int half = 0; half < 2; ++half) {
        std::string names;
        Pixel colour = kNameColour;
        for (const int side : battle_.sides()) {
            if ((side == game_.player) != (half == 0)) {
                continue;
            }
            std::string name = side >= 0 && static_cast<std::size_t>(side) < game_.raceNames.size()
                                   ? game_.raceNames[static_cast<std::size_t>(side)]
                                   : "?";
            if (battle_.round() > 0) {
                name += ": " + maneuverText(battle_.lastManeuver(side));
            }
            names += (names.empty() ? "" : ", ") + name;
            colour = ownerColour(side);
        }
        canvas.text(small, names, {kView.x + half * kView.w / 2 + 8, kView.y + 24, kView.w / 2 - 16, 14}, colour,
                    half == 0 ? Align::Left : Align::Right);
    }
    const auto& units = battle_.units();
    for (std::size_t i = 0; i < units.size(); ++i) {
        const CombatUnit& u = units[i];
        const Rect r = places_[i];
        const Pixel colour = ownerColour(u.owner);
        if (u.ship < 0 && u.station < 0) {
            // A colony's defences: the planet.
            const int system = game_.galaxy.systemAt(battle_.at());
            for (int row = 0; row < 40; ++row) {
                const int half = static_cast<int>(20.0 * std::sqrt(1.0 - (row - 19.5) * (row - 19.5) / 400.0));
                canvas.fill({r.x + r.w / 2 - half, r.y + 5 + row, 2 * half, 1}, 0xff305080u);
            }
            const std::string name = system >= 0 ? game_.galaxy.systems[static_cast<std::size_t>(system)].name : "";
            canvas.text(small, name, {r.x, r.y + 50, r.w, 12}, colour, Align::Center);
        } else {
            const ShipClass& sc = game_.economy.ships[static_cast<std::size_t>(u.shipClass)];
            if (u.hull > 0) {
                // The others' ships face the player's.
                const Image& pic = u.owner == game_.player ? assets_.image("i_" + sc.picture + "60.tga") : mirrored("i_" + sc.picture + "60.tga");
                canvas.draw(pic, r.x + (r.w - pic.width) / 2, r.y);
                if (battle_.gone(i)) {
                    canvas.blend({r.x, r.y, r.w, 50}, 0xff000000u, 170);
                }
            } else {
                // Destroyed: a red cross where it was.
                for (int d = 0; d < 30; ++d) {
                    canvas.fill({r.x + 35 + d, r.y + 10 + d, 2, 2}, kBadColour);
                    canvas.fill({r.x + 65 - d, r.y + 10 + d, 2, 2}, kBadColour);
                }
            }
            canvas.text(small, sc.name, {r.x, r.y + 50, r.w, 12}, colour, Align::Center);
            const Rect hull{r.x + 15, r.y + 64, r.w - 30, 4};
            canvas.fill(hull, kHullEmpty);
            canvas.fill({hull.x, hull.y, u.maxHull > 0 ? hull.w * u.hull / u.maxHull : 0, hull.h}, kHullFull);
        }
        const int hit = battle_.lastDamage()[i];
        if (hit > 0 && ms - shotsAt_ < kShotTime * 2) {
            canvas.text(small, "-" + std::to_string(hit), r.x + r.w - 24, r.y + 2, kBadColour);
        }
    }
    // The fire of the last round, fading.
    if (ms - shotsAt_ < kShotTime) {
        for (const auto& s : battle_.lastShots()) {
            const Rect a = places_[s.from];
            const Rect b = places_[s.to];
            beam(canvas, {a.centerX(), a.y + 25}, {b.centerX(), b.y + 25}, ownerColour(units[s.from].owner));
        }
    }
    // Below: the maneuvers, the chosen one's help and the round.
    if (phase_ == Phase::Orders) {
        canvas.text(small, lex[kOrdersWord], 58, 490, kInfoColour);
        for (int m = 0; m < kManeuvers; ++m) {
            const Maneuver man = static_cast<Maneuver>(m);
            const Rect r = maneuverRect(man);
            const bool on = man == chosen_;
            if (on) {
                canvas.fill(r, 0xff604010u);
            }
            canvas.text(small, maneuverText(man), r, on ? kChosen : (r.contains(pointer_.x, pointer_.y) ? kInfoColour : kNameColour), Align::Center);
        }
        drawParagraph(canvas, small, lex[maneuverHelp(chosen_)], {60, 548, 570, 42}, kInfoColour);
    } else if (phase_ == Phase::Over) {
        canvas.text(font, resultText(), {kView.x, kView.y + kView.h / 2 - 12, kView.w, 24}, kChosen, Align::Center);
    }
    canvas.text(font, lex[kTurnWord] + " " + std::to_string(std::min(battle_.round() + (phase_ == Phase::Over ? 0 : 1), kTacticalRounds)) + "/" +
                          std::to_string(kTacticalRounds),
                {645, 535, 145, 24}, 0xff000000u, Align::Center);
    if (phase_ == Phase::Choose) {
        canvas.blend(kView, 0xff000000u, 120);
        question_.draw(canvas);
    }
}

void CombatScreen::pointerMoved(Point p) {
    pointer_ = p;
    (phase_ == Phase::Choose ? question_ : panel_).pointerMoved(p);
}

void CombatScreen::pointerPressed(Point p) {
    pointer_ = p;
    (phase_ == Phase::Choose ? question_ : panel_).pointerPressed(p);
}

void CombatScreen::pointerReleased(Point p) {
    pointer_ = p;
    if (phase_ == Phase::Choose) {
        const auto id = question_.pointerReleased(p);
        if (id && *id == kAutomatic) {
            battle_.fightToEnd(game_);
            newShots_ = true;
            phase_ = Phase::Over;
        } else if (id && *id == kManual) {
            phase_ = Phase::Orders;
        }
        refresh();
        return;
    }
    const auto id = panel_.pointerReleased(p);
    if (id && *id == kTurn) {
        fightRound();
        return;
    }
    if (id && *id == kFinish) {
        outcome_ = Outcome::Accept;
        return;
    }
    if (phase_ == Phase::Orders) {
        for (int m = 0; m < kManeuvers; ++m) {
            if (maneuverRect(static_cast<Maneuver>(m)).contains(p.x, p.y)) {
                assets_.clicked();
                chosen_ = static_cast<Maneuver>(m);
            }
        }
    }
}

std::optional<Point> CombatScreen::command(Command c) {
    switch (phase_) {
    case Phase::Choose:
        if (c == Command::Up || c == Command::Down || c == Command::Left || c == Command::Right) {
            const bool manual = question_.rect(kManual).contains(pointer_.x, pointer_.y);
            return buttonCentre(manual == (c == Command::Up || c == Command::Left) ? kAutomatic : kManual);
        }
        return std::nullopt;
    case Phase::Orders: {
        // The arrows choose the maneuver; the pointer waits on Turn, so A
        // fights the round.
        int m = static_cast<int>(chosen_);
        switch (c) {
        case Command::Left: m = (m + kManeuvers - 1) % kManeuvers; break;
        case Command::Right: m = (m + 1) % kManeuvers; break;
        case Command::Up:
        case Command::Down: m = (m + 5) % kManeuvers; break;
        default: return std::nullopt;
        }
        chosen_ = static_cast<Maneuver>(m);
        return buttonCentre(kTurn);
    }
    case Phase::Over:
        if (c == Command::Start) {
            outcome_ = Outcome::Accept;
            return std::nullopt;
        }
        return buttonCentre(kFinish);
    }
    return std::nullopt;
}

} // namespace nagus
