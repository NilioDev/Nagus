// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/summary_screen.hpp"

#include "nagus/game/diplomacy.hpp"
#include "nagus/game/economy.hpp"
#include "nagus/ui/diplomacy_screen.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr char kPrefixes[kEmpires] = {'c', 'h', 'f', 'k', 'r'};
constexpr Pixel kText = 0xffffcc66u;
constexpr Pixel kHead = 0xff000000u;
constexpr Pixel kChosen = 0xffffffffu;
constexpr const char* kFont = "human15.fnt";
constexpr int kRowHeight = 20;
// The three columns' left edges and widths, and the header bar, in the
// layout (under the coloured bars of the background).
constexpr int kColumnX[3] = {186, 356, 489};
constexpr int kColumnW[3] = {164, 128, 190};
constexpr int kHeaderY = 172;
constexpr int kHeaderH = 24;
// Lexicon numbers.
constexpr std::size_t kEventSector = 305; // "EVENT|SECTOR"
constexpr std::size_t kPop = 922;
constexpr std::size_t kCurrentBuild = 208;

std::string raceName(const Game& g, int race) {
    return race >= 0 && static_cast<std::size_t>(race) < g.raceNames.size() ? g.raceNames[static_cast<std::size_t>(race)] : "?";
}

} // namespace

SummaryScreen::SummaryScreen(Assets& assets, const Game& game, const std::vector<std::pair<std::string, Pixel>>& events, Page page)
    : assets_(assets), game_(game), events_(events),
      panel_(assets, std::string(1, kPrefixes[std::clamp(game.player, 0, kEmpires - 1)]) + "summary.wdf") {
    const Rect r = panel_.layout().rect;
    panel_.setOrigin({(kScreenWidth - r.w) / 2, (kScreenHeight - r.h) / 2});
    show(page);
}

void SummaryScreen::show(Page page) {
    page_ = page;
    rows_.clear();
    first_ = 0;
    row_ = 0;
    const Font& font = assets_.font(kFont);
    const Lexicon& lex = assets_.lexicon();
    switch (page) {
    case Page::Events:
        for (const auto& [text, colour] : events_) {
            for (const auto& line : wrapText(font, text, panel_.rect(kList).w - 8)) {
                rows_.push_back({{line, "", ""}, colour, -1});
            }
        }
        break;
    case Page::Relations:
        for (int race = 0; race < static_cast<int>(game_.raceNames.size()); ++race) {
            if (race == game_.player || !met(game_, game_.player, race)) {
                continue;
            }
            rows_.push_back({{raceName(game_, race), treatyName(lex, treaty(game_, game_.player, race)),
                              std::to_string(regard(game_, race, game_.player))},
                             atWar(game_, game_.player, race) ? 0xffff6050u : kText, -1});
        }
        break;
    case Page::Systems:
        for (std::size_t i = 0; i < game_.colonies.size(); ++i) {
            const Colony& c = game_.colonies[i];
            if (c.owner != game_.player || c.system < 0 || static_cast<std::size_t>(c.system) >= game_.galaxy.systems.size()) {
                continue;
            }
            const std::string building = c.queue.empty() ? "-" : itemName(c.queue.front(), game_.economy);
            rows_.push_back({{game_.galaxy.systems[static_cast<std::size_t>(c.system)].name,
                              std::to_string(c.population) + " / " + std::to_string(c.maxPopulation), building},
                             kText, static_cast<int>(i)});
        }
        break;
    }
    panel_.setOn(kEvents, page == Page::Events);
    panel_.setOn(kRelations, page == Page::Relations);
    panel_.setOn(kSystems, page == Page::Systems);
    scrollBy(0);
}

int SummaryScreen::visibleRows() const { return std::max(1, panel_.rect(kList).h / kRowHeight); }

void SummaryScreen::scrollBy(int rows) {
    const int most = std::max(0, static_cast<int>(rows_.size()) - visibleRows());
    first_ = std::clamp(first_ + rows, 0, most);
    panel_.setEnabled(kUp, first_ > 0);
    panel_.setEnabled(kDown, first_ < most);
}

std::optional<Rect> SummaryScreen::rowRect(std::size_t row) const {
    const int at = static_cast<int>(row) - first_;
    if (row >= rows_.size() || at < 0 || at >= visibleRows()) {
        return std::nullopt;
    }
    const Rect list = panel_.rect(kList);
    return Rect{list.x, list.y + at * kRowHeight, list.w, kRowHeight};
}

void SummaryScreen::draw(Canvas& canvas, std::uint64_t) {
    canvas.blend({0, 0, canvas.width(), canvas.height()}, rgb(0, 0, 0), 120);
    panel_.draw(canvas);
    const Font& font = assets_.font(kFont);
    const Lexicon& lex = assets_.lexicon();
    const Point o = panel_.origin();
    std::array<std::string, 3> heads;
    switch (page_) {
    case Page::Events: {
        const std::string both = lex[kEventSector];
        heads[0] = both.substr(0, both.find('|'));
        break;
    }
    case Page::Relations: heads = {"RACE", "TREATY", "REGARD"}; break;
    case Page::Systems: heads = {"SYSTEM", lex[kPop], lex[kCurrentBuild]}; break;
    }
    for (std::size_t c = 0; c < 3; ++c) {
        canvas.text(font, heads[c], {o.x + kColumnX[c] + 4, o.y + kHeaderY, kColumnW[c] - 8, kHeaderH}, kHead, Align::Left);
    }
    for (std::size_t r = 0; r < rows_.size(); ++r) {
        const auto box = rowRect(r);
        if (!box) {
            continue;
        }
        const Row& row = rows_[r];
        if (page_ == Page::Events) {
            canvas.text(font, row.cells[0], {box->x + 4, box->y, box->w - 8, box->h}, row.colour, Align::Left);
        } else {
            for (std::size_t c = 0; c < 3; ++c) {
                canvas.text(font, row.cells[c], {o.x + kColumnX[c] + 4, box->y, kColumnW[c] - 8, box->h}, row.colour, Align::Left);
            }
        }
        if (static_cast<int>(r) == row_ && page_ == Page::Systems) {
            canvas.frame(*box, kChosen, 1);
        }
    }
}

void SummaryScreen::pointerMoved(Point p) {
    pointer_ = p;
    panel_.pointerMoved(p);
    if (page_ == Page::Systems) {
        for (std::size_t r = 0; r < rows_.size(); ++r) {
            if (const auto box = rowRect(r); box && box->contains(p.x, p.y)) {
                row_ = static_cast<int>(r);
            }
        }
    }
}

void SummaryScreen::pointerReleased(Point p) {
    pointer_ = p;
    if (const auto id = panel_.pointerReleased(p)) {
        assets_.clicked();
        switch (*id) {
        case kEvents: show(Page::Events); break;
        case kRelations: show(Page::Relations); break;
        case kSystems: show(Page::Systems); break;
        case kClose: outcome_ = Outcome::Back; break;
        case kUp: scrollBy(-visibleRows() + 1); break;
        case kDown: scrollBy(visibleRows() - 1); break;
        default: break;
        }
        return;
    }
    if (page_ != Page::Systems) {
        return;
    }
    for (std::size_t r = 0; r < rows_.size(); ++r) {
        if (const auto box = rowRect(r); box && box->contains(p.x, p.y)) {
            assets_.clicked();
            chosenColony_ = rows_[r].colony;
            outcome_ = Outcome::Open;
        }
    }
}

std::optional<Point> SummaryScreen::command(Command c) {
    switch (c) {
    case Command::Back: outcome_ = Outcome::Back; return std::nullopt;
    case Command::Next: show(page_ == Page::Systems ? Page::Events : static_cast<Page>(static_cast<int>(page_) + 1)); return std::nullopt;
    case Command::Previous: show(page_ == Page::Events ? Page::Systems : static_cast<Page>(static_cast<int>(page_) - 1)); return std::nullopt;
    case Command::Up:
    case Command::Down: {
        if (page_ != Page::Systems) {
            // The list scrolls; the pointer stays.
            scrollBy(c == Command::Down ? 1 : -1);
            return std::nullopt;
        }
        if (rows_.empty()) {
            return std::nullopt;
        }
        row_ = std::clamp(row_ + (c == Command::Down ? 1 : -1), 0, static_cast<int>(rows_.size()) - 1);
        if (row_ < first_) {
            scrollBy(row_ - first_);
        } else if (row_ >= first_ + visibleRows()) {
            scrollBy(row_ - first_ - visibleRows() + 1);
        }
        const Rect box = *rowRect(static_cast<std::size_t>(row_));
        return Point{box.x + 20, box.centerY()};
    }
    default: return panel_.step(c, pointer_);
    }
}

} // namespace nagus
