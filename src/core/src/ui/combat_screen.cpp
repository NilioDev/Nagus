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
// How long each shot's fire shows, and the time between shots, on the tape
// of a round (milliseconds).
constexpr std::int64_t kShotTime = 700;
constexpr std::int64_t kShotGap = 220;
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
constexpr std::size_t kHailText = 429;

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
      question_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "fiskdlg.wdf"),
      util_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "butil.wdf"),
      views_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "b_views.wdf"),
      vcr_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "bvcr.wdf") {
    const Rect q = question_.layout().rect;
    question_.setOrigin({kView.x + (kView.w - q.w) / 2, kView.y + (kView.h - q.h) / 2});
    const Rect u = panel_.rect(kUtilArea);
    util_.setOrigin({u.x, u.y});
    const Rect v = panel_.rect(kViewsArea);
    views_.setOrigin({v.x, v.y});
    const Rect t = util_.rect(kVcrArea);
    vcr_.setOrigin({t.x, t.y});
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
    // In 3D: each side in rows and columns on its half, the planet behind
    // its owner's ships.
    spots_.resize(units.size());
    shown_.resize(units.size());
    for (int half = 0; half < 2; ++half) {
        const float side = half == 0 ? -1.0f : 1.0f;
        std::vector<std::size_t> ships;
        for (const std::size_t i : halves[half]) {
            if (units[i].ship < 0 && units[i].station < 0) {
                spots_[i] = {side * 520.0f, -260.0f, 420.0f};
            } else {
                ships.push_back(i);
            }
        }
        const int perRow = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(ships.size())))));
        for (std::size_t n = 0; n < ships.size(); ++n) {
            const int col = static_cast<int>(n) / perRow;
            const int row = static_cast<int>(n) % perRow;
            const float across = (static_cast<float>(row) - static_cast<float>(perRow - 1) / 2.0f) * 230.0f;
            spots_[ships[n]] = {side * (200.0f + static_cast<float>(col) * 230.0f), static_cast<float>((row + col) % 3 - 1) * 40.0f,
                                across};
        }
    }
    if (battle_.over()) {
        phase_ = Phase::Over;
    }
    refresh();
}

const Model* CombatScreen::model(std::size_t unit) {
    const auto& units = battle_.units();
    if (unit >= units.size() || (units[unit].ship < 0 && units[unit].station < 0) || units[unit].shipClass < 0 ||
        static_cast<std::size_t>(units[unit].shipClass) >= game_.economy.ships.size()) {
        return nullptr;
    }
    std::string name = game_.economy.ships[static_cast<std::size_t>(units[unit].shipClass)].picture + "_a";
    for (char& c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (assets_.image(name + ".gif").empty()) {
        return nullptr;
    }
    return assets_.model(name + ".hob");
}

bool CombatScreen::hasModel(std::size_t unit) { return model(unit) != nullptr; }

Rect CombatScreen::maneuverRect(Maneuver m) const {
    const int i = static_cast<int>(m);
    return {58 + (i % 5) * 115, 504 + (i / 5) * 20, 112, 18};
}

std::optional<Rect> CombatScreen::button(std::uint32_t id) const {
    if (id == kAutomatic || id == kManual || id == kHail) {
        return phase_ == Phase::Choose ? std::optional<Rect>(question_.rect(id)) : std::nullopt;
    }
    if (id >= kCameraUp && id <= kAuto) {
        return phase_ != Phase::Choose && view_ == View::Visual && util_.usable(id) ? std::optional<Rect>(util_.rect(id)) : std::nullopt;
    }
    if (id >= kTapeStart && id <= kTapeEnd) {
        return phase_ != Phase::Choose && view_ == View::Visual && vcr_.usable(id) ? std::optional<Rect>(vcr_.rect(id)) : std::nullopt;
    }
    if (id >= kTopView && id <= kShipView) {
        return phase_ != Phase::Choose && view_ == View::Visual && viewsOpen_ ? std::optional<Rect>(views_.rect(id)) : std::nullopt;
    }
    if (!panel_.usable(id)) {
        return std::nullopt;
    }
    return panel_.rect(id);
}

Rect CombatScreen::unitRect(std::size_t unit) const {
    const auto& from = view_ == View::Visual ? shown_ : places_;
    return unit < from.size() ? from[unit] : Rect{};
}

Point CombatScreen::buttonCentre(std::uint32_t id) const {
    const Rect r = id == kAutomatic || id == kManual || id == kHail ? question_.rect(id) : panel_.rect(id);
    return {r.centerX(), r.centerY()};
}

void CombatScreen::refresh() {
    for (const auto& e : panel_.layout().elements) {
        panel_.setVisible(e.id, false);
    }
    panel_.setVisible(kTurn, phase_ == Phase::Orders);
    panel_.setVisible(kFinish, phase_ == Phase::Over);
    panel_.setVisible(kVisual, true);
    panel_.setVisible(kTactical, true);
    panel_.setOn(kVisual, view_ == View::Visual);
    panel_.setOn(kTactical, view_ == View::Tactical);
    util_.setEnabled(kAuto, phase_ == Phase::Orders);
    util_.setOn(kViews, viewsOpen_);
    util_.setOn(kStatus, hud_);
    const std::pair<std::uint32_t, CameraView> views[] = {{kTopView, CameraView::Top},
                                                          {kChaseView, CameraView::Chase},
                                                          {kFrontView, CameraView::Front},
                                                          {kEnemyView, CameraView::Enemy},
                                                          {kShipView, CameraView::Ship}};
    for (const auto& [id, v] : views) {
        views_.setOn(id, cameraView_ == v);
    }
    // The tape: Stop and Play share a place, so only one shows.
    const bool tape = tapeLength() > 0;
    for (std::uint32_t id = kTapeStart; id <= kTapeEnd; ++id) {
        vcr_.setEnabled(id, tape);
    }
    vcr_.setVisible(kStop, tapeRate_ != 0);
    vcr_.setVisible(kPlay, tapeRate_ == 0);
    vcr_.setOn(kRewind, tapeRate_ < 0);
    vcr_.setOn(kForward, tapeRate_ > 1);
}

void CombatScreen::fightToEnd(bool hail) {
    // Hailing: the player's ships hail in this round; if not everyone
    // does, the computer fights the rest for them.
    if (hail && !battle_.over()) {
        battle_.fight(game_, Maneuver::Hail);
    }
    if (!battle_.over()) {
        battle_.fightToEnd(game_);
    }
    newRound();
    phase_ = Phase::Over;
    refresh();
}

bool CombatScreen::cameraButton(std::uint32_t id) {
    constexpr float kTurnStep = 0.3f;
    constexpr float kTiltStep = 0.15f;
    switch (id) {
    case kCameraLeft: turn_ -= kTurnStep; break;
    case kCameraRight: turn_ += kTurnStep; break;
    case kCameraUp: tilt_ = std::min(1.0f, tilt_ + kTiltStep); break;
    case kCameraDown: tilt_ = std::max(-0.6f, tilt_ - kTiltStep); break;
    case kZoomIn: zoom_ = std::max(0.4f, zoom_ * 0.8f); break;
    case kZoomOut: zoom_ = std::min(2.5f, zoom_ * 1.25f); break;
    case kPanorama:
        cameraView_ = CameraView::Panorama;
        turn_ = 0;
        tilt_ = 0;
        zoom_ = 1;
        break;
    case kViews: viewsOpen_ = !viewsOpen_; break;
    case kTapeStart:
        tapeMs_ = 0;
        tapeRate_ = 0;
        break;
    case kRewind: tapeRate_ = tapeRate_ < 0 ? 0 : -3; break;
    case kStop: tapeRate_ = 0; break;
    case kPlay:
        if (tapeMs_ >= tapeLength()) {
            tapeMs_ = 0;
        }
        tapeRate_ = 1;
        break;
    case kForward: tapeRate_ = tapeRate_ > 1 ? 1 : 3; break;
    case kTapeEnd:
        tapeMs_ = tapeLength();
        tapeRate_ = 0;
        break;
    case kStatus: hud_ = !hud_; break;
    case kAuto:
        if (phase_ == Phase::Orders) {
            fightToEnd(false);
        }
        break;
    case kTopView: cameraView_ = CameraView::Top; break;
    case kChaseView: cameraView_ = CameraView::Chase; break;
    case kFrontView: cameraView_ = CameraView::Front; break;
    case kEnemyView: cameraView_ = CameraView::Enemy; break;
    case kShipView: cameraView_ = CameraView::Ship; break;
    default: return false;
    }
    if (id >= kTopView && id <= kShipView) {
        viewsOpen_ = false;
        turn_ = 0;
        tilt_ = 0;
        zoom_ = 1;
    }
    refresh();
    return true;
}

Camera CombatScreen::camera(std::uint64_t ms) const {
    const auto& units = battle_.units();
    // Where the player's ships and the others' are, and one of the
    // player's ships and its target.
    Vec3 mine{};
    Vec3 theirs{};
    int nMine = 0;
    int nTheirs = 0;
    int ship = -1;
    for (std::size_t i = 0; i < units.size() && i < spots_.size(); ++i) {
        if (units[i].hull <= 0) {
            continue;
        }
        const bool own = units[i].owner == game_.player;
        Vec3& sum = own ? mine : theirs;
        sum = {sum.x + spots_[i].x, sum.y + spots_[i].y, sum.z + spots_[i].z};
        (own ? nMine : nTheirs) += 1;
        if (own && ship < 0 && (units[i].ship >= 0 || units[i].station >= 0)) {
            ship = static_cast<int>(i);
        }
    }
    const auto average = [](Vec3 v, int n, float fallback) {
        return n > 0 ? Vec3{v.x / static_cast<float>(n), v.y / static_cast<float>(n), v.z / static_cast<float>(n)} : Vec3{fallback, 0, 0};
    };
    mine = average(mine, nMine, -300.0f);
    theirs = average(theirs, nTheirs, 300.0f);
    const Vec3 own = ship >= 0 ? spots_[static_cast<std::size_t>(ship)] : mine;
    float reach = 0;
    for (const Vec3& p : spots_) {
        reach = std::max({reach, std::fabs(p.x), std::fabs(p.z)});
    }
    const float distance = 500.0f + reach * 1.3f;
    // From the player's side towards the others', flat.
    float ax = theirs.x - mine.x;
    float az = theirs.z - mine.z;
    const float len = std::max(1.0f, std::sqrt(ax * ax + az * az));
    ax /= len;
    az /= len;
    Camera c;
    c.focal = static_cast<float>(kView.w);
    switch (cameraView_) {
    case CameraView::Panorama: {
        // Swinging slowly to and fro above the player's side.
        const float swing = 0.35f * static_cast<float>(std::sin(static_cast<double>(ms % 62832u) / 10000.0));
        c.eye = {distance * std::sin(swing), distance * 0.45f, -distance * std::cos(swing)};
        c.target = {0, -40, 0};
        break;
    }
    case CameraView::Top:
        c.eye = {0, distance * 1.3f, -distance * 0.08f};
        c.target = {0, 0, 0};
        break;
    case CameraView::Chase:
        c.eye = {own.x - ax * 420.0f, own.y + 160.0f, own.z - az * 420.0f};
        c.target = theirs;
        break;
    case CameraView::Front:
        c.eye = {mine.x + ax * 650.0f, mine.y + 200.0f, mine.z + az * 650.0f};
        c.target = mine;
        break;
    case CameraView::Enemy:
        c.eye = {theirs.x + ax * 650.0f, theirs.y + 220.0f, theirs.z + az * 650.0f};
        c.target = mine;
        break;
    case CameraView::Ship:
        c.eye = {own.x + ax * 90.0f, own.y + 60.0f, own.z + az * 90.0f};
        c.target = theirs;
        break;
    }
    // The player's turn, height and distance, about what the camera looks at.
    float ox = c.eye.x - c.target.x;
    float oy = c.eye.y - c.target.y;
    float oz = c.eye.z - c.target.z;
    const float ct = std::cos(turn_);
    const float st = std::sin(turn_);
    const float rx = ox * ct - oz * st;
    oz = ox * st + oz * ct;
    ox = rx;
    const float flat = std::sqrt(ox * ox + oz * oz);
    const float r = std::sqrt(flat * flat + oy * oy);
    const float pitch = std::clamp(std::atan2(oy, std::max(1.0f, flat)) + tilt_, -1.2f, 1.45f);
    const float scale = flat > 0 ? r * std::cos(pitch) / flat : 0.0f;
    c.eye = {c.target.x + ox * scale * zoom_, c.target.y + r * std::sin(pitch) * zoom_, c.target.z + oz * scale * zoom_};
    if (flat <= 0) {
        c.eye.z -= 1.0f;
    }
    return c;
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
    return capitalised(assets_.lexicon()[m == Maneuver::Hail ? kHailText : maneuverName(m)]);
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
    newRound();
    if (battle_.over()) {
        phase_ = Phase::Over;
    }
    refresh();
}

void CombatScreen::newRound() {
    // The tape starts when the round first shows.
    lastMs_.reset();
    tapeMs_ = 0;
    tapeRate_ = 1;
    refresh();
}

std::int64_t CombatScreen::tapeLength() const {
    const auto n = static_cast<std::int64_t>(battle_.lastShots().size());
    return n == 0 ? 0 : (n - 1) * kShotGap + kShotTime * 2;
}

std::int64_t CombatScreen::shotStart(std::size_t shot) { return static_cast<std::int64_t>(shot) * kShotGap; }

std::int64_t CombatScreen::lastHit(std::size_t unit) const {
    std::int64_t at = -1;
    const auto& shots = battle_.lastShots();
    for (std::size_t s = 0; s < shots.size(); ++s) {
        if (shots[s].to == unit) {
            at = shotStart(s) + kShotTime / 2;
        }
    }
    return at;
}

float CombatScreen::shotLight(std::size_t shot) const {
    const std::int64_t since = tapeMs_ - shotStart(shot);
    return since < 0 || since >= kShotTime ? 0.0f : 1.0f - static_cast<float>(since) / static_cast<float>(kShotTime);
}

bool CombatScreen::stillThere(std::size_t unit) const {
    const std::int64_t hit = lastHit(unit);
    return battle_.lastDamage()[unit] > 0 && hit >= 0 && tapeMs_ < hit;
}

void CombatScreen::runTape(std::uint64_t ms) {
    if (lastMs_ && ms > *lastMs_) {
        tapeMs_ += static_cast<std::int64_t>(std::min<std::uint64_t>(ms - *lastMs_, 250)) * tapeRate_;
    }
    lastMs_ = ms;
    const std::int64_t end = tapeLength();
    if (tapeMs_ >= end && tapeRate_ > 0) {
        tapeMs_ = end;
        tapeRate_ = 0;
        refresh();
    } else if (tapeMs_ <= 0 && tapeRate_ < 0) {
        tapeMs_ = 0;
        tapeRate_ = 0;
        refresh();
    }
    tapeMs_ = std::clamp<std::int64_t>(tapeMs_, 0, end);
}

void CombatScreen::draw(Canvas& canvas, std::uint64_t ms) {
    runTape(ms);
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
    if (view_ == View::Visual) {
        drawVisual(canvas, ms, small);
    } else {
        drawTactical(canvas, ms, small);
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
    if (view_ == View::Visual && phase_ != Phase::Choose) {
        util_.draw(canvas);
        vcr_.draw(canvas);
        // How far the tape has run, under its buttons.
        const Rect first = vcr_.rect(kTapeStart);
        const Rect last = vcr_.rect(kTapeEnd);
        const Rect bar{first.x, std::max(vcr_.rect(kPlay).y + vcr_.rect(kPlay).h, last.y + last.h) + 3, last.x + last.w - first.x, 3};
        const std::int64_t length = tapeLength();
        canvas.fill(bar, kHullEmpty);
        if (length > 0) {
            canvas.fill({bar.x, bar.y, static_cast<int>(bar.w * tapeMs_ / length), bar.h}, kHullFull);
        }
        if (viewsOpen_) {
            views_.draw(canvas);
        }
    }
    if (phase_ == Phase::Choose) {
        canvas.blend(kView, 0xff000000u, 120);
        question_.draw(canvas);
        // Where, and who meets whom.
        std::string text = fillText(lex[kCombatIn], {std::to_string(battle_.at().x + 1) + "." + std::to_string(battle_.at().y + 1)});
        for (const int side : battle_.sides()) {
            int ships = 0;
            for (const auto& u : battle_.units()) {
                ships += u.owner == side && (u.ship >= 0 || u.station >= 0) ? 1 : 0;
            }
            const std::string name = side >= 0 && static_cast<std::size_t>(side) < game_.raceNames.size()
                                         ? game_.raceNames[static_cast<std::size_t>(side)]
                                         : "?";
            text += "\n" + name + ": " + std::to_string(ships) + (ships == 1 ? " ship" : " ships");
        }
        drawParagraph(canvas, font, text, question_.rect(kQuestionText), kInfoColour);
    }
}

void CombatScreen::drawTactical(Canvas& canvas, std::uint64_t /*ms*/, const Font& small) {
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
        const std::int64_t at = lastHit(i);
        if (hit > 0 && at >= 0 && tapeMs_ >= at && tapeMs_ < tapeLength()) {
            canvas.text(small, "-" + std::to_string(hit), r.x + r.w - 24, r.y + 2, kBadColour);
        }
    }
    // The fire of the last round, shot by shot as the tape runs.
    const auto& shots = battle_.lastShots();
    for (std::size_t k = 0; k < shots.size(); ++k) {
        if (shotLight(k) > 0) {
            const Rect a = places_[shots[k].from];
            const Rect b = places_[shots[k].to];
            beam(canvas, {a.centerX(), a.y + 25}, {b.centerX(), b.y + 25}, ownerColour(units[shots[k].from].owner));
        }
    }
}

void CombatScreen::drawLabel(Canvas& canvas, std::size_t i, const Rect& r, std::uint64_t /*ms*/, const Font& small) {
    const CombatUnit& u = battle_.units()[i];
    const Pixel colour = ownerColour(u.owner);
    if (u.ship >= 0 || u.station >= 0) {
        const ShipClass& sc = game_.economy.ships[static_cast<std::size_t>(u.shipClass)];
        canvas.text(small, sc.name, {r.x, r.y + r.h, r.w, 12}, colour, Align::Center);
        const Rect hull{r.x + r.w / 2 - 35, r.y + r.h + 14, 70, 4};
        canvas.fill(hull, kHullEmpty);
        canvas.fill({hull.x, hull.y, u.maxHull > 0 ? hull.w * u.hull / u.maxHull : 0, hull.h}, kHullFull);
    }
    const int hit = battle_.lastDamage()[i];
    const std::int64_t at = lastHit(i);
    if (hit > 0 && at >= 0 && tapeMs_ >= at && tapeMs_ < tapeLength()) {
        canvas.text(small, "-" + std::to_string(hit), r.x + r.w - 24, r.y + 2, kBadColour);
    }
}

void CombatScreen::drawVisual(Canvas& canvas, std::uint64_t ms, const Font& small) {
    const auto& units = battle_.units();
    Scene scene(canvas, kView, camera(ms));
    for (std::size_t i = 0; i < units.size(); ++i) {
        const CombatUnit& u = units[i];
        const bool planet = u.ship < 0 && u.station < 0;
        // Bigger ships for stronger hulls; the models' own sizes differ too
        // much to use.
        const float size = planet ? 200.0f
                           : u.station >= 0 ? 200.0f
                                            : 55.0f + 4.0f * std::sqrt(static_cast<float>(std::max(0, u.maxHull)));
        const auto p = scene.project(spots_[i]);
        shown_[i] = {};
        if (p) {
            const int r = std::max(8, static_cast<int>(size * scene.scale(p->depth) * 0.6f));
            shown_[i] = {static_cast<int>(p->x) - 2 * r, static_cast<int>(p->y) - r, 4 * r, 2 * r};
        }
        if (planet) {
            scene.ball(spots_[i], size, 0xff305080u);
            continue;
        }
        if (u.hull <= 0 && !stillThere(i)) {
            // Destroyed in this round: a fireball after its last hit, then
            // nothing.
            const std::int64_t since = tapeMs_ - lastHit(i);
            if (battle_.lastDamage()[i] > 0 && lastHit(i) >= 0 && since < kShotTime * 2) {
                const float t = static_cast<float>(since) / static_cast<float>(kShotTime * 2);
                scene.glow(spots_[i], size * (0.5f + t), 0xffffb040u, 1.0f - t);
                scene.glow(spots_[i], size * 0.4f * (1.0f - t), 0xffffffffu, 1.0f - t);
            }
            continue;
        }
        // The player's ships face right, the others' left.
        const float yaw = u.owner == game_.player ? 1.5708f : -1.5708f;
        if (const Model* m = model(i)) {
            const ShipClass& sc = game_.economy.ships[static_cast<std::size_t>(u.shipClass)];
            std::string tex = sc.picture + "_a.gif";
            scene.model(*m, assets_.image(tex), spots_[i], yaw, size, battle_.gone(i) ? 0.35f : 1.0f);
        } else {
            scene.ball(spots_[i], size * 0.4f, ownerColour(u.owner));
        }
    }
    const auto& shots = battle_.lastShots();
    for (std::size_t k = 0; k < shots.size(); ++k) {
        const float light = shotLight(k);
        if (light > 0) {
            scene.beam(spots_[shots[k].from], spots_[shots[k].to], ownerColour(units[shots[k].from].owner), 2);
            scene.glow(spots_[shots[k].to], 26.0f, 0xffffd080u, 0.8f * light);
        }
    }
    for (std::size_t i = 0; i < units.size() && hud_; ++i) {
        if (units[i].hull > 0 || (units[i].ship < 0 && units[i].station < 0) || stillThere(i)) {
            drawLabel(canvas, i, shown_[i], ms, small);
        }
    }
}

void CombatScreen::pointerMoved(Point p) {
    if (drag_) {
        // Dragging across the 3D view turns and tilts the camera.
        turn_ += static_cast<float>(p.x - drag_->x) * 0.01f;
        tilt_ = std::clamp(tilt_ + static_cast<float>(p.y - drag_->y) * 0.005f, -0.6f, 1.0f);
        drag_ = p;
    }
    pointer_ = p;
    if (phase_ == Phase::Choose) {
        question_.pointerMoved(p);
        return;
    }
    panel_.pointerMoved(p);
    if (view_ == View::Visual) {
        util_.pointerMoved(p);
        vcr_.pointerMoved(p);
        views_.pointerMoved(p);
    }
}

void CombatScreen::pointerPressed(Point p) {
    pointer_ = p;
    if (phase_ == Phase::Choose) {
        question_.pointerPressed(p);
        return;
    }
    panel_.pointerPressed(p);
    if (view_ == View::Visual) {
        util_.pointerPressed(p);
        vcr_.pointerPressed(p);
        if (viewsOpen_) {
            views_.pointerPressed(p);
        }
        const bool onViews = viewsOpen_ && views_.layout().rect.w > 0 &&
                             Rect{views_.origin().x, views_.origin().y, views_.layout().rect.w, views_.layout().rect.h}.contains(p.x, p.y);
        if (kView.contains(p.x, p.y) && !onViews) {
            drag_ = p;
        }
    }
}

void CombatScreen::pointerReleased(Point p) {
    pointer_ = p;
    drag_.reset();
    if (phase_ == Phase::Choose) {
        const auto id = question_.pointerReleased(p);
        if (id && (*id == kAutomatic || *id == kHail)) {
            fightToEnd(*id == kHail);
        } else if (id && *id == kManual) {
            phase_ = Phase::Orders;
        }
        refresh();
        return;
    }
    if (view_ == View::Visual) {
        const auto viewId = viewsOpen_ ? views_.pointerReleased(p) : std::nullopt;
        const auto utilId = util_.pointerReleased(p);
        const auto vcrId = vcr_.pointerReleased(p);
        if ((viewId && cameraButton(*viewId)) || (utilId && cameraButton(*utilId)) || (vcrId && cameraButton(*vcrId))) {
            assets_.clicked();
            refresh();
            return;
        }
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
    if (id && (*id == kVisual || *id == kTactical)) {
        view_ = *id == kVisual ? View::Visual : View::Tactical;
        refresh();
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
    // The shoulder buttons switch between the 3D and the flat view.
    if (c == Command::Next || c == Command::Previous) {
        view_ = view_ == View::Visual ? View::Tactical : View::Visual;
        refresh();
        return std::nullopt;
    }
    switch (phase_) {
    case Phase::Choose:
        if (c == Command::Up || c == Command::Down || c == Command::Left || c == Command::Right) {
            return question_.step(c, pointer_);
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
