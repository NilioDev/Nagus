// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/intel_screen.hpp"

#include "nagus/game/combat.hpp"
#include "nagus/game/diplomacy.hpp"

#include <algorithm>
#include <cctype>

namespace nagus {

namespace {

constexpr const char* kPrefixes = "chfkr";
constexpr const char* kFaces[kEmpires] = {"cardsn", "human", "ferngi", "klingn", "romuln"};
constexpr Pixel kText = 0xffffcc66u;
constexpr Pixel kDim = 0xff9098b0u;
constexpr Pixel kBad = 0xffff5050u;
constexpr Pixel kGood = 0xff60e070u;
constexpr Pixel kBarFull = 0xffff9900u;
constexpr Pixel kBarEmpty = 0xff303848u;
constexpr Pixel kChosenLine = 0xff304080u;
constexpr Pixel kEmpireColours[kEmpires] = {0xffe0c040u, 0xff60a0ffu, 0xffffa040u, 0xffff5050u, 0xff40e070u};
constexpr int kLine = 19;
// Lexicon numbers.
constexpr std::size_t kIntelligence = 548;
constexpr std::size_t kEspionage = 292;
constexpr std::size_t kSabotage = 1093;
constexpr std::size_t kSecurity = 550;     // "Internal Security"
constexpr std::size_t kIntelStrength = 547;
constexpr std::size_t kCreditsText = 193;
constexpr std::size_t kEmpireResearch = 269;
constexpr std::size_t kEmpirePopulation = 268;
constexpr std::size_t kMilitary = 642;
constexpr std::size_t kFocusTexts[kIntelFocuses] = {411, 1099, 261, 642}; // General, Science, Economy Dept, Military

std::string upper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string raceName(const Game& g, int race) {
    return race >= 0 && static_cast<std::size_t>(race) < g.raceNames.size() ? g.raceNames[static_cast<std::size_t>(race)]
                                                                             : std::string("?");
}

void bar(Canvas& canvas, const Rect& r, int value, int total, Pixel full) {
    canvas.fill(r, kBarEmpty);
    if (total > 0) {
        const int w = static_cast<int>(static_cast<long long>(r.w) * std::clamp(value, 0, total) / total);
        canvas.fill({r.x, r.y, w, r.h}, full);
    }
}

} // namespace

std::string intelText(const Lexicon& lex, const Game& g, const IntelReport& r) {
    const std::string kind = lex[r.sabotage ? kSabotage : kEspionage];
    std::string what;
    if (r.success) {
        const std::string v = std::to_string(r.value);
        if (!r.sabotage) {
            switch (r.focus) {
            case IntelFocus::General: what = "learned where " + v + " of their systems are"; break;
            case IntelFocus::Science: what = "took " + v + " research points"; break;
            case IntelFocus::Economy: what = "took " + v + " credits"; break;
            case IntelFocus::Military: what = "found " + v + " of their fleets"; break;
            }
        } else {
            switch (r.focus) {
            case IntelFocus::General: what = "stirred unrest, " + v + " million died"; break;
            case IntelFocus::Science: what = "destroyed " + v + " research points"; break;
            case IntelFocus::Economy: what = "destroyed " + v + " industry put into building"; break;
            case IntelFocus::Military: what = "damaged a ship"; break;
            }
        }
    } else {
        what = "caught";
    }
    std::string where;
    if (r.system >= 0 && static_cast<std::size_t>(r.system) < g.galaxy.systems.size()) {
        where = " at " + g.galaxy.systems[static_cast<std::size_t>(r.system)].name;
    }
    return kind + " (" + lex[kFocusTexts[static_cast<std::size_t>(r.focus)]] + ") by the " + raceName(g, r.attacker) +
           " against the " + raceName(g, r.target) + where + ", " + what;
}

IntelScreen::IntelScreen(Assets& assets, Game& game)
    : assets_(assets), game_(game), panel_(assets, std::string(1, prefix()) + "intel.wdf") {
    showTab(Tab::Management);
}

char IntelScreen::prefix() const { return kPrefixes[std::clamp(game_.player, 0, kEmpires - 1)]; }

const Font& IntelScreen::font(int size) const {
    return assets_.font(std::string(kFaces[std::clamp(game_.player, 0, kEmpires - 1)]) + std::to_string(size) + ".fnt");
}

EmpireState& IntelScreen::mine() { return game_.empires[static_cast<std::size_t>(std::clamp(game_.player, 0, kEmpires - 1))]; }

int IntelScreen::quarterEmpire(int quarter) const {
    int n = 0;
    for (int e = 0; e < kEmpires; ++e) {
        if (e != game_.player && n++ == quarter) {
            return e;
        }
    }
    return -1;
}

void IntelScreen::showTab(Tab t) {
    tab_ = t;
    const std::string p(1, prefix());
    const char* names[] = {"intelm.wdf", "intelr.wdf", "intele.wdf"};
    sub_ = std::make_unique<Panel>(assets_, p + names[static_cast<int>(t)]);
    const Rect area = panel_.rect(kArea);
    sub_->setOrigin({area.x, area.y});
    sub_->pointerMoved(pointer_);
    refresh();
}

void IntelScreen::refresh() {
    panel_.setOn(kManagement, tab_ == Tab::Management);
    panel_.setOn(kReport, tab_ == Tab::Report);
    panel_.setOn(kStatus, tab_ == Tab::Status);
    if (!sub_) {
        return;
    }
    if (tab_ == Tab::Management) {
        const EmpireState& e = mine();
        sub_->setEnabled(kSecurityLess, e.security > 0);
        sub_->setEnabled(kSecurityMore, e.security < 100);
        int planned = 0;
        for (const auto& p : e.intel) {
            planned += p.espionage + p.sabotage;
        }
        for (int q = 0; q < 4; ++q) {
            const int target = quarterEmpire(q);
            const bool known = target >= 0 && met(game_, game_.player, target);
            const auto u = static_cast<std::uint32_t>(q);
            const IntelPlan p = known ? e.intel[static_cast<std::size_t>(target)] : IntelPlan{};
            for (int f = 0; f < kIntelFocuses; ++f) {
                sub_->setEnabled(kFocus[f] + u, known);
                sub_->setOn(kFocus[f] + u, known && static_cast<int>(p.focus) == f);
            }
            for (int row = 0; row < 2; ++row) {
                const int value = row == 0 ? p.espionage : p.sabotage;
                sub_->setEnabled(kRowLock[row] + u, false);
                sub_->setEnabled(kRowLess[row] + u, known && value > 0);
                sub_->setEnabled(kRowMore[row] + u, known && planned < 100);
            }
        }
    } else if (tab_ == Tab::Report) {
        const int n = static_cast<int>(game_.intelReports.size());
        sub_->setEnabled(kReportUp, firstReport_ > 0);
        sub_->setEnabled(kReportDown, reportLine(n - 1) == std::nullopt && n > 0);
        for (const std::uint32_t id : {0x1069u, 0x106au, 0x106bu, 0x106cu, 0x106du, 0x1072u, 0x1073u}) {
            sub_->setEnabled(id, false);
        }
    }
}

std::vector<IntelReport> IntelScreen::reports() const {
    std::vector<IntelReport> out;
    for (auto it = game_.intelReports.rbegin(); it != game_.intelReports.rend(); ++it) {
        // The player knows of their own operations, and of others' only
        // when they were caught or felt at home.
        if (it->attacker == game_.player || (it->target == game_.player && (!it->success || it->sabotage))) {
            out.push_back(*it);
        }
    }
    return out;
}

std::optional<Rect> IntelScreen::reportLine(int i) const {
    if (!sub_ || tab_ != Tab::Report) {
        return std::nullopt;
    }
    const Rect list = sub_->rect(kReportList);
    const int row = i - firstReport_;
    if (i < 0 || row < 0 || i >= static_cast<int>(reports().size())) {
        return std::nullopt;
    }
    const Rect r{list.x + 4, list.y + 4 + row * kLine, list.w - 8, kLine};
    if (r.y + r.h > list.y + list.h) {
        return std::nullopt;
    }
    return r;
}

std::optional<Rect> IntelScreen::button(std::uint32_t id) const {
    if (sub_ && sub_->layout().find(id) != nullptr) {
        return sub_->rect(id);
    }
    if (panel_.layout().find(id) != nullptr) {
        return panel_.rect(id);
    }
    return std::nullopt;
}

void IntelScreen::click(std::uint32_t id) {
    if (sub_ && sub_->layout().find(id) != nullptr && sub_->usable(id)) {
        EmpireState& e = mine();
        if (tab_ == Tab::Management) {
            if (id == kSecurityLess || id == kSecurityMore) {
                setSecurity(e, e.security + (id == kSecurityMore ? kIntelStep : -kIntelStep));
            }
            for (int q = 0; q < 4; ++q) {
                const int target = quarterEmpire(q);
                const auto u = static_cast<std::uint32_t>(q);
                for (int f = 0; f < kIntelFocuses; ++f) {
                    if (id == kFocus[f] + u) {
                        setIntelFocus(e, target, static_cast<IntelFocus>(f));
                    }
                }
                for (int row = 0; row < 2; ++row) {
                    if (target < 0) {
                        continue;
                    }
                    const IntelPlan& p = e.intel[static_cast<std::size_t>(target)];
                    const int value = row == 0 ? p.espionage : p.sabotage;
                    if (id == kRowLess[row] + u) {
                        setIntelShare(e, target, row == 1, value - kIntelStep);
                    } else if (id == kRowMore[row] + u) {
                        setIntelShare(e, target, row == 1, value + kIntelStep);
                    }
                }
            }
        } else if (tab_ == Tab::Report) {
            if (id == kReportUp) {
                firstReport_ = std::max(0, firstReport_ - 1);
            } else if (id == kReportDown) {
                firstReport_ += 1;
            }
        }
        refresh();
        return;
    }
    if (panel_.layout().find(id) == nullptr || !panel_.usable(id)) {
        return;
    }
    switch (id) {
    case kManagement: showTab(Tab::Management); break;
    case kReport: showTab(Tab::Report); break;
    case kStatus: showTab(Tab::Status); break;
    case kSummary: outcome_ = Outcome::Back; break;
    default: break;
    }
}

void IntelScreen::draw(Canvas& canvas, std::uint64_t) {
    panel_.draw(canvas);
    if (sub_) {
        sub_->draw(canvas);
    }
    const Lexicon& lex = assets_.lexicon();
    const Font& f15 = font(15);
    const Font& f20 = font(20);
    const Font& f28 = font(28);
    const int me = game_.player;
    const EmpireState& e = mine();

    // On the left: the empire's intelligence.
    canvas.text(f28, upper(lex[kIntelligence]).substr(0, 5), panel_.rect(kTitle), kText, Align::Left);
    canvas.text(f20, raceName(game_, me), panel_.rect(kSubtitle), kEmpireColours[std::clamp(me, 0, kEmpires - 1)], Align::Left);
    canvas.text(f15, lex[kIntelStrength] + " " + std::to_string(e.lastIntel), panel_.rect(kSmall), kText, Align::Left);
    // What the plan makes of last turn's points.
    EmpireState last = e;
    last.intelPoints = e.lastIntel;
    int ops = 0;
    for (int t = 0; t < kEmpires; ++t) {
        ops += operationPoints(last, t, false) + operationPoints(last, t, true);
    }
    const std::string lines[7] = {
        lex[kSecurity],
        std::to_string(securityPoints(last)) + " a turn",
        "Operations",
        std::to_string(ops) + " a turn",
        "",
        "",
        "",
    };
    for (int i = 0; i < 7; ++i) {
        canvas.text(f20, lines[i], panel_.rect(kLines[i]), i % 2 == 0 ? kDim : kText, Align::Left);
    }
    if (!sub_) {
        return;
    }

    switch (tab_) {
    case Tab::Management: {
        canvas.text(f28, upper(lex[kIntelligence]), sub_->rect(kPlanTitle), kText, Align::Center);
        bar(canvas, sub_->rect(kSecurityBar), e.security, 100, kBarFull);
        canvas.text(f15, lex[kSecurity], sub_->rect(kSecurityBar), 0xff000000u, Align::Center);
        canvas.text(f20, std::to_string(e.security) + "%", sub_->rect(kSecurityText), kText, Align::Center);
        for (int q = 0; q < 4; ++q) {
            const int target = quarterEmpire(q);
            const auto u = static_cast<std::uint32_t>(q);
            const bool known = target >= 0 && met(game_, me, target);
            canvas.text(f20, known ? raceName(game_, target) : "?", sub_->rect(kQuarterName + u),
                        known ? kEmpireColours[target] : kDim, Align::Left);
            if (!known) {
                continue;
            }
            const IntelPlan& p = e.intel[static_cast<std::size_t>(target)];
            for (int row = 0; row < 2; ++row) {
                const int value = row == 0 ? p.espionage : p.sabotage;
                canvas.text(f15, lex[row == 0 ? kEspionage : kSabotage], sub_->rect(kRowLabels[row][q]), kText, Align::Left);
                bar(canvas, sub_->rect(kRowBar[row] + u), value, 100, kBarFull);
                canvas.text(f15, std::to_string(value) + "%", sub_->rect(kRowText[row] + u), kText, Align::Right);
            }
            const Treaty t = treaty(game_, me, target);
            const int pool = e.espionagePool[static_cast<std::size_t>(target)] + e.sabotagePool[static_cast<std::size_t>(target)];
            canvas.text(f15, std::to_string(pool) + " of " + std::to_string(kOperationCost), sub_->rect(kQuarterStatus[q]),
                        t == Treaty::War ? kBad : kText, Align::Left);
        }
        break;
    }
    case Tab::Report: {
        const auto list = reports();
        canvas.text(f20, upper(lex[kIntelligence]), sub_->rect(kReportTitle), kText, Align::Left);
        for (int i = 0; i < static_cast<int>(list.size()); ++i) {
            const auto r = reportLine(i);
            if (!r) {
                continue;
            }
            const IntelReport& rep = list[static_cast<std::size_t>(i)];
            if (i == chosenReport_) {
                canvas.fill(*r, kChosenLine);
            }
            // Under the column headings: turns ago, source, department,
            // target and location; the department shows green or red for
            // how it went for the player.
            const Pixel colour = rep.attacker == me ? (rep.success ? kGood : kBad) : (rep.success ? kBad : kGood);
            auto column = [&](std::uint32_t id) { return sub_->rect(id).x + 2; };
            canvas.text(f15, std::to_string(std::max(0, game_.turn - rep.turn)), column(0x1069), r->y + 2, kDim);
            canvas.text(f15, raceName(game_, rep.attacker), column(0x106a), r->y + 2, kEmpireColours[std::clamp(rep.attacker, 0, kEmpires - 1)]);
            canvas.text(f15, lex[kFocusTexts[static_cast<std::size_t>(rep.focus)]], column(0x106b), r->y + 2, colour);
            canvas.text(f15, raceName(game_, rep.target), column(0x106c), r->y + 2, kEmpireColours[std::clamp(rep.target, 0, kEmpires - 1)]);
            if (rep.system >= 0 && static_cast<std::size_t>(rep.system) < game_.galaxy.systems.size()) {
                canvas.text(f15, game_.galaxy.systems[static_cast<std::size_t>(rep.system)].name, column(0x106d), r->y + 2, kText);
            }
        }
        if (chosenReport_ < static_cast<int>(list.size())) {
            drawParagraph(canvas, f15, intelText(lex, game_, list[static_cast<std::size_t>(chosenReport_)]),
                          sub_->rect(kReportText), kText);
        }
        break;
    }
    case Tab::Status: {
        // Six tables, each empire ranked: what the player knows of the
        // empires it has met.
        std::array<std::array<int, kEmpires>, 6> values{};
        for (int x = 0; x < kEmpires; ++x) {
            const EmpireState& s = game_.empires[static_cast<std::size_t>(x)];
            int people = 0;
            int colonies = 0;
            for (const auto& c : game_.colonies) {
                if (c.owner == x) {
                    people += c.population;
                    colonies += 1;
                }
            }
            int strength = 0;
            for (const auto& f : game_.fleets) {
                for (const auto& sh : f.ships) {
                    if (f.owner == x) {
                        strength += shipFirepower(game_.economy.ships[static_cast<std::size_t>(sh.shipClass)], s);
                    }
                }
            }
            int levels = 0;
            for (const int t : s.tech) {
                levels += t;
            }
            const auto i = static_cast<std::size_t>(x);
            values[0][i] = s.credits;
            values[1][i] = levels;
            values[2][i] = people;
            values[3][i] = colonies;
            values[4][i] = strength;
            values[5][i] = s.lastIntel;
        }
        const std::string titles[6] = {lex[kCreditsText], lex[kEmpireResearch], lex[kEmpirePopulation],
                                       "Systems",         lex[kMilitary],       lex[kIntelligence]};
        for (int t = 0; t < 6; ++t) {
            canvas.text(f20, titles[t], sub_->rect(kTableTitle[t]), kText, Align::Left);
            std::array<int, kEmpires> order{0, 1, 2, 3, 4};
            const auto& v = values[static_cast<std::size_t>(t)];
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return v[static_cast<std::size_t>(a)] > v[static_cast<std::size_t>(b)]; });
            const int top = std::max(1, v[static_cast<std::size_t>(order[0])]);
            for (int row = 0; row < kEmpires; ++row) {
                const int x = order[static_cast<std::size_t>(row)];
                const auto u = static_cast<std::uint32_t>(row);
                const bool known = x == me || met(game_, me, x);
                canvas.text(f15, std::to_string(row + 1), sub_->rect(kTableRank[t] + u), kText, Align::Center);
                const Rect name = sub_->rect(kTableName[t] + u);
                if (t >= 4) {
                    // These tables show bars.
                    bar(canvas, name, known ? v[static_cast<std::size_t>(x)] : 0, top, known ? kEmpireColours[x] : kDim);
                } else {
                    canvas.text(f15, known ? raceName(game_, x) : "?", {name.x + 4, name.y, name.w - 8, name.h},
                                known ? kEmpireColours[x] : kDim, Align::Left);
                }
                canvas.text(f15, known ? std::to_string(v[static_cast<std::size_t>(x)]) : "?", sub_->rect(kTableValue[t] + u),
                            kText, Align::Right);
            }
        }
        break;
    }
    }
}

void IntelScreen::pointerMoved(Point p) {
    pointer_ = p;
    panel_.pointerMoved(p);
    if (sub_) {
        sub_->pointerMoved(p);
    }
}

void IntelScreen::pointerPressed(Point p) {
    pointer_ = p;
    panel_.pointerPressed(p);
    if (sub_) {
        sub_->pointerPressed(p);
    }
}

void IntelScreen::pointerReleased(Point p) {
    pointer_ = p;
    if (sub_) {
        if (const auto id = sub_->pointerReleased(p)) {
            click(*id);
            return;
        }
    }
    if (const auto id = panel_.pointerReleased(p)) {
        click(*id);
        return;
    }
    for (int i = 0; i < static_cast<int>(game_.intelReports.size()); ++i) {
        if (const auto r = reportLine(i); r && r->contains(p.x, p.y)) {
            chosenReport_ = i;
            return;
        }
    }
}

std::optional<Point> IntelScreen::command(Command c) {
    if (c == Command::Back) {
        outcome_ = Outcome::Back;
        return std::nullopt;
    }
    if (c == Command::Next || c == Command::Previous) {
        const int n = 3;
        showTab(static_cast<Tab>((static_cast<int>(tab_) + (c == Command::Next ? 1 : n - 1)) % n));
        return std::nullopt;
    }
    std::optional<Point> best = panel_.step(c, pointer_);
    if (sub_) {
        if (const auto other = sub_->step(c, pointer_)) {
            auto dist = [&](Point q) { return std::abs(q.x - pointer_.x) + std::abs(q.y - pointer_.y); };
            if (!best || dist(*other) < dist(*best)) {
                best = other;
            }
        }
    }
    return best;
}

} // namespace nagus
