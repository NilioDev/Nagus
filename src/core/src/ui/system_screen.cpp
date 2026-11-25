// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/system_screen.hpp"

#include "nagus/ui/galaxy_screen.hpp"

#include <algorithm>
#include <cctype>

namespace nagus {

namespace {

constexpr const char* kPrefixes = "chfkr";
constexpr const char* kFaces[kEmpires] = {"cardsn", "human", "ferngi", "klingn", "romuln"};
constexpr Pixel kText = 0xffffcc66u;
constexpr Pixel kDim = 0xff9098b0u;
constexpr Pixel kGood = 0xff60e060u;
constexpr Pixel kBad = 0xffff6040u;
constexpr Pixel kChosenLine = 0xff304080u;
constexpr Pixel kBarFull = 0xffff9900u;
constexpr Pixel kBarEmpty = 0xff303848u;
constexpr int kLineHeight = 19;
constexpr std::size_t kNoText = 0;

// The picture of a build item: buildings add "_30", "_60", "170" or ""
// to their picture's name, ships put "i_" before it and "30", "60", "170"
// or "120" after.
std::string itemPicture(const EconomyData& data, const BuildItem& item, const char* size) {
    if (item.kind == BuildItem::Kind::Ship) {
        if (item.building < 0 || static_cast<std::size_t>(item.building) >= data.ships.size()) {
            return {};
        }
        const std::string s = size;
        const std::string number = s == ".tga" ? "120" : s == "_30.tga" ? "30" : s == "_60.tga" ? "60" : "170";
        return "i_" + data.ships[static_cast<std::size_t>(item.building)].picture + number + ".tga";
    }
    if (item.building < 0 || static_cast<std::size_t>(item.building) >= data.buildings.size()) {
        return {};
    }
    return data.buildings[static_cast<std::size_t>(item.building)].picture + size;
}

std::string pictureOf(const EconomyData& data, int building, const char* suffix) {
    if (building < 0 || static_cast<std::size_t>(building) >= data.buildings.size()) {
        return {};
    }
    return data.buildings[static_cast<std::size_t>(building)].picture + suffix;
}

std::string withNumber(const std::string& pattern, int n) {
    std::string out = pattern;
    if (const auto at = out.find("%d"); at != std::string::npos) {
        out.replace(at, 2, std::to_string(n));
    }
    return out;
}

std::string withText(const std::string& pattern, const std::string& s) {
    std::string out = pattern;
    if (const auto at = out.find("%s"); at != std::string::npos) {
        out.replace(at, 2, s);
    }
    return out;
}

std::size_t outputText(Output o) {
    switch (o) {
    case Output::Food: return text::kFood;
    case Output::Industry: return text::kIndustry;
    case Output::Energy: return text::kEnergy;
    case Output::Research: return text::kResearch;
    case Output::Intelligence: return text::kIntelligence;
    }
    return kNoText;
}

// A label on the left of a box and its value on the right.
void drawFact(Canvas& canvas, const Font& font, const Rect& box, const std::string& label, const std::string& value,
              Pixel valueColour) {
    canvas.text(font, label, box, kText, Align::Left);
    canvas.text(font, value, box, valueColour, Align::Right);
}

void drawBar(Canvas& canvas, const Rect& r, int filled, int total) {
    canvas.fill(r, kBarEmpty);
    if (total <= 0) {
        return;
    }
    // One block for each facility, lit while it has workers.
    const int gap = 2;
    const int w = std::max(2, (r.w - gap * (total - 1)) / total);
    for (int i = 0; i < total && i * (w + gap) < r.w; ++i) {
        canvas.fill({r.x + i * (w + gap), r.y + 2, std::min(w, r.w - i * (w + gap)), r.h - 4},
                    i < filled ? kBarFull : 0xff505868u);
    }
}

} // namespace

SystemScreen::SystemScreen(Assets& assets, Game& game, int colony)
    : assets_(assets), game_(game), colony_(colony), prefix_(kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]),
      panel_(assets, std::string(1, prefix_) + "solars.wdf"), labor_(assets, std::string(1, prefix_) + "labor.wdf"),
      build_(assets, std::string(1, prefix_) + "solbld.wdf"), structures_(assets, std::string(1, prefix_) + "solcur.wdf"),
      energy_(assets, std::string(1, prefix_) + "senergy.wdf"), strip_(assets, std::string(1, prefix_) + "_sysmap.wdf"),
      animations_(assets.resources().names(".ani")) {
    const Rect middle = panel_.rect(kMiddle);
    for (Panel* p : {&labor_, &build_, &structures_, &energy_}) {
        p->setOrigin({middle.x, middle.y});
    }
    const Rect bottom = panel_.rect(kBottom);
    strip_.setOrigin({bottom.x, bottom.y});
    refresh();
}

const StarSystem& SystemScreen::system() const { return game_.galaxy.systems[static_cast<std::size_t>(col().system)]; }

const Font& SystemScreen::font(int size) const {
    const int face = std::clamp(game_.player, 0, kEmpires - 1);
    return assets_.font(std::string(kFaces[face]) + std::to_string(size) + ".fnt");
}

Panel* SystemScreen::sub() {
    switch (tab_) {
    case Tab::Production: return &labor_;
    case Tab::BuildList:
    case Tab::Shipbuilding: return &build_;
    case Tab::Structures: return &structures_;
    case Tab::Energy: return &energy_;
    case Tab::Summary: break;
    }
    return nullptr;
}

void SystemScreen::showTab(Tab t) {
    if (t != tab_) {
        chosenOption_ = 0;
        firstOption_ = 0;
    }
    tab_ = t;
    refresh();
}

void SystemScreen::refresh() {
    const Colony& c = col();
    const auto& empire = game_.empires[static_cast<std::size_t>(std::clamp(game_.player, 0, kEmpires - 1))];
    options_ = buildOptions(c, empire, system(), game_.economy);
    // Ships have a tab of their own.
    const bool ships = tab_ == Tab::Shipbuilding;
    std::erase_if(options_, [&](const BuildItem& i) { return (i.kind == BuildItem::Kind::Ship) != ships; });
    chosenOption_ = std::clamp(chosenOption_, 0, std::max(0, static_cast<int>(options_.size()) - 1));
    panel_.setOn(kSummary, tab_ == Tab::Summary);
    panel_.setOn(kProduction, tab_ == Tab::Production);
    panel_.setOn(kEnergy, tab_ == Tab::Energy);
    panel_.setOn(kBuildList, tab_ == Tab::BuildList);
    panel_.setOn(kShipbuilding, tab_ == Tab::Shipbuilding);
    panel_.setOn(kStructures, tab_ == Tab::Structures);
    panel_.setEnabled(kBuy, !c.queue.empty() && buyCost(c, game_.economy) <= empire.credits);
    for (int row = 0; row < 5; ++row) {
        const auto o = static_cast<std::size_t>(kLaborOrder[row]);
        labor_.setEnabled(kLaborRows[row][2], c.working[o] > 0);
        labor_.setEnabled(kLaborRows[row][3], c.working[o] < c.facilities[o]);
    }
    const int lines = std::max(1, build_.rect(kBuildItems).h / kLineHeight);
    build_.setEnabled(kBuildUp, firstOption_ > 0);
    build_.setEnabled(kBuildDown, firstOption_ + lines < static_cast<int>(options_.size()));
    const int pages = std::max(1, (static_cast<int>(c.structures.size()) + 5) / 6);
    structurePage_ = std::clamp(structurePage_, 0, pages - 1);
    structures_.setEnabled(kStructuresBack, structurePage_ > 0);
    structures_.setEnabled(kStructuresNext, structurePage_ + 1 < pages);
}

std::string SystemScreen::turnsText(int turns) const {
    const Lexicon& lex = assets_.lexicon();
    return turns == 1 ? lex[text::kOneTurn] : withNumber(lex[text::kTurns], turns);
}

std::vector<int> SystemScreen::poweredStructures() const {
    std::vector<int> list;
    const Colony& c = col();
    for (std::size_t i = 0; i < c.structures.size(); ++i) {
        const int b = c.structures[i].building;
        if (b >= 0 && static_cast<std::size_t>(b) < game_.economy.buildings.size() &&
            game_.economy.buildings[static_cast<std::size_t>(b)].energy > 0) {
            list.push_back(static_cast<int>(i));
        }
    }
    return list;
}

void SystemScreen::draw(Canvas& canvas, std::uint64_t ms) {
    panel_.draw(canvas);
    const Lexicon& lex = assets_.lexicon();
    const StarSystem& s = system();
    std::string title = s.name;
    for (char& ch : title) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    // The large fonts have capitals only.
    canvas.text(font(28), title, panel_.rect(kTitle), kText, Align::Center);
    canvas.text(font(20), s.name, panel_.rect(kName), kText, Align::Center);
    canvas.text(font(15), lex[text::kBuildQueue], panel_.rect(kQueueTitle), kText, Align::Center);
    drawFacts(canvas);
    drawQueue(canvas);
    switch (tab_) {
    case Tab::Summary: drawSummary(canvas, ms); break;
    case Tab::Production: drawLabor(canvas); break;
    case Tab::Energy: drawEnergy(canvas); break;
    case Tab::BuildList:
    case Tab::Shipbuilding: drawBuildList(canvas); break;
    case Tab::Structures: drawStructures(canvas); break;
    }
    strip_.draw(canvas);
    canvas.text(font(9), s.name, strip_.rect(GalaxyScreen::kInfoName), kText, Align::Center);
    const Rect details = strip_.rect(GalaxyScreen::kSystemDetails);
    canvas.text(font(9), lex[text::kPlanets] + ": " + std::to_string(s.planets.size()), details.x + 4, details.y + 2, kDim);
    drawPlanetStrip(canvas, assets_, game_, s, strip_.rect(GalaxyScreen::kInfoPicture), font(9), ms, animations_);
}

void SystemScreen::drawFacts(Canvas& canvas) {
    const Lexicon& lex = assets_.lexicon();
    const Colony& c = col();
    const ColonyOutput out = colonyOutput(c, game_.economy);
    const Font& f = font(15);
    auto made = [&](Output o) { return out.made[static_cast<std::size_t>(o)]; };
    const int food = made(Output::Food) - out.foodNeeded;
    const int energy = made(Output::Energy) - out.energyNeeded;
    const std::string signedFood = (food >= 0 ? "+" : "") + std::to_string(food);
    const std::string signedEnergy = (energy >= 0 ? "+" : "") + std::to_string(energy);
    const auto& empire = game_.empires[static_cast<std::size_t>(std::clamp(game_.player, 0, kEmpires - 1))];
    struct Fact {
        std::string label;
        std::string value;
        Pixel colour;
    };
    const Fact facts[9] = {
        {lex[text::kPopulation], std::to_string(c.population), kText},
        {lex[text::kMaxPop], std::to_string(c.maxPopulation), kText},
        {lex[text::kFood], signedFood + " (" + std::to_string(c.food) + ")", food >= 0 ? kGood : kBad},
        {lex[text::kIndustry], std::to_string(made(Output::Industry)), kText},
        {lex[text::kEnergy], signedEnergy, energy >= 0 ? kGood : kBad},
        {lex[text::kResearch], std::to_string(made(Output::Research)), kText},
        {lex[text::kIntelligence], std::to_string(made(Output::Intelligence)), kText},
        {lex[text::kCredits], "+" + std::to_string(out.credits), kText},
        {lex[text::kCredits], std::to_string(empire.credits), kDim},
    };
    for (int i = 0; i < 9; ++i) {
        const Rect box = panel_.rect(kFacts[i]);
        drawFact(canvas, f, {box.x + 3, box.y, box.w - 6, box.h}, facts[i].label, facts[i].value, facts[i].colour);
    }
}

void SystemScreen::drawQueue(Canvas& canvas) {
    const Colony& c = col();
    const EconomyData& data = game_.economy;
    const ColonyOutput out = colonyOutput(c, data);
    const int industry = out.made[static_cast<std::size_t>(Output::Industry)];
    const Font& f = font(15);
    const Font& small = font(9);
    if (c.queue.empty()) {
        canvas.text(f, assets_.lexicon()[text::kTradeGoods], panel_.rect(kCurrentName), kText, Align::Center);
        return;
    }
    // The item being built, with how long it still takes.
    const BuildItem& first = c.queue.front();
    const Image& pic = assets_.image(itemPicture(data, first, ".tga"));
    const Rect place = panel_.rect(kCurrentPicture);
    canvas.draw(pic, place.x + (place.w - pic.width) / 2, place.y + (place.h - pic.height) / 2);
    canvas.text(small, itemName(first, data), panel_.rect(kCurrentName), kText, Align::Center);
    const int left = static_cast<int>(itemCost(first, c, data)) - c.progress;
    const int turns = industry > 0 ? std::max(1, (left + industry - 1) / industry) : 0;
    canvas.text(small, industry > 0 ? turnsText(turns) : "-", panel_.rect(kCurrentTurns), kText, Align::Left);
    canvas.text(small, std::to_string(buyCost(c, data)), panel_.rect(kCurrentTurns), kDim, Align::Right);
    // The items after it.
    for (std::size_t i = 1; i < c.queue.size() && i <= std::size(kQueueSlots); ++i) {
        const Rect slot = panel_.rect(kQueueSlots[i - 1]);
        const Image& small30 = assets_.image(itemPicture(data, c.queue[i], "_30.tga"));
        canvas.draw(small30, slot.x + 4, slot.y + (slot.h - small30.height) / 2);
        drawParagraph(canvas, small, itemName(c.queue[i], data), {slot.x + 40, slot.y + 3, slot.w - 44, slot.h - 6}, kText);
    }
}

void SystemScreen::drawSummary(Canvas& canvas, std::uint64_t ms) {
    // The planets, one to a line, each turning, with its kind and room.
    const StarSystem& s = system();
    const Rect middle = panel_.rect(kMiddle);
    const Font& f = font(15);
    const Font& small = font(9);
    const Lexicon& lex = assets_.lexicon();
    const int lineH = std::min(70, (middle.h - 60) / std::max<int>(1, static_cast<int>(s.planets.size())));
    int y = middle.y + 50;
    static constexpr std::size_t kPlanetTypeTexts[kPlanetTypes] = {38, 56, 230, 0, 559, 879, 1259, 1415};
    for (std::size_t i = 0; i < s.planets.size(); ++i) {
        const Planet& p = s.planets[i];
        const Animation& a = assets_.animation(planetAnimation(p, animations_));
        if (a.frames > 0) {
            canvas.draw(a.sheet, a.part(static_cast<int>(ms / 90) + static_cast<int>(i) * 5), middle.x + 70 - a.frameWidth / 2,
                        y + lineH / 2 - a.frameHeight / 2);
        }
        canvas.text(f, p.name, middle.x + 120, y + lineH / 2 - f.height(), kText);
        const std::size_t typeText = static_cast<std::size_t>(p.type) < std::size(kPlanetTypeTexts) ? kPlanetTypeTexts[p.type] : 0;
        std::string kind = typeText != 0 ? lex[typeText] : "Gas Giant";
        if (p.room > 0) {
            kind += "   " + lex[text::kMaxPop] + " " + std::to_string(p.room);
        }
        if (p.room > 0 && !p.terraformed && p.type != kGasGiant) {
            // Holds nobody until terraformed: the work it takes.
            kind += "   " + lex[1257] + " " + std::to_string(terraformCost(p));
        }
        if (p.bonus != kNoBonus) {
            // Food (lexicon 365) or energy (279), and how much it adds.
            kind += "   " + lex[p.bonus == kFoodBonus ? 365 : 279] + " bonus " + std::to_string(kPlanetBonusPercent);
        }
        canvas.text(small, kind, middle.x + 120, y + lineH / 2 + 2, kDim);
        y += lineH;
    }
    if (s.dilithium) {
        canvas.text(small, lex[240], middle.x + 120, middle.y + 32, kText); // "Dilithium Present"
    }
}

void SystemScreen::drawLabor(Canvas& canvas) {
    labor_.draw(canvas);
    const Lexicon& lex = assets_.lexicon();
    const Colony& c = col();
    const EconomyData& data = game_.economy;
    const ColonyOutput out = colonyOutput(c, data);
    const Font& f = font(15);
    canvas.text(f, lex[text::kLabor], labor_.rect(kLaborTitle), kText, Align::Left);
    int busy = 0;
    for (int row = 0; row < 5; ++row) {
        const Output o = kLaborOrder[row];
        const auto i = static_cast<std::size_t>(o);
        busy += c.working[i];
        const int building = c.facility[i];
        const Image& pic = assets_.image(pictureOf(data, building, "_60.tga"));
        const Rect place = labor_.rect(kLaborRows[row][0]);
        canvas.draw(pic, place.x, place.y);
        std::string title = lex[outputText(o)];
        if (building >= 0) {
            title += ": " + data.buildings[static_cast<std::size_t>(building)].plural;
        }
        const Rect titleBox = labor_.rect(kLaborRows[row][1]);
        canvas.text(f, title, {titleBox.x + 4, titleBox.y, titleBox.w - 8, titleBox.h}, kText, Align::Left);
        canvas.text(f, std::to_string(out.made[i]), {titleBox.x + 4, titleBox.y, titleBox.w - 8, titleBox.h}, kText, Align::Right);
        const Rect bar = labor_.rect(kLaborRows[row][4]);
        drawBar(canvas, bar, c.working[i], c.facilities[i]);
    }
    const int idle = c.population / 10 - busy;
    canvas.text(f, lex[text::kUnassigned], labor_.rect(kUnassignedText), kText, Align::Left);
    const Rect bar = labor_.rect(kUnassignedBar);
    canvas.text(f, std::to_string(std::max(0, idle)), bar, idle > 0 ? kBad : kDim, Align::Center);
}

std::optional<Rect> SystemScreen::optionRect(int i) const {
    const Rect box = build_.rect(kBuildItems);
    const int lines = std::max(1, box.h / kLineHeight);
    if (i < firstOption_ || i >= firstOption_ + lines || i >= static_cast<int>(options_.size())) {
        return std::nullopt;
    }
    return Rect{box.x, box.y + (i - firstOption_) * kLineHeight, box.w, kLineHeight};
}

void SystemScreen::chooseOption(int i) {
    if (options_.empty()) {
        return;
    }
    chosenOption_ = std::clamp(i, 0, static_cast<int>(options_.size()) - 1);
    const int lines = std::max(1, build_.rect(kBuildItems).h / kLineHeight);
    firstOption_ = std::clamp(firstOption_, chosenOption_ - lines + 1, chosenOption_);
    refresh();
}

void SystemScreen::addChosen() {
    if (chosenOption_ < 0 || chosenOption_ >= static_cast<int>(options_.size())) {
        return;
    }
    Colony& c = col();
    if (c.queue.size() >= std::size(kQueueSlots) + 1) {
        return;
    }
    c.queue.push_back(options_[static_cast<std::size_t>(chosenOption_)]);
    refresh();
}

void SystemScreen::drawBuildList(Canvas& canvas) {
    build_.draw(canvas);
    const Lexicon& lex = assets_.lexicon();
    const EconomyData& data = game_.economy;
    const Colony& c = col();
    const Font& f = font(15);
    canvas.text(f, lex[text::kBuild], build_.rect(kBuildTitle), kText, Align::Left);
    for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
        const auto r = optionRect(i);
        if (!r) {
            continue;
        }
        if (i == chosenOption_) {
            canvas.fill(*r, kChosenLine);
        }
        const BuildItem& item = options_[static_cast<std::size_t>(i)];
        std::string name = itemName(item, data);
        if (item.kind == BuildItem::Kind::Upgrade) {
            name = withText(lex[text::kUpgradeTo], name);
        }
        canvas.text(f, name, {r->x + 6, r->y, r->w - 60, r->h}, kText, Align::Left);
        canvas.text(f, std::to_string(itemCost(item, c, data)), {r->x, r->y, r->w - 6, r->h}, kDim, Align::Right);
    }
    if (options_.empty()) {
        return;
    }
    // The chosen line: its picture and what it does.
    const BuildItem& item = options_[static_cast<std::size_t>(chosenOption_)];
    const Rect place = build_.rect(kBuildPicture);
    const Image& pic = assets_.image(itemPicture(data, item, "170.tga"));
    canvas.draw(pic, place.x, place.y);
    const Rect details = build_.rect(kBuildDetails);
    const Font& small = font(9);
    int y = details.y + 4;
    const std::string name = itemName(item, data);
    drawParagraph(canvas, f, name, {details.x + 6, y, details.w - 12, f.height() * 2 + 2}, kText);
    y += f.height() * (f.width(name) > details.w - 12 ? 2 : 1) + 6;
    auto line = [&](const std::string& label, const std::string& value) {
        drawFact(canvas, small, {details.x + 6, y, details.w - 12, small.height() + 2}, label, value, kText);
        y += small.height() + 4;
    };
    line(lex[text::kBuildCost], std::to_string(itemCost(item, c, data)));
    if (item.kind == BuildItem::Kind::Ship) {
        const ShipClass& s = data.ships[static_cast<std::size_t>(item.building)];
        Fleet one;
        one.ships.push_back({item.building, s.hull});
        const auto& empire = game_.empires[static_cast<std::size_t>(std::clamp(game_.player, 0, kEmpires - 1))];
        line(s.className, "");
        line(lex[text::kHullShields], std::to_string(s.hull) + " / " + std::to_string(s.shields));
        line(lex[text::kGalSpeed], std::to_string(fleetSpeed(one, data, empire)));
        line(lex[text::kGalRange], lex[text::kRangeNames[std::clamp(s.range, 0, 2)]]);
        if (s.capacity > 0) {
            line(lex[s.role == ShipRole::Colony ? text::kPopulation : text::kCrew],
                 std::to_string(s.role == ShipRole::Colony ? s.capacity * kColonistsPerCapacity : s.capacity));
        }
        return;
    }
    const Building& b = data.buildings[static_cast<std::size_t>(item.building)];
    if (b.energy > 0) {
        line(lex[text::kMaintenance], std::to_string(b.energy));
    }
    if (b.kind == Building::Class::Facility) {
        line(lex[outputText(b.output)], "+" + std::to_string(b.amount));
    }
}

void SystemScreen::drawStructures(Canvas& canvas) {
    structures_.draw(canvas);
    const Lexicon& lex = assets_.lexicon();
    const EconomyData& data = game_.economy;
    const Colony& c = col();
    const Font& small = font(9);
    canvas.text(font(15), lex[text::kCurrentStructures], structures_.rect(kStructuresTitle), kText, Align::Left);
    for (int k = 0; k < 6; ++k) {
        const std::size_t i = static_cast<std::size_t>(structurePage_ * 6 + k);
        if (i >= c.structures.size()) {
            break;
        }
        const int building = c.structures[i].building;
        const Rect place = structures_.rect(kStructurePlaces[k][0]);
        const Image& pic = assets_.image(pictureOf(data, building, "_60.tga"));
        canvas.draw(pic, place.x, place.y);
        drawParagraph(canvas, small, data.buildings[static_cast<std::size_t>(building)].name, structures_.rect(kStructurePlaces[k][1]),
                      kText);
    }
    if (c.structures.empty()) {
        canvas.text(font(15), withText(lex[853], lex[text::kStructures]), structures_.rect(kStructurePlaces[0][1]), kDim, Align::Left);
    }
}

void SystemScreen::drawEnergy(Canvas& canvas) {
    energy_.draw(canvas);
    const Lexicon& lex = assets_.lexicon();
    const EconomyData& data = game_.economy;
    const Colony& c = col();
    const ColonyOutput out = colonyOutput(c, data);
    const Font& f = font(15);
    const Font& small = font(9);
    canvas.text(f, lex[text::kEnergy], energy_.rect(kEnergyTitle), kText, Align::Left);
    // The structures that use energy, each switched on or off by a click.
    const auto list = poweredStructures();
    for (std::size_t k = 0; k < list.size() && k < 14; ++k) {
        const Structure& st = c.structures[static_cast<std::size_t>(list[k])];
        const std::uint32_t place = k < 7 ? 0x96a + static_cast<std::uint32_t>(k) : 0x97e + static_cast<std::uint32_t>(k - 7);
        const std::uint32_t label = k < 7 ? 0x974 + static_cast<std::uint32_t>(k) : 0x988 + static_cast<std::uint32_t>(k - 7);
        const Rect r = energy_.rect(place);
        canvas.draw(assets_.image(pictureOf(data, st.building, "_30.tga")), r.x, r.y);
        if (!st.on) {
            canvas.blend(r, rgb(0, 0, 0), 160);
        }
        canvas.text(small, std::to_string(data.buildings[static_cast<std::size_t>(st.building)].energy), energy_.rect(label),
                    st.on ? kGood : kDim, Align::Center);
    }
    const int made = out.made[static_cast<std::size_t>(Output::Energy)];
    drawBar(canvas, energy_.rect(kEnergyBar), std::min(out.energyNeeded, made) / 5, std::max(made, out.energyNeeded) / 5);
    canvas.text(f, std::to_string(out.energyNeeded) + " / " + std::to_string(made), energy_.rect(kEnergyText),
                out.energyNeeded <= made ? kGood : kBad, Align::Left);
}

void SystemScreen::pointerMoved(Point p) {
    pointer_ = p;
    panel_.pointerMoved(p);
    if (Panel* s = sub()) {
        s->pointerMoved(p);
    }
}

void SystemScreen::pointerPressed(Point p) {
    pointer_ = p;
    if (tab_ == Tab::BuildList || tab_ == Tab::Shipbuilding) {
        for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
            if (const auto r = optionRect(i); r && r->contains(p.x, p.y)) {
                // A click chooses a line; a click on the chosen line builds it.
                if (i == chosenOption_) {
                    addChosen();
                } else {
                    chooseOption(i);
                }
                return;
            }
        }
    }
    if (tab_ == Tab::Energy) {
        const auto list = poweredStructures();
        for (std::size_t k = 0; k < list.size() && k < 14; ++k) {
            const std::uint32_t place = k < 7 ? 0x96a + static_cast<std::uint32_t>(k) : 0x97e + static_cast<std::uint32_t>(k - 7);
            if (energy_.rect(place).contains(p.x, p.y)) {
                auto& st = col().structures[static_cast<std::size_t>(list[k])];
                st.on = !st.on;
                return;
            }
        }
    }
    // A click on a waiting item of the queue takes it out.
    for (std::size_t i = 0; i < std::size(kQueueSlots); ++i) {
        if (panel_.rect(kQueueSlots[i]).contains(p.x, p.y) && i + 1 < col().queue.size()) {
            col().queue.erase(col().queue.begin() + static_cast<std::ptrdiff_t>(i + 1));
            refresh();
            return;
        }
    }
    panel_.pointerPressed(p);
    if (Panel* s = sub()) {
        s->pointerPressed(p);
    }
}

void SystemScreen::pointerReleased(Point p) {
    pointer_ = p;
    if (const auto id = panel_.pointerReleased(p)) {
        clicked(*id);
    }
    if (Panel* s = sub()) {
        if (const auto id = s->pointerReleased(p)) {
            clicked(*id);
        }
    }
}

void SystemScreen::clicked(std::uint32_t id) {
    Colony& c = col();
    switch (id) {
    case kSummary: showTab(Tab::Summary); return;
    case kProduction: showTab(Tab::Production); return;
    case kEnergy: showTab(Tab::Energy); return;
    case kBuildList:
    case kBuild: showTab(Tab::BuildList); return;
    case kStructures: showTab(Tab::Structures); return;
    case kShipbuilding: showTab(Tab::Shipbuilding); return;
    case kBuy: {
        auto& empire = game_.empires[static_cast<std::size_t>(std::clamp(game_.player, 0, kEmpires - 1))];
        const int cost = buyCost(c, game_.economy);
        if (!c.queue.empty() && cost <= empire.credits) {
            empire.credits -= cost;
            c.progress = static_cast<int>(itemCost(c.queue.front(), c, game_.economy));
        }
        refresh();
        return;
    }
    case kSystems: {
        // On to the player's next colony.
        for (std::size_t k = 1; k <= game_.colonies.size(); ++k) {
            const std::size_t i = (static_cast<std::size_t>(colony_) + k) % game_.colonies.size();
            if (game_.colonies[i].owner == game_.player) {
                colony_ = static_cast<int>(i);
                break;
            }
        }
        refresh();
        return;
    }
    case kBuildUp: firstOption_ = std::max(0, firstOption_ - 1); refresh(); return;
    case kBuildDown: firstOption_ += 1; refresh(); return;
    case kStructuresBack: structurePage_ -= 1; refresh(); return;
    case kStructuresNext: structurePage_ += 1; refresh(); return;
    default: break;
    }
    for (int row = 0; row < 5; ++row) {
        const Output o = kLaborOrder[row];
        const int now = c.working[static_cast<std::size_t>(o)];
        if (id == kLaborRows[row][2]) {
            setWorking(c, o, now - 1);
        } else if (id == kLaborRows[row][3]) {
            setWorking(c, o, now + 1);
        }
    }
    refresh();
}

std::optional<Point> SystemScreen::command(Command c) {
    if (c == Command::Back) {
        outcome_ = Outcome::Back;
        return std::nullopt;
    }
    // In the build list, up and down choose a line.
    if ((tab_ == Tab::BuildList || tab_ == Tab::Shipbuilding) && build_.rect(kBuildItems).contains(pointer_.x, pointer_.y) &&
        (c == Command::Up || c == Command::Down)) {
        chooseOption(chosenOption_ + (c == Command::Up ? -1 : 1));
        if (const auto r = optionRect(chosenOption_)) {
            return Point{r->centerX(), r->centerY()};
        }
        return std::nullopt;
    }
    if (c == Command::Next || c == Command::Previous) {
        static constexpr Tab kOrder[] = {Tab::Summary, Tab::Production, Tab::Energy, Tab::BuildList, Tab::Shipbuilding, Tab::Structures};
        constexpr int n = static_cast<int>(std::size(kOrder));
        int i = 0;
        while (kOrder[i] != tab_) {
            ++i;
        }
        i = (i + (c == Command::Next ? 1 : n - 1)) % n;
        showTab(kOrder[i]);
        return std::nullopt;
    }
    // Otherwise the pointer moves to the nearest button of either panel.
    std::optional<Point> best = panel_.step(c, pointer_);
    if (Panel* s = sub()) {
        if (const auto other = s->step(c, pointer_)) {
            auto dist = [&](Point q) { return std::abs(q.x - pointer_.x) + std::abs(q.y - pointer_.y); };
            if (!best || dist(*other) < dist(*best)) {
                best = other;
            }
        }
    }
    if ((tab_ == Tab::BuildList || tab_ == Tab::Shipbuilding) && (c == Command::Left || c == Command::Right)) {
        // Lets the gamepad reach the list itself.
        const Rect list = build_.rect(kBuildItems);
        if (const auto r = optionRect(chosenOption_); r && !list.contains(pointer_.x, pointer_.y) && c == Command::Right &&
                                                      pointer_.x < list.x) {
            return Point{r->centerX(), r->centerY()};
        }
    }
    return best;
}

} // namespace nagus
