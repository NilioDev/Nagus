// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/research_screen.hpp"

#include "nagus/game/research.hpp"
#include "nagus/render/scene.hpp"

#include <algorithm>
#include <cctype>

namespace nagus {

namespace {

constexpr const char* kPrefixes = "chfkr";
constexpr const char* kFaces[kEmpires] = {"cardsn", "human", "ferngi", "klingn", "romuln"};
constexpr const char* kFieldPictures[kResearchFields] = {"bio.tga", "comp.tga", "cnst.tga", "engy.tga", "prop.tga", "weap.tga"};
constexpr std::size_t kFieldTexts[kTechFields] = {92, 179, 185, 279, 955, 1439, 1159};
constexpr std::size_t kLevelText = 602;     // "Level"
constexpr std::size_t kAllocation = 426;
constexpr std::size_t kProgress = 930;
constexpr std::size_t kResearching = 1003;
constexpr std::size_t kEmpireResearch = 269;
constexpr Pixel kText = 0xffffcc66u;
constexpr Pixel kDim = 0xff9098b0u;
constexpr Pixel kBarFull = 0xffff9900u;
constexpr Pixel kBarEmpty = 0xff303848u;

void bar(Canvas& canvas, const Rect& r, int value, int total) {
    canvas.fill(r, kBarEmpty);
    if (total > 0) {
        const int w = static_cast<int>(static_cast<long long>(r.w) * std::clamp(value, 0, total) / total);
        canvas.fill({r.x, r.y + 2, w, r.h - 4}, kBarFull);
    }
}

// Which object category a building is in: research (and intelligence),
// development (industry and the rest), food, energy or defence.
int categoryOf(const Building& b) {
    if (b.kind == Building::Class::Facility) {
        switch (b.output) {
        case Output::Research:
        case Output::Intelligence: return 0;
        case Output::Food: return 2;
        case Output::Energy: return 3;
        case Output::Industry: return 1;
        }
    }
    switch (b.bonus) {
    case bonus::kFood:
    case bonus::kFoodPercent:
    case bonus::kFoodToo: return 2;
    case bonus::kEnergy:
    case bonus::kEnergyPercent:
    case bonus::kEnergyToo: return 3;
    case bonus::kResearch:
    case bonus::kResearchPercent:
    case bonus::kResearchEmpirePercent:
    case bonus::kIntel:
    case bonus::kIntelPercent:
    case 12: // % security
    case 15: // % espionage
    case 22: case 23: case 24: case 25: case 26: // % research in a field
    case 27: // % sabotage
    case 28: // % energy research
    case bonus::kResearchPoints:
    case bonus::kIntelPoints:
    case 34: case 35: // % general and economic intelligence
        return 0;
    case bonus::kShipExperience:
    case bonus::kDefense:
    case bonus::kShieldPerEnergyTech:
    case 21: // bribe resistance
    case bonus::kScanRange:
    case bonus::kGroundDefense:
    case bonus::kGroundCombat: return 4;
    default: return 1;
    }
}

constexpr const char* kRanges[3] = {"short", "medium", "long"};

// Splits a text into lines that fit, keeping its line breaks.
std::vector<std::string> lines(const Font& font, const std::string& text, int width) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        std::string para = text.substr(start, end - start);
        std::erase(para, '\r');
        if (para.empty()) {
            out.emplace_back();
        } else {
            for (auto& l : wrapText(font, para, width)) {
                out.push_back(std::move(l));
            }
        }
        start = end + 1;
    }
    return out;
}

} // namespace

ResearchScreen::ResearchScreen(Assets& assets, Game& game)
    : assets_(assets), game_(game),
      panel_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "tech.wdf"),
      fields_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "techr.wdf"),
      fieldDb_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "techf.wdf"),
      objects_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "techo.wdf"),
      objShips_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "techosh.wdf"),
      objBuildings_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "techost.wdf") {
    const Rect area = panel_.rect(kArea);
    fields_.setOrigin({area.x, area.y});
    fieldDb_.setOrigin({area.x, area.y});
    objects_.setOrigin({area.x, area.y});
    if (const auto* inner = objects_.layout().find(kObjArea)) {
        objShips_.setOrigin({area.x + inner->rect.x, area.y + inner->rect.y});
        objBuildings_.setOrigin({area.x + inner->rect.x, area.y + inner->rect.y});
    }
    panel_.setEnabled(kSummary, false);
    showPage(Page::Management);
}

void ResearchScreen::showPage(Page p) {
    page_ = p;
    panel_.setOn(kManagement, p == Page::Management);
    panel_.setOn(kFieldDatabase, p == Page::Fields);
    panel_.setOn(kObjectDatabase, p == Page::Objects);
    entry_ = 0;
    listFirst_ = 0;
    textFirst_ = 0;
    refresh();
    refreshDatabase();
}

void ResearchScreen::refreshDatabase() {
    for (int f = 0; f < kResearchFields; ++f) {
        fieldDb_.setOn(kDbFields[f], f == dbField_);
    }
    for (int c = 0; c < kCategories; ++c) {
        objects_.setOn(kObjCategories[c], c == category_);
    }
    for (Panel* p : {&objShips_, &objBuildings_}) {
        p->setOn(kObjDescription, !statistics_);
        p->setOn(kObjStatistics, statistics_);
    }
}

std::vector<int> ResearchScreen::categoryItems(int category) const {
    std::vector<int> items;
    const int player = std::clamp(game_.player, 0, kEmpires - 1);
    if (category == kCategories - 1) {
        for (std::size_t i = 0; i < game_.economy.ships.size(); ++i) {
            if (game_.economy.ships[i].race == player) {
                items.push_back(static_cast<int>(i));
            }
        }
        return items;
    }
    for (std::size_t i = 0; i < game_.economy.buildings.size(); ++i) {
        const Building& b = game_.economy.buildings[i];
        // Those the empire can build: not another empire's, nor a minor
        // race's own.
        if (b.buildableBy(player) && b.minorRaces == 0xffffffffu && categoryOf(b) == category) {
            items.push_back(static_cast<int>(i));
        }
    }
    return items;
}

int ResearchScreen::entries() const {
    if (page_ == Page::Fields) {
        return static_cast<int>(std::count_if(game_.economy.techs.begin(), game_.economy.techs.end(),
                                              [&](const Tech& t) { return static_cast<int>(t.field) == dbField_; }));
    }
    return page_ == Page::Objects ? static_cast<int>(categoryItems(category_).size()) : 0;
}

Rect ResearchScreen::listRect() const {
    // Below the list's heading, which is part of the background.
    const Rect r = page_ == Page::Fields ? fieldDb_.rect(kDbList) : objectPanel().rect(kObjList);
    return {r.x, r.y + kListHeading, r.w, r.h - kListHeading};
}

int ResearchScreen::listLines() const { return std::max(1, (listRect().h - 4) / kListLine); }

std::optional<Rect> ResearchScreen::entryRect(int i) const {
    if (page_ == Page::Management || i < listFirst_ || i >= entries() || i - listFirst_ >= listLines()) {
        return std::nullopt;
    }
    const Rect list = listRect();
    return Rect{list.x + 4, list.y + 2 + (i - listFirst_) * kListLine, list.w - 8, kListLine};
}

std::string ResearchScreen::entryTitle() const {
    if (page_ == Page::Fields) {
        const Tech* t = findTech(game_.economy.techs, static_cast<TechField>(dbField_), entry_);
        return t != nullptr ? t->name : std::string();
    }
    const auto items = categoryItems(category_);
    if (page_ != Page::Objects || entry_ < 0 || static_cast<std::size_t>(entry_) >= items.size()) {
        return {};
    }
    const auto i = static_cast<std::size_t>(items[static_cast<std::size_t>(entry_)]);
    return category_ == kCategories - 1 ? game_.economy.ships[i].name + " (" + game_.economy.ships[i].className + ")"
                                        : game_.economy.buildings[i].name;
}

std::string ResearchScreen::entryText() const {
    const Resources& res = assets_.resources();
    auto text = [&](const char* file, std::uint32_t offset) {
        return res.has(file) ? textAt(res.file(file), offset) : std::string();
    };
    const EmpireState& e = game_.empires[static_cast<std::size_t>(std::clamp(game_.player, 0, kEmpires - 1))];
    if (page_ == Page::Fields) {
        const Tech* t = findTech(game_.economy.techs, static_cast<TechField>(dbField_), entry_);
        if (t == nullptr) {
            return {};
        }
        std::string out = text("techdesc.tec", t->description);
        out += "\n\n" + assets_.lexicon()[kLevelText] + " " + std::to_string(t->level) + ": " + std::to_string(t->cost) +
               " research points";
        out += e.tech[static_cast<std::size_t>(dbField_)] >= t->level ? " (reached)" : "";
        return out;
    }
    const auto items = categoryItems(category_);
    if (page_ != Page::Objects || entry_ < 0 || static_cast<std::size_t>(entry_) >= items.size()) {
        return {};
    }
    const auto i = static_cast<std::size_t>(items[static_cast<std::size_t>(entry_)]);
    if (category_ == kCategories - 1) {
        const ShipClass& s = game_.economy.ships[i];
        if (!statistics_) {
            return text("shipdesc.sst", s.description);
        }
        std::string out = "Hull " + std::to_string(s.hull) + "\nShields " + std::to_string(s.shields) + "\nBeams " +
                          std::to_string(s.beamCount) + " x " + std::to_string(s.beamDamage) + "\nTorpedoes " +
                          std::to_string(s.torpedoCount) + " x " + std::to_string(s.torpedoDamage) + "\nRange " +
                          kRanges[std::clamp(s.range, 0, 2)] + "\nScan " + std::to_string(s.scan) + "\nBuild cost " +
                          std::to_string(s.cost);
        if (s.capacity > 0) {
            out += "\nCapacity " + std::to_string(s.capacity);
        }
        if (s.cloak) {
            out += "\nCloaking device";
        }
        out += "\nNeeds:";
        for (int f = 0; f < kResearchFields; ++f) {
            if (s.tech[static_cast<std::size_t>(f)] > 0) {
                out += "\n  " + fieldName(f) + " " + std::to_string(s.tech[static_cast<std::size_t>(f)]);
            }
        }
        return out;
    }
    const Building& b = game_.economy.buildings[i];
    if (!statistics_) {
        return text("edifdesc.bst", b.description);
    }
    std::string out = "Build cost " + std::to_string(b.cost) + "\nEnergy " + std::to_string(b.energy);
    if (b.kind == Building::Class::Facility) {
        out += "\nMakes " + std::to_string(b.amount) + " for each " + std::to_string(std::max(1, b.workers)) + " million workers";
    } else {
        out += "\nBonus " + std::to_string(b.amount);
    }
    out += "\nNeeds " + fieldName(static_cast<int>(b.field)) + " " + std::to_string(b.level);
    return out;
}

EmpireState& ResearchScreen::empire() { return game_.empires[static_cast<std::size_t>(std::clamp(game_.player, 0, kEmpires - 1))]; }

Rect ResearchScreen::fieldRect(int field, std::uint32_t element) const {
    return fields_.rect(element + kStep * static_cast<std::uint32_t>(field));
}

std::string ResearchScreen::fieldName(int field) const {
    std::string name = assets_.lexicon()[kFieldTexts[field]];
    if (!name.empty()) {
        name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    }
    return name;
}

void ResearchScreen::refresh() {
    const EmpireState& e = empire();
    for (int f = 0; f < kResearchFields; ++f) {
        const auto i = static_cast<std::size_t>(f);
        const auto id = [&](std::uint32_t element) { return element + kStep * static_cast<std::uint32_t>(f); };
        fields_.setOn(id(kLock), e.locked[i]);
        fields_.setEnabled(id(kLess), !e.locked[i] && e.share[i] > 0);
        fields_.setEnabled(id(kMore), !e.locked[i] && e.share[i] < 100);
    }
}

void ResearchScreen::draw(Canvas& canvas, std::uint64_t ms) {
    panel_.draw(canvas);
    if (page_ == Page::Management) {
        fields_.draw(canvas);
    }
    const Lexicon& lex = assets_.lexicon();
    const EmpireState& e = empire();
    const int face = std::clamp(game_.player, 0, kEmpires - 1);
    const Font& f15 = assets_.font(std::string(kFaces[face]) + "15.fnt");
    const Font& f20 = assets_.font(std::string(kFaces[face]) + "20.fnt");
    const Font& f28 = assets_.font(std::string(kFaces[face]) + "28.fnt");

    // On the left: research a turn and every field's level.
    int points = 0;
    for (const auto& c : game_.colonies) {
        if (c.owner == game_.player) {
            points += colonyOutput(c, game_.economy).made[static_cast<std::size_t>(Output::Research)];
        }
    }
    canvas.text(f28, std::to_string(points), panel_.rect(kPoints), kText, Align::Center);
    canvas.text(f20, lex[kEmpireResearch], panel_.rect(kPointsLabel), kText, Align::Left);
    canvas.text(f15, lex[kLevelText], panel_.rect(kListTitle), kDim, Align::Right);
    for (int f = 0; f < kTechFields; ++f) {
        const Rect r = panel_.rect(kList[f]);
        const Rect inner{r.x + 4, r.y, r.w - 8, r.h};
        canvas.text(f20, fieldName(f), inner, f < kResearchFields ? kText : kDim, Align::Left);
        canvas.text(f20, std::to_string(e.tech[static_cast<std::size_t>(f)]), inner, kText, Align::Right);
    }

    if (page_ != Page::Management) {
        drawDatabase(canvas, f15, f20, ms);
        return;
    }
    // On the right: a block for each field.
    for (int f = 0; f < kResearchFields; ++f) {
        const auto i = static_cast<std::size_t>(f);
        const auto field = static_cast<TechField>(f);
        canvas.text(f20, fieldName(f), fieldRect(f, kFieldName), kText, Align::Left);
        const Image& pic = assets_.image(kFieldPictures[f]);
        const Rect place = fieldRect(f, kFieldPicture);
        canvas.draw(pic, place.x + (place.w - pic.width) / 2, place.y + (place.h - pic.height) / 2);
        canvas.text(f15, lex[kLevelText], fieldRect(f, kFieldText), kDim, Align::Left);
        canvas.text(f20, std::to_string(e.tech[i]), fields_.rect(kFieldLevel[f]), kText, Align::Center);
        const int cost = nextLevelCost(e, field, game_.economy.techs);
        const Tech* next = findTech(game_.economy.techs, field, e.tech[i] + 1);
        canvas.text(f15, lex[kResearching], fieldRect(f, kFieldLevelLabel), kDim, Align::Left);
        canvas.text(f15, next != nullptr ? next->name : "-", fieldRect(f, kFieldNext), kText, Align::Left);
        canvas.text(f15, lex[kAllocation], fieldRect(f, kShareLabel), kDim, Align::Left);
        bar(canvas, fieldRect(f, kShareBar), e.share[i], 100);
        canvas.text(f15, std::to_string(e.share[i]) + "%", fieldRect(f, kShareText), kText, Align::Right);
        canvas.text(f15, lex[kProgress], fieldRect(f, kProgressLabel), kDim, Align::Left);
        bar(canvas, fieldRect(f, kProgressBar), e.progress[i], cost);
        // Turns to go at this share.
        const int perTurn = points * kFieldBoost * e.share[i] / 100;
        std::string left = "-";
        if (cost > 0 && perTurn > 0) {
            left = std::to_string(std::max(1, (cost - e.progress[i] + perTurn - 1) / perTurn));
        }
        canvas.text(f15, left, fieldRect(f, kProgressText), kText, Align::Right);
    }
}

void ResearchScreen::drawDatabase(Canvas& canvas, const Font& f15, const Font& f20, std::uint64_t ms) {
    const EmpireState& e = game_.empires[static_cast<std::size_t>(std::clamp(game_.player, 0, kEmpires - 1))];
    Rect picture;
    Rect title;
    Rect body;
    if (page_ == Page::Fields) {
        fieldDb_.draw(canvas);
        picture = fieldDb_.rect(kDbPicture);
        title = fieldDb_.rect(kDbTitle);
        body = fieldDb_.rect(kDbText);
        const Image& pic = assets_.image(std::string(kFieldPictures[dbField_]).insert(std::string(kFieldPictures[dbField_]).size() - 4, "_b"));
        canvas.draw(pic, picture.x + (picture.w - pic.width) / 2, picture.y + (picture.h - pic.height) / 2);
    } else {
        objects_.draw(canvas);
        Panel& inner = objectPanel();
        inner.draw(canvas);
        picture = inner.rect(kObjPicture);
        title = inner.rect(kObjTitle);
        body = inner.rect(kObjText);
        const auto items = categoryItems(category_);
        modelShown_ = false;
        if (entry_ >= 0 && static_cast<std::size_t>(entry_) < items.size()) {
            const auto i = static_cast<std::size_t>(items[static_cast<std::size_t>(entry_)]);
            const bool ship = category_ == kCategories - 1;
            // A ship turns as a 3D model where the game has one.
            std::string model = ship ? game_.economy.ships[i].picture + "_a" : "";
            for (char& ch : model) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            const Model* m = ship ? assets_.model(model + ".hob") : nullptr;
            const Image& texture = ship ? assets_.image(model + ".gif") : assets_.image("");
            if (m != nullptr && !texture.empty()) {
                const float step = static_cast<float>(std::min<std::uint64_t>(ms - std::min(ms, lastMs_), 200)) / 1000.0f;
                switch (spin_) {
                case Spin::Left: yaw_ -= step; break;
                case Spin::Right: yaw_ += step; break;
                case Spin::Up: pitch_ -= step; break;
                case Spin::Down: pitch_ += step; break;
                case Spin::None: break;
                }
                Camera cam;
                cam.eye = {0, 0, -330};
                cam.focal = 1.35f * static_cast<float>(std::min(picture.w, picture.h));
                Scene scene(canvas, picture, cam);
                scene.model(*m, texture, {0, 0, 0}, yaw_, 115.0f, 1.0f, pitch_);
                modelShown_ = true;
            } else {
                const std::string name = ship ? "i_" + game_.economy.ships[i].picture + "170.tga" : game_.economy.buildings[i].picture + "_b.tga";
                const Image& pic = assets_.image(name);
                canvas.draw(pic, picture.x + (picture.w - pic.width) / 2, picture.y + (picture.h - pic.height) / 2);
            }
        }
        lastMs_ = ms;
    }
    // The list: what the empire has (or can build) bright, the rest dim.
    const auto items = categoryItems(category_);
    for (int i = 0; i < entries(); ++i) {
        const auto r = entryRect(i);
        if (!r) {
            continue;
        }
        std::string name;
        bool known = false;
        if (page_ == Page::Fields) {
            const Tech* t = findTech(game_.economy.techs, static_cast<TechField>(dbField_), i);
            name = std::to_string(i) + "  " + (t != nullptr ? t->name : std::string());
            known = e.tech[static_cast<std::size_t>(dbField_)] >= i;
        } else {
            const auto k = static_cast<std::size_t>(items[static_cast<std::size_t>(i)]);
            if (category_ == kCategories - 1) {
                const ShipClass& s = game_.economy.ships[k];
                name = s.name;
                known = std::equal(s.tech.begin(), s.tech.end(), e.tech.begin(), [](int need, int have) { return need <= have; });
            } else {
                const Building& b = game_.economy.buildings[k];
                name = b.name;
                known = e.tech[static_cast<std::size_t>(b.field)] >= b.level;
            }
        }
        if (i == entry_) {
            canvas.fill(*r, 0xff304080u);
        }
        canvas.text(f15, name, {r->x + 2, r->y, r->w - 4, r->h}, known ? kText : kDim, Align::Left);
    }
    if (listFirst_ > 0 || listFirst_ + listLines() < entries()) {
        const Rect list = listRect();
        const std::string more = std::to_string(listFirst_ + 1) + "-" + std::to_string(std::min(entries(), listFirst_ + listLines())) +
                                 " of " + std::to_string(entries());
        canvas.text(f15, more, {list.x, list.y + list.h, list.w, 16}, kDim, Align::Center);
    }
    canvas.text(f20, entryTitle(), title, kText, Align::Left);
    const auto text = lines(f15, entryText(), body.w);
    const int fit = std::max(1, body.h / (f15.height() + 1));
    textFirst_ = std::clamp(textFirst_, 0, std::max(0, static_cast<int>(text.size()) - fit));
    for (int i = 0; i < fit && textFirst_ + i < static_cast<int>(text.size()); ++i) {
        canvas.text(f15, text[static_cast<std::size_t>(textFirst_ + i)], body.x, body.y + i * (f15.height() + 1), kText);
    }
}

void ResearchScreen::pointerMoved(Point p) {
    pointer_ = p;
    panel_.pointerMoved(p);
    if (page_ == Page::Management) {
        fields_.pointerMoved(p);
    } else if (page_ == Page::Fields) {
        fieldDb_.pointerMoved(p);
    } else {
        objects_.pointerMoved(p);
        objectPanel().pointerMoved(p);
    }
}

void ResearchScreen::pointerPressed(Point p) {
    pointer_ = p;
    if (page_ != Page::Management) {
        // A click on the list chooses an entry; near its ends it scrolls.
        const Rect list = listRect();
        if (list.contains(p.x, p.y)) {
            const int line = (p.y - list.y - 2) / kListLine;
            if (line == 0 && listFirst_ > 0) {
                listFirst_ = std::max(0, listFirst_ - (listLines() - 1));
            } else if (line >= listLines() - 1 && listFirst_ + listLines() < entries()) {
                listFirst_ = std::min(entries() - listLines(), listFirst_ + listLines() - 1);
            } else if (listFirst_ + line < entries()) {
                entry_ = listFirst_ + line;
                textFirst_ = 0;
            }
            return;
        }
        panel_.pointerPressed(p);
        if (page_ == Page::Fields) {
            fieldDb_.pointerPressed(p);
        } else {
            objects_.pointerPressed(p);
            objectPanel().pointerPressed(p);
        }
        return;
    }
    // A click on a share bar sets the share there.
    for (int f = 0; f < kResearchFields; ++f) {
        const Rect r = fieldRect(f, kShareBar);
        if (r.contains(p.x, p.y) && !empire().locked[static_cast<std::size_t>(f)]) {
            const int percent = ((p.x - r.x) * 100 / std::max(1, r.w - 1) + kShareStep / 2) / kShareStep * kShareStep;
            setShare(empire(), f, percent);
            refresh();
            return;
        }
    }
    panel_.pointerPressed(p);
    fields_.pointerPressed(p);
}

void ResearchScreen::pointerReleased(Point p) {
    pointer_ = p;
    if (const auto button = panel_.pointerReleased(p)) {
        if (*button == kManagement) {
            showPage(Page::Management);
        } else if (*button == kFieldDatabase) {
            showPage(Page::Fields);
        } else if (*button == kObjectDatabase) {
            showPage(Page::Objects);
        }
        return;
    }
    if (page_ == Page::Fields) {
        if (const auto id = fieldDb_.pointerReleased(p)) {
            for (int f = 0; f < kResearchFields; ++f) {
                if (*id == kDbFields[f]) {
                    dbField_ = f;
                    entry_ = 0;
                    listFirst_ = 0;
                    textFirst_ = 0;
                }
            }
            textFirst_ += *id == kDbTextDown ? 3 : *id == kDbTextUp ? -3 : 0;
            textFirst_ = std::max(0, textFirst_);
            refreshDatabase();
        }
        return;
    }
    if (page_ == Page::Objects) {
        if (const auto id = objects_.pointerReleased(p)) {
            for (int c = 0; c < kCategories; ++c) {
                if (*id == kObjCategories[c]) {
                    category_ = c;
                    entry_ = 0;
                    listFirst_ = 0;
                    textFirst_ = 0;
                }
            }
        } else if (const auto inner = objectPanel().pointerReleased(p)) {
            if (*inner == kObjDescription || *inner == kObjStatistics) {
                statistics_ = *inner == kObjStatistics;
                textFirst_ = 0;
            }
            textFirst_ += *inner == kObjTextDown ? 3 : *inner == kObjTextUp ? -3 : 0;
            textFirst_ = std::max(0, textFirst_);
            if (*inner == kObjListUp || *inner == kObjListDown) {
                const int page = std::max(1, listLines() - 1);
                listFirst_ = std::clamp(listFirst_ + (*inner == kObjListDown ? page : -page), 0, std::max(0, entries() - listLines()));
            }
            // A turn button starts the ship turning that way, or stops it.
            const std::pair<std::uint32_t, Spin> turns[] = {
                {kTurnUp, Spin::Up}, {kTurnDown, Spin::Down}, {kTurnLeft, Spin::Left}, {kTurnRight, Spin::Right}};
            for (const auto& [button, way] : turns) {
                if (*inner == button) {
                    spin_ = spin_ == way ? Spin::None : way;
                }
            }
        }
        refreshDatabase();
        return;
    }
    const auto id = fields_.pointerReleased(p);
    if (!id) {
        return;
    }
    EmpireState& e = empire();
    for (int f = 0; f < kResearchFields; ++f) {
        const auto i = static_cast<std::size_t>(f);
        const std::uint32_t step = kStep * static_cast<std::uint32_t>(f);
        if (*id == kLock + step) {
            e.locked[i] = !e.locked[i];
        } else if (*id == kLess + step) {
            setShare(e, f, e.share[i] - kShareStep);
        } else if (*id == kMore + step) {
            setShare(e, f, e.share[i] + kShareStep);
        }
    }
    refresh();
}

std::optional<Point> ResearchScreen::command(Command c) {
    if (c == Command::Back) {
        outcome_ = Outcome::Back;
        return std::nullopt;
    }
    if (page_ != Page::Management && (c == Command::Next || c == Command::Previous)) {
        // Tab, Page Up/Down and the shoulder buttons go through the fields or
        // categories.
        const int step = c == Command::Next ? 1 : -1;
        if (page_ == Page::Fields) {
            dbField_ = (dbField_ + step + kResearchFields) % kResearchFields;
        } else {
            category_ = (category_ + step + kCategories) % kCategories;
        }
        entry_ = 0;
        listFirst_ = 0;
        textFirst_ = 0;
        refreshDatabase();
        return std::nullopt;
    }
    if (page_ != Page::Management && (c == Command::Up || c == Command::Down)) {
        // The gamepad and arrow keys go through the list.
        entry_ = std::clamp(entry_ + (c == Command::Down ? 1 : -1), 0, std::max(0, entries() - 1));
        listFirst_ = std::clamp(listFirst_, std::max(0, entry_ - listLines() + 1), entry_);
        textFirst_ = 0;
        return std::nullopt;
    }
    std::optional<Point> best = panel_.step(c, pointer_);
    const Panel& pagePanel = page_ == Page::Management ? fields_ : page_ == Page::Fields ? fieldDb_ : objects_;
    if (const auto other = pagePanel.step(c, pointer_)) {
        auto dist = [&](Point q) { return std::abs(q.x - pointer_.x) + std::abs(q.y - pointer_.y); };
        if (!best || dist(*other) < dist(*best)) {
            best = other;
        }
    }
    return best;
}

} // namespace nagus
