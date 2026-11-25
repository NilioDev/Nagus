// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/research_screen.hpp"

#include "nagus/game/research.hpp"

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

} // namespace

ResearchScreen::ResearchScreen(Assets& assets, Game& game)
    : assets_(assets), game_(game),
      panel_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "tech.wdf"),
      fields_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "techr.wdf") {
    const Rect area = panel_.rect(kArea);
    fields_.setOrigin({area.x, area.y});
    for (const std::uint32_t id : {kSummary, kFieldDatabase, kObjectDatabase}) {
        panel_.setEnabled(id, false);
    }
    panel_.setOn(kManagement, true);
    refresh();
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

void ResearchScreen::draw(Canvas& canvas, std::uint64_t) {
    panel_.draw(canvas);
    fields_.draw(canvas);
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

void ResearchScreen::pointerMoved(Point p) {
    pointer_ = p;
    panel_.pointerMoved(p);
    fields_.pointerMoved(p);
}

void ResearchScreen::pointerPressed(Point p) {
    pointer_ = p;
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
    panel_.pointerReleased(p);
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
    std::optional<Point> best = panel_.step(c, pointer_);
    if (const auto other = fields_.step(c, pointer_)) {
        auto dist = [&](Point q) { return std::abs(q.x - pointer_.x) + std::abs(q.y - pointer_.y); };
        if (!best || dist(*other) < dist(*best)) {
            best = other;
        }
    }
    return best;
}

} // namespace nagus
