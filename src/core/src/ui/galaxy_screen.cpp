// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/galaxy_screen.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr int kCloseSector = 25;
constexpr const char* kPrefixes = "chfkr";
constexpr const char* kSmallFonts[kEmpires] = {"cardsn9.fnt", "human9.fnt", "ferngi9.fnt", "klingn9.fnt", "romuln9.fnt"};
constexpr const char* kFonts[kEmpires] = {"cardsn15.fnt", "human15.fnt", "ferngi15.fnt", "klingn15.fnt", "romuln15.fnt"};
// Colours for the empires' names on the map (our choice).
constexpr Pixel kEmpireColours[kEmpires] = {0xffe0c040u, 0xff60a0ffu, 0xffffa040u, 0xffff5050u, 0xff40e070u};
constexpr Pixel kGrid = 0xff18204au;
constexpr Pixel kMinorColour = 0xffd0d0d0u;
constexpr Pixel kNameColour = 0xff9098b0u;
constexpr Pixel kChosen = 0xffffff00u;
constexpr Pixel kInfoColour = 0xffffcc66u;
constexpr std::uint64_t kFrameMs = 90; // how long each frame of a turning planet shows

std::string format1(const std::string& pattern, const std::string& value) {
    std::string out = pattern;
    if (const auto at = out.find("%s"); at != std::string::npos) {
        out.replace(at, 2, value);
    }
    return out;
}

} // namespace

GalaxyScreen::GalaxyScreen(Assets& assets, Game& game)
    : assets_(assets), game_(game), prefix_(kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]),
      panel_(assets, std::string(1, prefix_) + "maingal.wdf"), info_(assets, std::string(1, prefix_) + "_starmp.wdf"),
      system_(assets, std::string(1, prefix_) + "_sysmap.wdf"), animations_(assets.resources().names(".ani")) {
    map_ = panel_.rect(kMap);
    const Rect infoArea = panel_.rect(kInfo);
    info_.setOrigin({infoArea.x, infoArea.y});
    system_.setOrigin({infoArea.x, infoArea.y});
    for (const std::uint32_t id : {kTurn, kSummary, kEconomy, kMilitary, kEvents, kOverlay, 0x40eu, 0x40fu}) {
        panel_.setEnabled(id, false);
    }
    for (std::size_t i = 0; i < std::size(text::kObjectKinds); ++i) {
        kindNames_.push_back(text::kObjectKinds[i] != 0 ? assets.lexicon()[text::kObjectKinds[i]] : "Wormhole");
    }
    const int home = game_.galaxy.homes[static_cast<std::size_t>(game_.player)];
    if (home >= 0) {
        chosen_ = game_.galaxy.systems[static_cast<std::size_t>(home)].sector;
    }
    keepInView();
}

int GalaxyScreen::sectorSize() const {
    if (!whole_) {
        return kCloseSector;
    }
    const int fit = std::min(map_.w / std::max(1, game_.galaxy.width), map_.h / std::max(1, game_.galaxy.height));
    return std::clamp(fit, 4, kCloseSector);
}

void GalaxyScreen::keepInView() {
    const int size = sectorSize();
    const int cols = map_.w / size;
    const int rows = map_.h / size;
    auto fit = [](int origin, int chosen, int visible, int total) {
        if (total <= visible) {
            return 0;
        }
        // Keep a sector of room around the chosen one where possible.
        origin = std::min(origin, chosen - 1);
        origin = std::max(origin, chosen + 2 - visible);
        return std::clamp(origin, 0, total - visible);
    };
    origin_.x = fit(origin_.x, chosen_.x, cols, game_.galaxy.width);
    origin_.y = fit(origin_.y, chosen_.y, rows, game_.galaxy.height);
}

std::optional<Point> GalaxyScreen::sectorCentre(Sector s) const {
    const int size = sectorSize();
    const int cols = std::min(game_.galaxy.width, map_.w / size);
    const int rows = std::min(game_.galaxy.height, map_.h / size);
    const Sector o = whole_ ? Sector{} : origin_;
    if (s.x < o.x || s.y < o.y || s.x >= o.x + cols || s.y >= o.y + rows) {
        return std::nullopt;
    }
    const int left = map_.x + (map_.w - cols * size) / 2;
    const int top = map_.y + (map_.h - rows * size) / 2;
    return Point{left + (s.x - o.x) * size + size / 2, top + (s.y - o.y) * size + size / 2};
}

std::optional<Sector> GalaxyScreen::sectorAt(Point p) const {
    if (!map_.contains(p.x, p.y)) {
        return std::nullopt;
    }
    const int size = sectorSize();
    const int cols = std::min(game_.galaxy.width, map_.w / size);
    const int rows = std::min(game_.galaxy.height, map_.h / size);
    const Sector o = whole_ ? Sector{} : origin_;
    const int left = map_.x + (map_.w - cols * size) / 2;
    const int top = map_.y + (map_.h - rows * size) / 2;
    if (p.x < left || p.y < top || p.x >= left + cols * size || p.y >= top + rows * size) {
        return std::nullopt;
    }
    return Sector{o.x + (p.x - left) / size, o.y + (p.y - top) / size};
}

std::string GalaxyScreen::sectorName(Sector s) const {
    return format1(assets_.lexicon()[text::kSector], std::to_string(s.x + 1) + "." + std::to_string(s.y + 1));
}

void GalaxyScreen::choose(Sector s) {
    if (game_.galaxy.inside(s)) {
        chosen_ = s;
        keepInView();
    }
}

void GalaxyScreen::draw(Canvas& canvas, std::uint64_t ms) {
    panel_.draw(canvas);
    const Galaxy& g = game_.galaxy;
    const int size = sectorSize();
    const int player = std::clamp(game_.player, 0, kEmpires - 1);
    const Font& small = assets_.font(kSmallFonts[player]);
    const Font& font = assets_.font(kFonts[player]);

    canvas.setClip(map_);
    // The sector grid.
    for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
            if (const auto c = sectorCentre({x, y})) {
                canvas.frame({c->x - size / 2, c->y - size / 2, size + 1, size + 1}, kGrid);
            }
        }
    }
    // Stars and other objects, then the names of the stars.
    for (const auto& s : g.systems) {
        const auto c = sectorCentre(s.sector);
        if (!c) {
            continue;
        }
        const std::string code = static_cast<std::size_t>(s.kind) < game_.kindCodes.size()
                                     ? game_.kindCodes[static_cast<std::size_t>(s.kind)]
                                     : std::string();
        std::string picture = code + (whole_ ? "maps.tga" : "map.tga");
        if (code == "worm") {
            picture = whole_ ? "" : "wh_30.tga";
        } else if (code == "rpls") {
            picture = std::string("xp-") + (whole_ ? "maps.tga" : "map.tga");
        }
        const Image& img = assets_.image(picture);
        if (img.width > 0) {
            canvas.draw(img, c->x - img.width / 2, c->y - img.height / 2);
        } else {
            canvas.fill({c->x - 1, c->y - 1, 3, 3}, rgb(255, 255, 255));
        }
    }
    if (!whole_) {
        for (const auto& s : g.systems) {
            const auto c = sectorCentre(s.sector);
            if (!c || !s.star) {
                continue;
            }
            Pixel colour = kNameColour;
            if (s.race >= 0 && s.race < kEmpires) {
                colour = kEmpireColours[s.race];
            } else if (s.race >= kEmpires) {
                colour = kMinorColour;
            }
            const int w = small.width(s.name);
            canvas.text(small, s.name, c->x - w / 2, c->y + size / 2 - small.height() + 2, colour);
        }
    }
    if (const auto c = sectorCentre(chosen_)) {
        canvas.frame({c->x - size / 2, c->y - size / 2, size + 1, size + 1}, kChosen);
    }
    canvas.clearClip();

    // Around the map: the empire, the turn and the chosen sector.
    const Rect title = panel_.rect(kTitle);
    if (static_cast<std::size_t>(player) < game_.raceNames.size()) {
        canvas.text(font, game_.raceNames[static_cast<std::size_t>(player)], title, kInfoColour, Align::Center);
    }
    canvas.text(font, assets_.lexicon()[text::kTurn] + " " + std::to_string(game_.turn), panel_.rect(kTurnText),
                kInfoColour, Align::Center);

    // Below the map: the chosen sector and what is in it.
    const int at = g.systemAt(chosen_);
    if (at >= 0 && !g.systems[static_cast<std::size_t>(at)].planets.empty()) {
        drawSystem(canvas, g.systems[static_cast<std::size_t>(at)], ms);
    } else {
        drawSector(canvas, at);
    }
    drawList(canvas);
}

void GalaxyScreen::drawSector(Canvas& canvas, int at) {
    const Galaxy& g = game_.galaxy;
    const int player = std::clamp(game_.player, 0, kEmpires - 1);
    const Font& small = assets_.font(kSmallFonts[player]);
    const Font& font = assets_.font(kFonts[player]);
    info_.draw(canvas);
    canvas.text(small, sectorName(chosen_), info_.rect(kInfoSector), kInfoColour, Align::Center);
    const Rect details = info_.rect(kInfoDetails);
    int y = details.y + 4;
    if (at >= 0) {
        const auto& s = g.systems[static_cast<std::size_t>(at)];
        const std::string kind = static_cast<std::size_t>(s.kind) < kindNames_.size() ? kindNames_[static_cast<std::size_t>(s.kind)] : "";
        if (!s.name.empty()) {
            canvas.text(font, s.name, details.x + 6, y, kInfoColour);
            y += font.height() + 2;
        }
        canvas.text(small, kind, details.x + 6, y, kNameColour);
        y += small.height() + 4;
        if (s.race >= 0 && static_cast<std::size_t>(s.race) < game_.raceNames.size()) {
            canvas.text(font, game_.raceNames[static_cast<std::size_t>(s.race)], details.x + 6, y,
                        s.race < kEmpires ? kEmpireColours[s.race] : kMinorColour);
        }
    }
}

void GalaxyScreen::drawSystem(Canvas& canvas, const StarSystem& s, std::uint64_t ms) {
    const int player = std::clamp(game_.player, 0, kEmpires - 1);
    const Font& small = assets_.font(kSmallFonts[player]);
    const Font& font = assets_.font(kFonts[player]);
    system_.draw(canvas);
    canvas.text(small, sectorName(chosen_), system_.rect(kInfoSector), kInfoColour, Align::Center);
    const Pixel nameColour = s.race >= 0 && s.race < kEmpires ? kEmpireColours[s.race] : s.race >= 0 ? kMinorColour : kInfoColour;
    canvas.text(small, s.name, system_.rect(kInfoName), nameColour, Align::Center);

    // On the left: what kind of star, and who lives here.
    const Rect details = system_.rect(kSystemDetails);
    int y = details.y + 2;
    const std::string kind = static_cast<std::size_t>(s.kind) < kindNames_.size() ? kindNames_[static_cast<std::size_t>(s.kind)] : "";
    canvas.text(small, kind, details.x + 4, y, kNameColour);
    y += small.height() + 3;
    if (s.race >= 0 && static_cast<std::size_t>(s.race) < game_.raceNames.size()) {
        canvas.text(font, game_.raceNames[static_cast<std::size_t>(s.race)], details.x + 4, y, nameColour);
    }

    // The star on the left of the strip, its planets in a row to its right.
    const Rect strip = system_.rect(kInfoPicture);
    canvas.setClip(strip);
    const int frame = static_cast<int>(ms / kFrameMs);
    const std::string code = static_cast<std::size_t>(s.kind) < game_.kindCodes.size() ? game_.kindCodes[static_cast<std::size_t>(s.kind)] : "";
    const Animation& star = assets_.animation(code + ".ani");
    if (star.frames > 0) {
        canvas.draw(star.sheet, star.part(frame), strip.x - star.frameWidth / 2, strip.y + (strip.h - star.frameHeight) / 2);
    }
    const int left = strip.x + 60;
    const int step = (strip.w - 60) / static_cast<int>(s.planets.size());
    for (std::size_t i = 0; i < s.planets.size(); ++i) {
        const Planet& p = s.planets[i];
        const int cx = left + step * static_cast<int>(i) + step / 2;
        const int cy = strip.y + (strip.h - small.height()) / 2;
        const Animation& a = assets_.animation(planetAnimation(p, animations_));
        if (a.frames > 0) {
            // Each planet turns at its own pace.
            canvas.draw(a.sheet, a.part(frame + static_cast<int>(i) * 5), cx - a.frameWidth / 2, cy - a.frameHeight / 2);
        } else {
            canvas.fill({cx - 4, cy - 4, 8, 8}, kNameColour);
        }
        const int w = small.width(p.name);
        canvas.text(small, p.name, cx - w / 2, strip.y + strip.h - small.height() - 1, kNameColour);
    }
    canvas.clearClip();
}

void GalaxyScreen::drawList(Canvas& canvas) {
    const Galaxy& g = game_.galaxy;
    const int player = std::clamp(game_.player, 0, kEmpires - 1);
    const Font& small = assets_.font(kSmallFonts[player]);

    // The empire's systems, in the list on the left.
    const Rect list = panel_.rect(kList);
    int ly = list.y + 30;
    for (const auto& s : g.systems) {
        if (s.race == game_.player) {
            canvas.text(small, s.name, list.x + 10, ly, kEmpireColours[player]);
            ly += small.height() + 3;
        }
    }
}

void GalaxyScreen::pointerMoved(Point p) {
    pointer_ = p;
    panel_.pointerMoved(p);
}

void GalaxyScreen::pointerPressed(Point p) {
    pointer_ = p;
    if (const auto s = sectorAt(p)) {
        choose(*s);
        return;
    }
    panel_.pointerPressed(p);
}

void GalaxyScreen::pointerReleased(Point p) {
    pointer_ = p;
    if (const auto id = panel_.pointerReleased(p); id && *id == kWholeMap) {
        whole_ = !whole_;
        keepInView();
    }
}

std::optional<Point> GalaxyScreen::command(Command c) {
    const bool onMap = map_.contains(pointer_.x, pointer_.y);
    Sector next = chosen_;
    switch (c) {
    case Command::Up: next.y -= 1; break;
    case Command::Down: next.y += 1; break;
    case Command::Left: next.x -= 1; break;
    case Command::Right: next.x += 1; break;
    case Command::Next:
    case Command::Previous: {
        // Jumps between the homes and systems of known races.
        std::vector<Sector> marks;
        for (const auto& s : game_.galaxy.systems) {
            if (s.race >= 0) {
                marks.push_back(s.sector);
            }
        }
        if (marks.empty()) {
            return std::nullopt;
        }
        std::size_t i = 0;
        while (i < marks.size() && !(marks[i] == chosen_)) {
            ++i;
        }
        i = i == marks.size() ? 0 : (c == Command::Next ? i + 1 : i + marks.size() - 1) % marks.size();
        choose(marks[i]);
        return sectorCentre(chosen_);
    }
    default: return std::nullopt;
    }
    if (!onMap) {
        return panel_.step(c, pointer_);
    }
    choose(next);
    return sectorCentre(chosen_);
}

} // namespace nagus
