// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/galaxy_screen.hpp"

#include "nagus/game/combat.hpp"
#include "nagus/game/tactical.hpp"
#include "nagus/game/trade.hpp"
#include "nagus/ui/diplomacy_screen.hpp"
#include "nagus/ui/intel_screen.hpp"

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
    if (at == std::string::npos) {
        at = out.find("%1");
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
        // Planets that hold nobody until terraformed are dimmed.
        canvas.text(small, p.name, cx - w / 2, strip.y + strip.h - small.height() - 1,
                    p.terraformed || p.room == 0 ? kNameColour : 0xff707888u);
    }
    canvas.clearClip();
}

GalaxyScreen::GalaxyScreen(Assets& assets, Game& game)
    : assets_(assets), game_(game), prefix_(kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]),
      panel_(assets, std::string(1, prefix_) + "maingal.wdf"), info_(assets, std::string(1, prefix_) + "_starmp.wdf"),
      system_(assets, std::string(1, prefix_) + "_sysmap.wdf"), animations_(assets.resources().names(".ani")),
      fleetPanel_(assets, std::string(1, prefix_) + "_tfship.wdf"), orders_(assets, std::string(1, prefix_) + "_tford.wdf"),
      overlay_(assets, std::string(1, prefix_) + "_ovrlay.wdf"), routes_(assets, std::string(1, prefix_) + "inf_tr.wdf") {
    // Each mode's layers to begin with.
    auto bits = [](std::initializer_list<Layer> ls) {
        std::uint32_t b = 0;
        for (const Layer l : ls) {
            b |= 1u << static_cast<int>(l);
        }
        return b;
    };
    layers_ = {bits({Layer::Names, Layer::Territory, Layer::Building, Layer::Population, Layer::TradeRoutes}),
               bits({Layer::Names, Layer::Territory, Layer::Ranges, Layer::Paths, Layer::Defences}),
               bits({Layer::Names, Layer::Territory, Layer::Paths})};
    // There are no sector bonuses yet.
    overlay_.setEnabled(kLayerButtons[static_cast<int>(Layer::Bonuses)], false);
    const Rect ob = panel_.rect(kOverlay);
    overlay_.setOrigin({ob.x, ob.y - overlay_.layout().rect.h - 4});
    map_ = panel_.rect(kMap);
    const Rect infoArea = panel_.rect(kInfo);
    info_.setOrigin({infoArea.x, infoArea.y});
    system_.setOrigin({infoArea.x, infoArea.y});
    fleetPanel_.setOrigin({infoArea.x, infoArea.y});
    fleetPanel_.setEnabled(kFleetCloak, false);
    // The orders menu opens above the Orders button.
    const Rect ordersButton = fleetPanel_.rect(kFleetOrders);
    orders_.setOrigin({ordersButton.x - 40, infoArea.y - 260});
    for (const auto& e : orders_.layout().elements) {
        orders_.setEnabled(e.id, false);
    }
    for (const std::uint32_t id : {kListUp, kListDown}) {
        panel_.setEnabled(id, false);
    }
    refreshModes();
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

std::string GalaxyScreen::battleText(const Battle& b) const {
    int had = 0;
    int lost = 0;
    int enemyHad = 0;
    int enemyLost = 0;
    for (std::size_t i = 0; i < b.owners.size(); ++i) {
        (b.owners[i] == game_.player ? had : enemyHad) += b.ships[i];
        (b.owners[i] == game_.player ? lost : enemyLost) += b.lost[i];
    }
    return format1(assets_.lexicon()[text::kCombatIn], std::to_string(b.at.x + 1) + "." + std::to_string(b.at.y + 1)) + ", lost " + std::to_string(lost) +
           " of " + std::to_string(had) + ", enemy lost " + std::to_string(enemyLost) + " of " + std::to_string(enemyHad);
}

std::string GalaxyScreen::assaultText(const Assault& a) const {
    const std::string& where = game_.galaxy.systems[static_cast<std::size_t>(a.system)].name;
    const int other = a.attacker == game_.player ? a.defender : a.attacker;
    const std::string them = other >= 0 && static_cast<std::size_t>(other) < game_.raceNames.size()
                                 ? game_.raceNames[static_cast<std::size_t>(other)]
                                 : std::string();
    if (a.captured) {
        return a.attacker == game_.player ? where + " taken from the " + them : where + " lost to the " + them;
    }
    const std::string killed = std::to_string(a.killed) + " million killed";
    return a.attacker == game_.player ? where + " attacked, " + killed : where + " attacked by the " + them + ", " + killed;
}

std::string GalaxyScreen::sectorName(Sector s) const {
    return format1(assets_.lexicon()[text::kSector], std::to_string(s.x + 1) + "." + std::to_string(s.y + 1));
}

void GalaxyScreen::choose(Sector s) {
    if (game_.galaxy.inside(s)) {
        if (!(s == chosen_)) {
            listFirst_ = 0;
        }
        chosen_ = s;
        choosingPartner_ = false;
        keepInView();
        scrollFleets(0);
    }
}

void GalaxyScreen::scrollFleets(int lines) {
    const int most = std::max(0, static_cast<int>(fleetsHere().size()) - kFleetLines);
    listFirst_ = std::clamp(listFirst_ + lines, 0, most);
    panel_.setEnabled(kListUp, listFirst_ > 0);
    panel_.setEnabled(kListDown, listFirst_ < most);
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
    // The empires' space: the sectors around the colonies the player knows of.
    for (const auto& col : game_.colonies) {
        if (!shows(Layer::Territory)) {
            break;
        }
        const Sector at = g.systems[static_cast<std::size_t>(col.system)].sector;
        if (!exploredBy(game_, game_.player, at)) {
            continue;
        }
        const Pixel colour = col.owner >= 0 && col.owner < kEmpires ? kEmpireColours[col.owner] : kMinorColour;
        for (int y = at.y - 1; y <= at.y + 1; ++y) {
            for (int x = at.x - 1; x <= at.x + 1; ++x) {
                const auto c = exploredBy(game_, game_.player, {x, y}) ? sectorCentre({x, y}) : std::nullopt;
                if (c) {
                    canvas.blend({c->x - size / 2 + 1, c->y - size / 2 + 1, size - 1, size - 1}, colour,
                                 x == at.x && y == at.y ? 70 : 35);
                }
            }
        }
    }
    // Stars and other objects the player has found, then the names of the stars.
    for (const auto& s : g.systems) {
        const auto c = sectorCentre(s.sector);
        if (!c || !exploredBy(game_, game_.player, s.sector)) {
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
    if (!whole_ && shows(Layer::Names)) {
        for (const auto& s : g.systems) {
            const auto c = sectorCentre(s.sector);
            if (!c || !s.star || !exploredBy(game_, game_.player, s.sector)) {
                continue;
            }
            Pixel colour = kNameColour;
            const Colony* col = game_.colonyAt(g.systemAt(s.sector));
            if (col != nullptr && col->owner >= 0 && col->owner < kEmpires) {
                colour = kEmpireColours[col->owner];
            } else if (s.race >= 0 && s.race < kEmpires) {
                colour = kEmpireColours[s.race];
            } else if (s.race >= kEmpires) {
                colour = kMinorColour;
            }
            const int w = small.width(s.name);
            canvas.text(small, s.name, c->x - w / 2, c->y + size / 2 - small.height() + 2, colour);
        }
    }
    drawLayers(canvas);
    drawFleets(canvas);
    // Space not explored yet is dark.
    for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
            const auto c = exploredBy(game_, game_.player, {x, y}) ? std::nullopt : sectorCentre({x, y});
            if (c) {
                canvas.blend({c->x - size / 2 + 1, c->y - size / 2 + 1, size - 1, size - 1}, rgb(0, 0, 0), 150);
            }
        }
    }
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
    const int at = exploredBy(game_, game_.player, chosen_) ? g.systemAt(chosen_) : -1;
    if (game_.fleet(fleet_) != nullptr) {
        drawFleetPanel(canvas);
    } else if (mode_ == Mode::Events) {
        drawEvents(canvas);
    } else if (at >= 0 && !g.systems[static_cast<std::size_t>(at)].planets.empty()) {
        drawSystem(canvas, g.systems[static_cast<std::size_t>(at)], ms);
    } else {
        drawSector(canvas, at);
    }
    drawList(canvas);
    if (overlayOpen_) {
        overlay_.draw(canvas);
    }
}

void GalaxyScreen::drawLayers(Canvas& canvas) {
    const Galaxy& g = game_.galaxy;
    const int size = sectorSize();
    const int player = std::clamp(game_.player, 0, kEmpires - 1);
    const Font& small = assets_.font(kSmallFonts[player]);
    const auto& empire = game_.empires[static_cast<std::size_t>(player)];
    // How far the player's ships reach: the edge of each range, short in
    // green, medium in yellow, long in red.
    if (shows(Layer::Ranges)) {
        constexpr Pixel kRangeColours[3] = {0xff40e070u, 0xffe0e040u, 0xffff5050u};
        for (int y = 0; y < g.height; ++y) {
            for (int x = 0; x < g.width; ++x) {
                const auto c = sectorCentre({x, y});
                if (!c) {
                    continue;
                }
                const int d = distanceFromBases(game_, {x, y}, game_.player);
                for (int r = 0; r < 3; ++r) {
                    if (d == rangeSectors(r, empire)) {
                        canvas.frame({c->x - size / 2 + 2, c->y - size / 2 + 2, size - 3, size - 3}, kRangeColours[r]);
                    }
                }
            }
        }
    }
    // Known trade routes: the player's in green, the others' in yellow, as
    // dotted lines between the systems.
    if (shows(Layer::TradeRoutes)) {
        for (const auto& r : game_.tradeRoutes) {
            const Sector a = g.systems[static_cast<std::size_t>(r.from)].sector;
            const Sector b = g.systems[static_cast<std::size_t>(r.to)].sector;
            const Colony* from = game_.colonyAt(r.from);
            const bool mine = (from != nullptr && from->owner == game_.player) || tradePartner(game_, r.to) == game_.player;
            const auto ca = sectorCentre(a);
            const auto cb = sectorCentre(b);
            if (!ca || !cb || (!mine && (!exploredBy(game_, game_.player, a) || !exploredBy(game_, game_.player, b)))) {
                continue;
            }
            const int steps = std::max({1, std::abs(cb->x - ca->x), std::abs(cb->y - ca->y)});
            for (int i = 0; i <= steps; i += 4) {
                canvas.fill({ca->x + (cb->x - ca->x) * i / steps - 1, ca->y + (cb->y - ca->y) * i / steps - 1, 2, 2},
                            mine ? 0xff40e070u : 0xffe0e040u);
            }
        }
    }
    if (whole_) {
        return;
    }
    for (const auto& col : game_.colonies) {
        const Sector at = g.systems[static_cast<std::size_t>(col.system)].sector;
        const auto c = sectorCentre(at);
        if (!c || !exploredBy(game_, game_.player, at)) {
            continue;
        }
        const int top = c->y - size / 2 + 2;
        const int left = c->x - size / 2 + 3;
        // The colony's people, its defences, and what it builds (the
        // player's own only).
        if (shows(Layer::Population)) {
            const std::string pop = std::to_string(col.population);
            canvas.text(small, pop, c->x + size / 2 - small.width(pop) - 3, top, kInfoColour);
        }
        if (shows(Layer::Defences)) {
            const ColonyDefence d = colonyDefence(game_, col);
            if (d.firepower > 0 || d.shields > 0) {
                canvas.text(small, std::to_string(d.firepower) + (d.shields > 0 ? "+" : ""), left + 10, top, kBadColour);
            }
        }
        if (shows(Layer::Building) && col.owner == game_.player && !col.queue.empty()) {
            std::string name = itemName(col.queue.front(), game_.economy);
            while (!name.empty() && small.width(name) > size - 6) {
                name.pop_back();
            }
            canvas.text(small, name, left, top + small.height() + 1, kNameColour);
        }
    }
}

void GalaxyScreen::drawEvents(Canvas& canvas) {
    const int player = std::clamp(game_.player, 0, kEmpires - 1);
    const Font& small = assets_.font(kSmallFonts[player]);
    // In the panel below the map, right of the box on its left.
    info_.draw(canvas);
    const std::string head = assets_.lexicon()[305]; // "EVENT|SECTOR"
    canvas.text(small, head.substr(0, head.find('|')), info_.rect(kInfoSector), kInfoColour, Align::Center);
    const Rect area = panel_.rect(kInfo);
    const Rect box{area.x + 215, area.y + 42, area.w - 245, area.h - 52};
    int y = box.y;
    if (events_.empty()) {
        canvas.text(small, assets_.lexicon()[text::kTurn] + " " + std::to_string(game_.turn) + ": -", box.x, y, kNameColour);
        return;
    }
    for (const auto& [e, colour] : events_) {
        for (const auto& line : wrapText(small, e, box.w)) {
            if (y + small.height() > box.y + box.h) {
                return;
            }
            canvas.text(small, line, box.x, y, colour);
            y += small.height() + 2;
        }
    }
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
    scrollFleets(0);
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
    int ly = list.y + 30 + std::min(static_cast<int>(here.size()), kFleetLines) * kFleetLine + (here.empty() ? 0 : small.height());
    // The chosen colony's trade routes (economic mode).
    if (const auto header = routeHeader()) {
        const LayoutElement& e = *routes_.layout().find(kTradeRoutes);
        canvas.draw(assets_.image(choosingPartner_ && !e.downImage.empty() ? e.downImage : e.upImage), header->x, header->y);
        if (choosingPartner_) {
            canvas.frame({header->x - 1, header->y - 1, header->w + 2, header->h + 2}, kMoveColour);
        }
        const auto routes = routesHere();
        const Colony& c = game_.colonies[static_cast<std::size_t>(chosenColony())];
        for (int i = 0; i < static_cast<int>(routes.size()); ++i) {
            if (const auto r = routeLine(i)) {
                const TradeRoute& tr = routes[static_cast<std::size_t>(i)];
                const int partner = tradePartner(game_, tr.to);
                canvas.text(small, g.systems[static_cast<std::size_t>(tr.to)].name, r->x + 4, r->y + 2,
                            partner >= 0 && partner < kEmpires ? kEmpireColours[partner] : kMinorColour);
                const std::string income = "+" + std::to_string(routeIncome(game_, tr));
                canvas.text(small, income, r->x + r->w - small.width(income) - 4, r->y + 2, kInfoColour);
                ly = r->y + r->h;
            }
        }
        ly = std::max(ly, header->y + header->h + 4);
        const std::string count = "Free routes: " + std::to_string(std::max(0, routeLimit(game_, c) - static_cast<int>(routes.size())));
        canvas.text(small, choosingPartner_ ? "Click a system to trade with" : count, list.x + 10, ly + 2, kNameColour);
        ly += small.height() * 2 + 4;
    }
    for (const auto& c : game_.colonies) {
        if (c.owner == game_.player && ly + small.height() <= list.y + list.h) {
            canvas.text(small, g.systems[static_cast<std::size_t>(c.system)].name, list.x + 10, ly, kEmpireColours[player]);
            ly += small.height() + 3;
        }
    }
    // The credits, and what happened last turn.
    ly += small.height();
    const auto& empire = game_.empires[static_cast<std::size_t>(player)];
    canvas.text(small, assets_.lexicon()[text::kCredits] + ": " + std::to_string(empire.credits), list.x + 10, ly, kInfoColour);
    ly += small.height() * 2;
    for (const auto& [e, colour] : events_) {
        if (ly + small.height() > list.y + list.h) {
            break;
        }
        drawParagraph(canvas, small, e, {list.x + 10, ly, list.w - 16, small.height() * 2 + 2}, colour);
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
    if (overlayOpen_) {
        overlay_.pointerMoved(p);
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
        if (const Fleet* f = game_.fleet(id); f->owner != game_.player && fleetSeen(game_, game_.player, *f)) {
            ids.push_back(id);
        }
    }
    return ids;
}

std::vector<TradeRoute> GalaxyScreen::routesHere() const {
    std::vector<TradeRoute> out;
    if (mode_ != Mode::Economy || chosenColony() < 0) {
        return out;
    }
    const int system = game_.galaxy.systemAt(chosen_);
    for (const auto& r : game_.tradeRoutes) {
        if (r.from == system) {
            out.push_back(r);
        }
    }
    return out;
}

std::optional<Rect> GalaxyScreen::routeHeader() const {
    if (mode_ != Mode::Economy || chosenColony() < 0 || routes_.layout().find(kTradeRoutes) == nullptr) {
        return std::nullopt;
    }
    const Rect list = panel_.rect(kList);
    const int fleets = std::min(static_cast<int>(fleetsHere().size()), kFleetLines);
    const int y = list.y + 30 + fleets * kFleetLine + (fleets > 0 ? 8 : 0);
    const Rect own = routes_.layout().find(kTradeRoutes)->rect;
    return Rect{list.x + (list.w - own.w) / 2, y, own.w, own.h};
}

std::optional<Rect> GalaxyScreen::routeLine(int i) const {
    const auto header = routeHeader();
    const Rect list = panel_.rect(kList);
    if (!header || i < 0 || i >= static_cast<int>(routesHere().size())) {
        return std::nullopt;
    }
    const Rect r{list.x + 6, header->y + header->h + 4 + i * kFleetLine, list.w - 12, kFleetLine};
    if (r.y + r.h > list.y + list.h) {
        return std::nullopt;
    }
    return r;
}

std::optional<Rect> GalaxyScreen::fleetLine(int i) const {
    const Rect list = panel_.rect(kList);
    const int at = i - listFirst_;
    const Rect r{list.x + 6, list.y + 30 + at * kFleetLine, list.w - 12, kFleetLine};
    if (i < 0 || i >= static_cast<int>(fleetsHere().size()) || at < 0 || at >= kFleetLines || r.y + r.h > list.y + list.h) {
        return std::nullopt;
    }
    return r;
}

void GalaxyScreen::chooseFleet(int id) {
    fleet_ = id;
    ordersOpen_ = false;
    redeploy_ = false;
    const Fleet* f = game_.fleet(id);
    moving_ = f != nullptr && f->owner == game_.player;
    refreshFleet();
}

void GalaxyScreen::refreshFleet() {
    const Fleet* f = game_.fleet(fleet_);
    const bool mine = f != nullptr && f->owner == game_.player;
    fleetPanel_.setEnabled(kFleetOrders, mine);
    orders_.setEnabled(kOrderColonize, mine && canColonize(*f, game_.galaxy, game_.colonies, game_.economy));
    orders_.setEnabled(kOrderAttack, mine && canAttack(game_, *f));
    orders_.setEnabled(kOrderRaid, mine && canRaid(game_, *f));
    orders_.setEnabled(kOrderScrap, mine);
    orders_.setEnabled(kOrderAvoid, mine && !f->avoid);
    orders_.setEnabled(kOrderEngage, mine && f->avoid);
    orders_.setEnabled(kOrderDefend, mine && canDefend(game_, *f));
    orders_.setEnabled(kOrderWormhole, mine && canEnterWormhole(game_, *f));
    orders_.setEnabled(kOrderTrain, mine && canTrain(game_, *f));
    // Outpost and starbase share a place: the starbase where there is an
    // outpost of the player's.
    const Station* st = f != nullptr ? game_.stationAt(f->at) : nullptr;
    const bool starbase = st != nullptr && st->owner == game_.player;
    orders_.setVisible(kOrderOutpost, !starbase);
    orders_.setVisible(kOrderStarbase, starbase);
    orders_.setEnabled(kOrderOutpost, mine && canBuildOutpost(game_, *f));
    orders_.setEnabled(kOrderStarbase, mine && canBuildStarbase(game_, *f));
    orders_.setEnabled(kOrderUpgrade, mine && canUpgradeStation(game_, *f));
    orders_.setEnabled(kOrderTerraform, mine && terraformTarget(game_, *f) >= 0);
    fleetPanel_.setEnabled(kFleetCloak, mine && (f->cloaked || canCloak(game_, *f)));
    fleetPanel_.setOn(kFleetCloak, mine && f->cloaked);
    // Redeploying needs a second ship, in this fleet or another one here.
    const bool more = mine && (f->ships.size() > 1 || game_.fleetsAt(f->at, game_.player).size() > 1);
    fleetPanel_.setEnabled(kFleetRedeploy, more);
    redeploy_ = redeploy_ && more;
    fleetPanel_.setOn(kFleetRedeploy, redeploy_);
}

std::optional<Rect> GalaxyScreen::shipCell(int ship) const {
    const Fleet* f = game_.fleet(fleet_);
    if (f == nullptr || ship < 0 || static_cast<std::size_t>(ship) >= f->ships.size()) {
        return std::nullopt;
    }
    const Rect area = fleetPanel_.rect(kFleetShips);
    const Rect text = fleetPanel_.layout().elements.empty() ? area : fleetPanel_.rect(kFleetText);
    const Rect r{area.x + 6 + ship * kShipCell, area.y, kShipCell, 80};
    if (r.x + r.w > text.x) {
        return std::nullopt;
    }
    return r;
}

std::optional<Rect> GalaxyScreen::orderButton(std::uint32_t id) const {
    if (!ordersOpen_ || orders_.layout().find(id) == nullptr) {
        return std::nullopt;
    }
    return orders_.rect(id);
}

void GalaxyScreen::setMode(Mode m) {
    mode_ = m;
    refreshModes();
}

void GalaxyScreen::toggle(Layer l) {
    layers_[static_cast<std::size_t>(mode_)] ^= 1u << static_cast<int>(l);
    refreshModes();
}

void GalaxyScreen::refreshModes() {
    panel_.setOn(kEconomy, mode_ == Mode::Economy);
    panel_.setOn(kMilitary, mode_ == Mode::Military);
    panel_.setOn(kEvents, mode_ == Mode::Events);
    panel_.setOn(kOverlay, overlayOpen_);
    for (int i = 0; i < kLayers; ++i) {
        overlay_.setOn(kLayerButtons[i], shows(static_cast<Layer>(i)));
    }
}

std::optional<Rect> GalaxyScreen::layerButton(Layer l) const {
    const auto id = kLayerButtons[static_cast<int>(l)];
    if (!overlayOpen_ || !overlay_.usable(id)) {
        return std::nullopt;
    }
    return overlay_.rect(id);
}

bool GalaxyScreen::cancel() {
    if (choosingPartner_) {
        choosingPartner_ = false;
        return true;
    }
    if (overlayOpen_) {
        overlayOpen_ = false;
        refreshModes();
        return true;
    }
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
        if (f.owner != game_.player || !f.destination || !shows(Layer::Paths)) {
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
            const bool ok = distanceFromBases(game_, *target, f->owner) <=
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
        if (!fleetSeen(game_, game_.player, f)) {
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
    // Outposts and starbases: their small picture in the sector's bottom
    // right corner, when zoomed in.
    for (const auto& st : game_.stations) {
        const auto c = sectorCentre(st.at);
        if (!c || whole_ || st.shipClass < 0 || static_cast<std::size_t>(st.shipClass) >= game_.economy.ships.size() ||
            (st.owner != game_.player && !visibleTo(game_, game_.player, st.at))) {
            continue;
        }
        const ShipClass& sc = game_.economy.ships[static_cast<std::size_t>(st.shipClass)];
        const Image& pic = assets_.image("i_" + sc.picture + (sc.role == ShipRole::Outpost ? "11.tga" : ".tga"));
        canvas.draw(pic, c->x + size / 2 - pic.width - 2, c->y + size / 2 - pic.height - 2);
        canvas.frame({c->x + size / 2 - pic.width - 3, c->y + size / 2 - pic.height - 3, pic.width + 2, pic.height + 2},
                     st.owner >= 0 && st.owner < kEmpires ? kEmpireColours[st.owner] : kMinorColour);
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
    const int cell = kShipCell;
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
        const Rect hull{x + 10, area.y + 72, cell - 20, 4};
        canvas.fill(hull, 0xff303848u);
        canvas.fill({hull.x, hull.y, sc.hull > 0 ? hull.w * f->ships[i].hull / sc.hull : 0, hull.h}, 0xff40e070u);
        // The crew's experience, for the player's own ships.
        if (f->owner == game_.player) {
            static constexpr std::size_t kLevelTexts[] = {200, 202, 203, 198, 201}; // green to legendary
            const std::string& level = lex[kLevelTexts[static_cast<int>(crewLevel(f->ships[i].experience))]];
            canvas.text(small, level, x + (cell - small.width(level)) / 2, area.y + 80, kNameColour);
        }
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
        } else if (f->order == Fleet::Order::Attack) {
            canvas.text(small, lex[text::kAttackSystem], text.x + 6, y, kBadColour);
        } else if (f->order == Fleet::Order::Raid) {
            canvas.text(small, lex[661], text.x + 6, y, kBadColour);
        } else if (f->order == Fleet::Order::Scrap) {
            canvas.text(small, lex[662], text.x + 6, y, kBadColour);
        } else if (f->order == Fleet::Order::Defend) {
            canvas.text(small, fillText(lex[659], {std::to_string(interceptChance(game_, *f))}), text.x + 6, y, kInfoColour);
        } else if (f->order == Fleet::Order::Wormhole) {
            canvas.text(small, lex[657], text.x + 6, y, kInfoColour);
        } else if (f->order == Fleet::Order::Train) {
            canvas.text(small, lex[664], text.x + 6, y, kInfoColour);
        } else if (const int planet = terraformTarget(game_, *f); f->order == Fleet::Order::Terraform && planet >= 0) {
            const Planet& p = game_.galaxy.systems[static_cast<std::size_t>(game_.galaxy.systemAt(f->at))].planets[static_cast<std::size_t>(planet)];
            canvas.text(small, lex[663] + "  " + p.name + "  " + std::to_string(std::min(100, f->progress * 100 / terraformCost(p))) + "%",
                        text.x + 6, y, kInfoColour);
        } else if (f->order == Fleet::Order::BuildOutpost || f->order == Fleet::Order::BuildStarbase ||
                   f->order == Fleet::Order::Upgrade) {
            const int label = f->order == Fleet::Order::BuildOutpost ? 653 : f->order == Fleet::Order::BuildStarbase ? 654 : 665;
            const int work = std::max(1, stationWork(game_, *f));
            canvas.text(small, lex[static_cast<std::size_t>(label)] + "  " + std::to_string(std::min(100, f->progress * 100 / work)) + "%",
                        text.x + 6, y, kInfoColour);
        }
        y += small.height() + 4;
        canvas.text(small, lex[f->avoid ? 652 : 658] + (f->cloaked ? "   " + lex[162] : std::string()), text.x + 6, y, kNameColour);
        if (redeploy_) {
            y += small.height() + 4;
            drawParagraph(canvas, small, "Click a ship to take it out, or one of your fleets on the left to join it.",
                          {text.x + 6, y, text.w - 12, small.height() * 3 + 6}, kInfoColour);
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
    if (overlayOpen_) {
        const Rect r{overlay_.origin().x, overlay_.origin().y, overlay_.layout().rect.w, overlay_.layout().rect.h};
        if (r.contains(p.x, p.y)) {
            overlay_.pointerPressed(p);
            return;
        }
    }
    // The chosen colony's trade routes: the header, or a route to end.
    if (const auto header = routeHeader(); header && header->contains(p.x, p.y)) {
        choosingPartner_ = !choosingPartner_;
        return;
    }
    const auto routes = routesHere();
    for (int i = 0; i < static_cast<int>(routes.size()); ++i) {
        if (const auto r = routeLine(i); r && r->contains(p.x, p.y)) {
            removeRoute(game_, routes[static_cast<std::size_t>(i)].from, routes[static_cast<std::size_t>(i)].to);
            return;
        }
    }
    // Choosing a trade partner: a system on the map.
    if (const auto s = sectorAt(p); choosingPartner_ && s) {
        const int to = game_.galaxy.systemAt(*s);
        if (to >= 0) {
            addRoute(game_, game_.player, game_.galaxy.systemAt(chosen_), to);
        }
        choosingPartner_ = false;
        return;
    }
    // A fleet in the list on the left: chosen, or joined to the chosen one.
    const auto here = fleetsHere();
    for (int i = 0; i < static_cast<int>(here.size()); ++i) {
        if (const auto r = fleetLine(i); r && r->contains(p.x, p.y)) {
            const int other = here[static_cast<std::size_t>(i)];
            if (redeploy_ && other != fleet_ && mergeFleets(game_, fleet_, other)) {
                refreshFleet();
            } else {
                chooseFleet(other);
            }
            return;
        }
    }
    // Redeploying: a ship taken out of the fleet.
    if (redeploy_) {
        for (int i = 0; game_.fleet(fleet_) != nullptr && i < static_cast<int>(game_.fleet(fleet_)->ships.size()); ++i) {
            if (const auto r = shipCell(i); r && r->contains(p.x, p.y)) {
                splitShip(game_, fleet_, i);
                refreshFleet();
                return;
            }
        }
    }
    // The list's arrows and the buttons under the map lie over the edge of
    // the panel below the map.
    for (const std::uint32_t id : {kListUp, kListDown, kEconomy, kMilitary, kEvents, kOverlay, kWholeMap}) {
        if (panel_.usable(id) && panel_.rect(id).contains(p.x, p.y)) {
            panel_.pointerPressed(p);
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
            } else if (*order == kOrderAttack) {
                orderAttack(game_, fleet_);
            } else if (*order == kOrderRaid) {
                orderRaid(game_, fleet_);
            } else if (*order == kOrderScrap) {
                orderScrap(game_, fleet_);
            } else if (*order == kOrderAvoid || *order == kOrderEngage) {
                orderAvoid(game_, fleet_, *order == kOrderAvoid);
            } else if (*order == kOrderDefend) {
                orderDefend(game_, fleet_);
            } else if (*order == kOrderWormhole) {
                orderWormhole(game_, fleet_);
            } else if (*order == kOrderTrain) {
                orderTrain(game_, fleet_);
            } else if (*order == kOrderOutpost) {
                orderBuildStation(game_, fleet_, Fleet::Order::BuildOutpost);
            } else if (*order == kOrderStarbase) {
                orderBuildStation(game_, fleet_, Fleet::Order::BuildStarbase);
            } else if (*order == kOrderTerraform) {
                orderTerraform(game_, fleet_);
            } else if (*order == kOrderUpgrade) {
                orderBuildStation(game_, fleet_, Fleet::Order::Upgrade);
            }
            ordersOpen_ = false;
            refreshFleet();
        }
        return;
    }
    if (overlayOpen_) {
        if (const auto layer = overlay_.pointerReleased(p)) {
            for (int i = 0; i < kLayers; ++i) {
                if (kLayerButtons[i] == *layer) {
                    toggle(static_cast<Layer>(i));
                }
            }
            return;
        }
    }
    if (game_.fleet(fleet_) != nullptr) {
        const auto b = fleetPanel_.pointerReleased(p);
        if (b && *b == kFleetOrders) {
            refreshFleet();
            ordersOpen_ = true;
            moving_ = false;
            redeploy_ = false;
            return;
        }
        if (b && *b == kFleetCloak) {
            if (const Fleet* f = game_.fleet(fleet_)) {
                orderCloak(game_, fleet_, !f->cloaked);
            }
            refreshFleet();
            return;
        }
        if (b && *b == kFleetRedeploy) {
            redeploy_ = !redeploy_;
            moving_ = false;
            refreshFleet();
            return;
        }
    }
    const auto id = panel_.pointerReleased(p);
    if (id && *id == kWholeMap) {
        whole_ = !whole_;
        keepInView();
    } else if (id && *id == kTurn) {
        endPlayerTurn();
    } else if (id && *id == kSummary) {
        outcome_ = Outcome::Summary;
    } else if (id && (*id == kListUp || *id == kListDown)) {
        scrollFleets(*id == kListUp ? -(kFleetLines - 1) : kFleetLines - 1);
    } else if (id && *id == kOverlay) {
        overlayOpen_ = !overlayOpen_;
        refreshModes();
    } else if (id && (*id == kEconomy || *id == kMilitary || *id == kEvents)) {
        setMode(*id == kEconomy ? Mode::Economy : *id == kMilitary ? Mode::Military : Mode::Events);
        // The events show below the map in place of the chosen fleet.
        if (mode_ == Mode::Events) {
            fleet_ = -1;
            moving_ = false;
        }
    }
}

void GalaxyScreen::endPlayerTurn() {
    // In a multiplayer game the turn ends when everyone has given orders.
    if (game_.multiplayer()) {
        outcome_ = Outcome::EndTurn;
        return;
    }
    if (game_.settings.automaticCombat) {
        report(endTurn(game_));
        return;
    }
    // The player fights their battles themselves, between the halves.
    pending_ = beginEndTurn(game_);
    waiting_ = playerBattles(game_);
    if (waiting_.empty()) {
        finishTurn({});
    } else {
        outcome_ = Outcome::Combat;
    }
}

void GalaxyScreen::finishTurn(const std::vector<Battle>& fought) {
    waiting_.clear();
    finishEndTurn(game_, pending_, fought);
    report(pending_);
}

void GalaxyScreen::report(const TurnSummary& summary) {
    events_.clear();
    for (const auto& h : summary.happenings) {
        events_.emplace_back(fillText(assets_.lexicon()[h.text], h.values), h.text == 1021 ? kInfoColour : kBadColour);
    }
    for (const int e : summary.fallen) {
        const std::string name = static_cast<std::size_t>(e) < game_.raceNames.size() ? game_.raceNames[static_cast<std::size_t>(e)] : "?";
        events_.emplace_back("The " + name + " have lost their last system", kBadColour);
    }
    // Battles first: they need the player's attention.
    for (const auto& b : summary.battles) {
        events_.emplace_back(battleText(b), kBadColour);
    }
    for (const auto& a : summary.assaults) {
        events_.emplace_back(assaultText(a), kBadColour);
    }
    for (const auto& r : summary.intel) {
        const bool good = (r.attacker == game_.player) == r.success;
        events_.emplace_back(intelText(assets_.lexicon(), game_, r), good ? kInfoColour : kBadColour);
    }
    for (const auto& d : summary.diplomacy) {
        events_.emplace_back(diplomacyText(assets_.lexicon(), game_, d), d.text == 278 ? kBadColour : kInfoColour);
    }
    for (const auto& e : summary.events) {
        events_.emplace_back(e, kNameColour);
    }
    for (const auto& t : summary.researched) {
        events_.emplace_back(assets_.lexicon()[text::kResearch] + ": " + t, kNameColour);
    }
    for (const auto& s : summary.colonized) {
        events_.emplace_back(format1(assets_.lexicon()[text::kColonized], s), kNameColour);
    }
    if (game_.fleet(fleet_) == nullptr) {
        fleet_ = -1;
        moving_ = false;
    }
    refreshFleet();
}

std::optional<Point> GalaxyScreen::command(Command c) {
    if (ordersOpen_) {
        return orders_.step(c, pointer_);
    }
    if (overlayOpen_ && c != Command::Next && c != Command::Previous) {
        return overlay_.step(c, pointer_);
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
