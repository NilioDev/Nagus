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
constexpr std::uint64_t kFrameMs = 90;
constexpr Pixel kMoveColour = 0xff40ff40u;
constexpr Pixel kPathColour = 0xff80c0ffu; // how long each frame of a turning planet shows

std::string format1(const std::string& pattern, const std::string& value) {
    std::string out = pattern;
    auto at = out.find("%s");
    if (at == std::string::npos) {
        at = out.find("%d");
    }
    if (at != std::string::npos) {
        out.replace(at, 2, value);
    }
    return out;
}

constexpr Pixel kBadColour = 0xffff4040u;

} // namespace

void drawPlanetStrip(Canvas& canvas, Assets& assets, const Game& game, const StarSystem& s, const Rect& strip,
                     const Font& small, std::uint64_t ms, const std::vector<std::string>& animations) {
    // The star on the left of the strip, its planets in a row to its right.
    canvas.setClip(strip);
    const int frame = static_cast<int>(ms / kFrameMs);
    const std::string code = static_cast<std::size_t>(s.kind) < game.kindCodes.size() ? game.kindCodes[static_cast<std::size_t>(s.kind)] : "";
    const Animation& star = assets.animation(code + ".ani");
    if (star.frames > 0) {
        canvas.draw(star.sheet, star.part(frame), strip.x - star.frameWidth / 2, strip.y + (strip.h - star.frameHeight) / 2);
    }
    if (s.planets.empty()) {
        canvas.clearClip();
        return;
    }
    const int left = strip.x + 60;
    const int step = (strip.w - 60) / static_cast<int>(s.planets.size());
    for (std::size_t i = 0; i < s.planets.size(); ++i) {
        const Planet& p = s.planets[i];
        const int cx = left + step * static_cast<int>(i) + step / 2;
        const int cy = strip.y + (strip.h - small.height()) / 2;
        const Animation& a = assets.animation(planetAnimation(p, animations));
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

GalaxyScreen::GalaxyScreen(Assets& assets, Game& game)
    : assets_(assets), game_(game), prefix_(kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]),
      panel_(assets, std::string(1, prefix_) + "maingal.wdf"), info_(assets, std::string(1, prefix_) + "_starmp.wdf"),
      system_(assets, std::string(1, prefix_) + "_sysmap.wdf"), animations_(assets.resources().names(".ani")),
      fleetPanel_(assets, std::string(1, prefix_) + "_tfship.wdf"), orders_(assets, std::string(1, prefix_) + "_tford.wdf") {
    map_ = panel_.rect(kMap);
    const Rect infoArea = panel_.rect(kInfo);
    info_.setOrigin({infoArea.x, infoArea.y});
    system_.setOrigin({infoArea.x, infoArea.y});
    fleetPanel_.setOrigin({infoArea.x, infoArea.y});
    fleetPanel_.setEnabled(kFleetCloak, false);
    fleetPanel_.setEnabled(kFleetRedeploy, false);
    // The orders menu opens above the Orders button.
    const Rect ordersButton = fleetPanel_.rect(kFleetOrders);
    orders_.setOrigin({ordersButton.x - 40, infoArea.y - 260});
    for (const auto& e : orders_.layout().elements) {
        orders_.setEnabled(e.id, false);
    }
    for (const std::uint32_t id : {kSummary, kEconomy, kMilitary, kEvents, kOverlay, 0x40eu, 0x40fu}) {
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
    drawFleets(canvas);
    if (const auto c = sectorCentre(chosen_)) {
        canvas.frame({c->x - size / 2, c->y - size / 2, size + 1, size + 1}, moving_ ? kMoveColour : kChosen);
    }
    canvas.clearClip();

    // Around the map: the empire, the turn and the chosen sector.
    const Rect title = panel_.rect(kTitle);
    if (static_cast<std::size_t>(player) < game_.raceNames.size()) {
        canvas.text(font, game_.raceNames[static_cast<std::size_t>(player)], title, kInfoColour, Align::Center);
    }
    canvas.text(font, assets_.lexicon()[text::kTurn] + " " + std::to_string(game_.turn), panel_.rect(kTurnText),
                kInfoColour, Align::Center);

    // Below the map: the chosen fleet, or the chosen sector and what is in it.
    const int at = g.systemAt(chosen_);
    if (game_.fleet(fleet_) != nullptr) {
        drawFleetPanel(canvas);
    } else if (at >= 0 && !g.systems[static_cast<std::size_t>(at)].planets.empty()) {
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

    drawPlanetStrip(canvas, assets_, game_, s, system_.rect(kInfoPicture), small, ms, animations_);
}

void GalaxyScreen::drawList(Canvas& canvas) {
    const Galaxy& g = game_.galaxy;
    const int player = std::clamp(game_.player, 0, kEmpires - 1);
    const Font& small = assets_.font(kSmallFonts[player]);

    // The fleets seen in the chosen sector, then the empire's systems, in
    // the list on the left.
    const Rect list = panel_.rect(kList);
    const auto here = fleetsHere();
    for (int i = 0; i < static_cast<int>(here.size()); ++i) {
        const auto r = fleetLine(i);
        const Fleet* f = game_.fleet(here[static_cast<std::size_t>(i)]);
        if (!r || f == nullptr) {
            continue;
        }
        if (f->id == fleet_) {
            canvas.fill(*r, 0xff304080u);
        }
        std::string name = fleetName(*f);
        canvas.text(small, name, r->x + 4, r->y + 2, f->owner >= 0 && f->owner < kEmpires ? kEmpireColours[f->owner] : kMinorColour);
    }
    int ly = list.y + 30 + static_cast<int>(here.size()) * kFleetLine + (here.empty() ? 0 : small.height());
    for (const auto& c : game_.colonies) {
        if (c.owner == game_.player) {
            canvas.text(small, g.systems[static_cast<std::size_t>(c.system)].name, list.x + 10, ly, kEmpireColours[player]);
            ly += small.height() + 3;
        }
    }
    // The credits, and what happened last turn.
    ly += small.height();
    const auto& empire = game_.empires[static_cast<std::size_t>(player)];
    canvas.text(small, assets_.lexicon()[text::kCredits] + ": " + std::to_string(empire.credits), list.x + 10, ly, kInfoColour);
    ly += small.height() * 2;
    for (const auto& e : events_) {
        if (ly + small.height() > list.y + list.h) {
            break;
        }
        drawParagraph(canvas, small, e, {list.x + 10, ly, list.w - 16, small.height() * 2 + 2}, kNameColour);
        ly += small.height() * (small.width(e) > list.w - 16 ? 2 : 1) + 3;
    }
}

void GalaxyScreen::pointerMoved(Point p) {
    pointer_ = p;
    panel_.pointerMoved(p);
    if (game_.fleet(fleet_) != nullptr) {
        fleetPanel_.pointerMoved(p);
    }
    if (ordersOpen_) {
        orders_.pointerMoved(p);
    }
}

std::string GalaxyScreen::fleetName(const Fleet& f) const {
    if (f.ships.empty()) {
        return {};
    }
    const int cls = f.ships.front().shipClass;
    std::string name = cls >= 0 && static_cast<std::size_t>(cls) < game_.economy.ships.size()
                           ? game_.economy.ships[static_cast<std::size_t>(cls)].name
                           : std::string("?");
    if (f.ships.size() > 1) {
        name += " +" + std::to_string(f.ships.size() - 1);
    }
    return name;
}

std::vector<int> GalaxyScreen::fleetsHere() const {
    std::vector<int> ids;
    if (!visibleTo(game_, game_.player, chosen_)) {
        return ids;
    }
    // The player's first.
    for (const int id : game_.fleetsAt(chosen_, game_.player)) {
        ids.push_back(id);
    }
    for (const int id : game_.fleetsAt(chosen_)) {
        if (game_.fleet(id)->owner != game_.player) {
            ids.push_back(id);
        }
    }
    return ids;
}

std::optional<Rect> GalaxyScreen::fleetLine(int i) const {
    const Rect list = panel_.rect(kList);
    const Rect r{list.x + 6, list.y + 30 + i * kFleetLine, list.w - 12, kFleetLine};
    if (i < 0 || i >= static_cast<int>(fleetsHere().size()) || r.y + r.h > list.y + list.h) {
        return std::nullopt;
    }
    return r;
}

void GalaxyScreen::chooseFleet(int id) {
    fleet_ = id;
    ordersOpen_ = false;
    const Fleet* f = game_.fleet(id);
    moving_ = f != nullptr && f->owner == game_.player;
    refreshFleet();
}

void GalaxyScreen::refreshFleet() {
    const Fleet* f = game_.fleet(fleet_);
    const bool mine = f != nullptr && f->owner == game_.player;
    fleetPanel_.setEnabled(kFleetOrders, mine);
    orders_.setEnabled(kOrderColonize, mine && canColonize(*f, game_.galaxy, game_.colonies, game_.economy));
}

std::optional<Rect> GalaxyScreen::orderButton(std::uint32_t id) const {
    if (!ordersOpen_ || orders_.layout().find(id) == nullptr) {
        return std::nullopt;
    }
    return orders_.rect(id);
}

bool GalaxyScreen::cancel() {
    if (ordersOpen_) {
        ordersOpen_ = false;
        return true;
    }
    if (moving_) {
        moving_ = false;
        return true;
    }
    if (fleet_ >= 0) {
        fleet_ = -1;
        return true;
    }
    return false;
}

void GalaxyScreen::drawFleets(Canvas& canvas) {
    const int size = sectorSize();
    // Where the player's fleets are going.
    for (const auto& f : game_.fleets) {
        if (f.owner != game_.player || !f.destination) {
            continue;
        }
        for (const Sector s : path(f.at, *f.destination)) {
            if (const auto c = sectorCentre(s)) {
                canvas.fill({c->x - 1, c->y - 1, 3, 3}, f.id == fleet_ ? kMoveColour : kPathColour);
            }
        }
    }
    // While sending a fleet: the way to the sector under the pointer.
    if (const Fleet* f = game_.fleet(fleet_); moving_ && f != nullptr) {
        const auto target = sectorAt(pointer_);
        if (target) {
            const bool ok = distanceFromColonies(*target, f->owner, game_.colonies, game_.galaxy) <=
                            fleetRange(*f, game_.economy, game_.empires[static_cast<std::size_t>(f->owner)]);
            for (const Sector s : path(f->at, *target)) {
                if (const auto c = sectorCentre(s)) {
                    canvas.fill({c->x - 1, c->y - 1, 3, 3}, ok ? kMoveColour : kBadColour);
                }
            }
        }
    }
    // A small flag in the sector's top left corner for each empire with
    // fleets there (those the player can see).
    for (const auto& f : game_.fleets) {
        if (f.owner != game_.player && !visibleTo(game_, game_.player, f.at)) {
            continue;
        }
        const auto c = sectorCentre(f.at);
        if (!c) {
            continue;
        }
        const int w = std::max(3, size / 4);
        const int x = c->x - size / 2 + 2 + (f.owner == game_.player ? 0 : w + 1);
        const int y = c->y - size / 2 + 2;
        const Pixel colour = f.owner >= 0 && f.owner < kEmpires ? kEmpireColours[f.owner] : kMinorColour;
        for (int row = 0; row < w; ++row) {
            canvas.fill({x, y + row, w - row, 1}, colour);
        }
        if (f.id == fleet_) {
            canvas.frame({x - 1, y - 1, w + 2, w + 2}, kMoveColour);
        }
    }
}

void GalaxyScreen::drawFleetPanel(Canvas& canvas) {
    const Fleet* f = game_.fleet(fleet_);
    const int player = std::clamp(game_.player, 0, kEmpires - 1);
    const Font& small = assets_.font(kSmallFonts[player]);
    const Font& font = assets_.font(kFonts[player]);
    const Lexicon& lex = assets_.lexicon();
    fleetPanel_.draw(canvas);
    // The ships, each with its picture, name and hull.
    const Rect area = fleetPanel_.rect(kFleetShips);
    const Rect text = fleetPanel_.layout().elements.empty() ? area : fleetPanel_.rect(kFleetText);
    const int cell = 110;
    for (std::size_t i = 0; i < f->ships.size(); ++i) {
        const int x = area.x + 6 + static_cast<int>(i) * cell;
        if (x + cell > text.x) {
            break;
        }
        const int cls = f->ships[i].shipClass;
        if (cls < 0 || static_cast<std::size_t>(cls) >= game_.economy.ships.size()) {
            continue;
        }
        const ShipClass& sc = game_.economy.ships[static_cast<std::size_t>(cls)];
        const Image& pic = assets_.image("i_" + sc.picture + "60.tga");
        canvas.draw(pic, x + (cell - pic.width) / 2, area.y + 4);
        const int w = small.width(sc.name);
        canvas.text(small, sc.name, x + (cell - w) / 2, area.y + 58, kInfoColour);
        const Rect hull{x + 15, area.y + 72, cell - 30, 4};
        canvas.fill(hull, 0xff303848u);
        canvas.fill({hull.x, hull.y, sc.hull > 0 ? hull.w * f->ships[i].hull / sc.hull : 0, hull.h}, 0xff40e070u);
    }
    // About the fleet: whose, how fast and how far, where it goes.
    int y = text.y + 18;
    const std::string owner = f->owner >= 0 && static_cast<std::size_t>(f->owner) < game_.raceNames.size()
                                  ? game_.raceNames[static_cast<std::size_t>(f->owner)]
                                  : std::string();
    canvas.text(font, owner, text.x + 6, y, f->owner >= 0 && f->owner < kEmpires ? kEmpireColours[f->owner] : kMinorColour);
    y += font.height() + 4;
    if (f->owner == game_.player) {
        const auto& empire = game_.empires[static_cast<std::size_t>(f->owner)];
        canvas.text(small, "Speed " + std::to_string(fleetSpeed(*f, game_.economy, empire)) + "   Range " +
                               std::to_string(fleetRange(*f, game_.economy, empire)),
                    text.x + 6, y, kNameColour);
        y += small.height() + 4;
        if (f->destination) {
            const int turns = std::max(1, (distance(f->at, *f->destination) + std::max(1, fleetSpeed(*f, game_.economy, empire)) - 1) /
                                              std::max(1, fleetSpeed(*f, game_.economy, empire)));
            canvas.text(small, sectorName(*f->destination) + "  " + (turns == 1 ? lex[text::kOneTurn] : format1(lex[text::kTurns], std::to_string(turns))),
                        text.x + 6, y, kInfoColour);
        } else if (f->order == Fleet::Order::Colonize) {
            canvas.text(small, lex[656], text.x + 6, y, kInfoColour);
        }
    }
    if (ordersOpen_) {
        orders_.draw(canvas);
    }
}

int GalaxyScreen::chosenColony() const {
    const int at = game_.galaxy.systemAt(chosen_);
    for (std::size_t i = 0; at >= 0 && i < game_.colonies.size(); ++i) {
        if (game_.colonies[i].system == at && game_.colonies[i].owner == game_.player) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void GalaxyScreen::pointerPressed(Point p) {
    pointer_ = p;
    if (ordersOpen_) {
        orders_.pointerPressed(p);
        return;
    }
    // A fleet in the list on the left.
    const auto here = fleetsHere();
    for (int i = 0; i < static_cast<int>(here.size()); ++i) {
        if (const auto r = fleetLine(i); r && r->contains(p.x, p.y)) {
            chooseFleet(here[static_cast<std::size_t>(i)]);
            return;
        }
    }
    // Sending the chosen fleet.
    if (const Fleet* f = game_.fleet(fleet_); moving_ && f != nullptr) {
        if (const auto s = sectorAt(p)) {
            if (*s == f->at) {
                game_.fleet(fleet_)->destination.reset();
                moving_ = false;
            } else if (orderMove(game_, fleet_, *s)) {
                moving_ = false;
            }
            choose(*s);
            return;
        }
    }
    if (game_.fleet(fleet_) != nullptr && panel_.rect(kInfo).contains(p.x, p.y)) {
        fleetPanel_.pointerPressed(p);
        return;
    }
    if (const auto s = sectorAt(p)) {
        fleet_ = -1;
        // A second click on one of the player's systems opens it.
        if (*s == chosen_ && chosenColony() >= 0) {
            outcome_ = Outcome::Open;
        }
        choose(*s);
        return;
    }
    if (panel_.rect(kInfo).contains(p.x, p.y) && chosenColony() >= 0) {
        outcome_ = Outcome::Open;
        return;
    }
    panel_.pointerPressed(p);
}

void GalaxyScreen::pointerReleased(Point p) {
    pointer_ = p;
    if (ordersOpen_) {
        if (const auto order = orders_.pointerReleased(p)) {
            if (*order == kOrderColonize) {
                orderColonize(game_, fleet_);
            }
            ordersOpen_ = false;
        }
        return;
    }
    if (game_.fleet(fleet_) != nullptr) {
        if (const auto b = fleetPanel_.pointerReleased(p); b && *b == kFleetOrders) {
            refreshFleet();
            ordersOpen_ = true;
            moving_ = false;
            return;
        }
    }
    const auto id = panel_.pointerReleased(p);
    if (id && *id == kWholeMap) {
        whole_ = !whole_;
        keepInView();
    } else if (id && *id == kTurn) {
        const TurnSummary summary = endTurn(game_);
        events_ = summary.events;
        for (const auto& t : summary.researched) {
            events_.push_back(assets_.lexicon()[text::kResearch] + ": " + t);
        }
        for (const auto& s : summary.colonized) {
            events_.push_back(format1(assets_.lexicon()[text::kColonized], s));
        }
        if (game_.fleet(fleet_) == nullptr) {
            fleet_ = -1;
            moving_ = false;
        }
        refreshFleet();
    }
}

std::optional<Point> GalaxyScreen::command(Command c) {
    if (ordersOpen_) {
        return orders_.step(c, pointer_);
    }
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
        // The fleet lines on the left, the buttons, and the fleet panel's.
        std::vector<Point> spots;
        if (const auto p = panel_.step(c, pointer_)) {
            spots.push_back(*p);
        }
        if (game_.fleet(fleet_) != nullptr) {
            if (const auto p = fleetPanel_.step(c, pointer_)) {
                spots.push_back(*p);
            }
        }
        for (int i = 0; i < static_cast<int>(fleetsHere().size()); ++i) {
            if (const auto r = fleetLine(i)) {
                const Point q{r->x + 20, r->centerY()};
                const int dx = q.x - pointer_.x;
                const int dy = q.y - pointer_.y;
                const bool ahead = (c == Command::Up && dy < 0) || (c == Command::Down && dy > 0) ||
                                   (c == Command::Left && dx < 0) || (c == Command::Right && dx > 0);
                if (ahead) {
                    spots.push_back(q);
                }
            }
        }
        std::optional<Point> best;
        for (const Point q : spots) {
            auto d = [&](Point a) { return std::abs(a.x - pointer_.x) + std::abs(a.y - pointer_.y); };
            if (!best || d(q) < d(*best)) {
                best = q;
            }
        }
        return best;
    }
    choose(next);
    return sectorCentre(chosen_);
}

} // namespace nagus
