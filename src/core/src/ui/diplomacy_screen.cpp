// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/diplomacy_screen.hpp"

#include <algorithm>
#include <cctype>

namespace nagus {

namespace {

constexpr const char* kPrefixes = "chfkr";
constexpr const char* kFaces[kEmpires] = {"cardsn", "human", "ferngi", "klingn", "romuln"};
constexpr Pixel kText = 0xffffcc66u;
constexpr Pixel kDim = 0xff9098b0u;
constexpr Pixel kChosenLine = 0xff304080u;
constexpr Pixel kWarColour = 0xffff5050u;
constexpr Pixel kEmpireColours[kEmpires] = {0xffe0c040u, 0xff60a0ffu, 0xffffa040u, 0xffff5050u, 0xff40e070u};
constexpr Pixel kMinorColour = 0xffd0d0d0u;
constexpr int kLine = 19;
constexpr int kGiftStep = 50;
constexpr int kMinGift = 50;
// Lexicon numbers.
constexpr std::size_t kTreatyNames[kTreaties] = {832, 1420, 809, 861, 388, 15, 19, 639};
constexpr std::size_t kGiveGift = 416;   // "Give Gift"
constexpr std::size_t kDeclareWarText = 214; // "Declare War"
constexpr std::size_t kNewPeace = 817;   // "New Peace Treaty"
constexpr std::size_t kTreatyTerms = 1369; // "TREATY TERMS"

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    for (std::size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) {
        s.replace(at, from.size(), to);
    }
    return s;
}

std::string upper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string raceName(const Game& g, int race) {
    return race >= 0 && static_cast<std::size_t>(race) < g.raceNames.size() ? g.raceNames[static_cast<std::size_t>(race)]
                                                                             : std::string();
}

Pixel raceColour(int race) { return race >= 0 && race < kEmpires ? kEmpireColours[race] : kMinorColour; }

} // namespace

std::string treatyName(const Lexicon& lex, Treaty t) { return lex[kTreatyNames[static_cast<std::size_t>(t)]]; }

std::string diplomacyText(const Lexicon& lex, const Game& g, const DiplomacyEvent& e) {
    const std::string a = raceName(g, e.a);
    const std::string b = raceName(g, e.b);
    if (e.text == kTextMet) {
        return "First contact between the " + a + " and the " + b;
    }
    if (e.text == kTextJoined) {
        return "The " + a + " joined the " + b;
    }
    std::string s = lex[e.text];
    s = replaceAll(s, "%s1", a);
    s = replaceAll(s, "%1", a);
    s = replaceAll(s, "%s", a);
    s = replaceAll(s, "%2", b);
    return s;
}

DiplomacyScreen::DiplomacyScreen(Assets& assets, Game& game)
    : assets_(assets), game_(game), panel_(assets, std::string(1, prefix()) + "diplomt.wdf") {
    for (int r = 0; r < static_cast<int>(game_.races.size()); ++r) {
        if (r != game_.player && met(game_, game_.player, r)) {
            races_.push_back(r);
        }
    }
    // The first race with a proposal for the player, or the first met.
    bool offered = false;
    for (const auto& o : game_.offers) {
        offered = offered || o.to == game_.player;
        if (chosen_ < 0 && o.to == game_.player) {
            chosen_ = o.from;
        }
    }
    if (chosen_ < 0 && !races_.empty()) {
        chosen_ = races_.front();
    }
    for (const std::uint32_t id : {kRequest, kWarPact}) {
        panel_.setEnabled(id, false);
    }
    showTab(offered ? Tab::Active : Tab::Propose);
}

char DiplomacyScreen::prefix() const { return kPrefixes[std::clamp(game_.player, 0, kEmpires - 1)]; }

const Font& DiplomacyScreen::font(int size) const {
    return assets_.font(std::string(kFaces[std::clamp(game_.player, 0, kEmpires - 1)]) + std::to_string(size) + ".fnt");
}

void DiplomacyScreen::showTab(Tab t) {
    tab_ = t;
    dialog_ = Dialog::None;
    openSub();
}

void DiplomacyScreen::choose(int race) {
    if (std::find(races_.begin(), races_.end(), race) == races_.end()) {
        return;
    }
    chosen_ = race;
    dialog_ = Dialog::None;
    reply_.clear();
    openSub();
}

void DiplomacyScreen::openSub() {
    const std::string p(1, prefix());
    std::string name;
    switch (dialog_) {
    case Dialog::Peace: name = p + "d_peace.wdf"; break;
    case Dialog::Gift: name = p + "d_gift.wdf"; break;
    case Dialog::War: name = p + "d_dwar.wdf"; break;
    case Dialog::None:
        switch (tab_) {
        case Tab::Active: name = p + "d_actev.wdf"; break;
        case Tab::Propose: name = p + (isMinor(game_, chosen_) ? "dminbtn.wdf" : "dmajbtn.wdf"); break;
        case Tab::RaceInfo: name = p + "d_rinfo.wdf"; break;
        case Tab::Events: break;
        }
        break;
    }
    sub_.reset();
    if (!name.empty() && chosen_ >= 0) {
        sub_ = std::make_unique<Panel>(assets_, name);
        const Rect area = panel_.rect(kArea);
        // The proposal buttons sit in the middle of the area.
        const bool buttons = dialog_ == Dialog::None && tab_ == Tab::Propose;
        sub_->setOrigin({area.x + (buttons ? 150 : 0), area.y + (buttons ? 190 : 0)});
        sub_->pointerMoved(pointer_);
    }
    refresh();
}

void DiplomacyScreen::refresh() {
    panel_.setOn(kActive, tab_ == Tab::Active);
    panel_.setOn(kPropose, tab_ == Tab::Propose);
    panel_.setOn(kEvents, tab_ == Tab::Events);
    panel_.setOn(kRaceInfo, tab_ == Tab::RaceInfo);
    panel_.setEnabled(kUp, first_ > 0);
    panel_.setEnabled(kDown, raceLine(static_cast<int>(races_.size()) - 1) == std::nullopt && !races_.empty());
    if (!sub_) {
        return;
    }
    const int me = game_.player;
    const int them = chosen_;
    const int credits = game_.empires[static_cast<std::size_t>(std::clamp(me, 0, kEmpires - 1))].credits;
    for (const auto& e : sub_->layout().elements) {
        sub_->setEnabled(e.id, false);
    }
    auto enable = [&](std::uint32_t id, bool on) { sub_->setEnabled(id, on); };
    switch (dialog_) {
    case Dialog::Peace:
        enable(kNonAggression, canPropose(game_, me, them, Treaty::NonAggression));
        enable(kFriendship, canPropose(game_, me, them, Treaty::Friendship));
        enable(kAffiliation, canPropose(game_, me, them, Treaty::Affiliation));
        sub_->setOn(kNonAggression, peace_ == Treaty::NonAggression);
        sub_->setOn(kFriendship, peace_ == Treaty::Friendship);
        sub_->setOn(kAffiliation, peace_ == Treaty::Affiliation);
        enable(kSend, canPropose(game_, me, them, peace_));
        enable(kCancel, true);
        sub_->setVisible(0x1454, false);
        break;
    case Dialog::Gift:
        enable(kLess, gift_ > kMinGift);
        enable(kMore, gift_ + kGiftStep <= credits);
        enable(kSend, gift_ <= credits);
        enable(kCancel, true);
        sub_->setVisible(0x1454, false);
        break;
    case Dialog::War:
        enable(kSend, canDeclareWar(game_, me, them));
        enable(kCancel, true);
        sub_->setVisible(0x1454, false);
        break;
    case Dialog::None:
        if (tab_ == Tab::Propose) {
            const bool war = atWar(game_, me, them);
            enable(kGift, met(game_, me, them) && credits >= kMinGift && treaty(game_, me, them) != Treaty::Membership);
            enable(kDeclareWar, canDeclareWar(game_, me, them));
            if (isMinor(game_, them)) {
                enable(kFriendship, canPropose(game_, me, them, Treaty::Friendship));
                enable(kAffiliation, canPropose(game_, me, them, Treaty::Affiliation));
                enable(kMembership, canPropose(game_, me, them, Treaty::Membership));
            } else {
                enable(kPeaceTreaty, canPropose(game_, me, them, Treaty::NonAggression) ||
                                         canPropose(game_, me, them, Treaty::Friendship) ||
                                         canPropose(game_, me, them, Treaty::Affiliation));
                enable(kAlliance, canPropose(game_, me, them, Treaty::Alliance));
                // Declare war and surrender share a place.
                sub_->setVisible(kDeclareWar, !war);
                sub_->setVisible(kSurrender, war);
            }
        } else if (tab_ == Tab::Active) {
            const bool offer = std::any_of(game_.offers.begin(), game_.offers.end(), [&](const Offer& o) { return o.from == them && o.to == game_.player; });
            enable(kAccept, offer);
            enable(kReject, offer);
            sub_->setVisible(0x1542, false);
            sub_->setVisible(0x13a0, false);
        }
        break;
    }
}

std::optional<Rect> DiplomacyScreen::raceLine(int i) const {
    const Rect list = panel_.rect(kList);
    const int row = i - first_;
    if (i < 0 || i >= static_cast<int>(races_.size()) || row < 0) {
        return std::nullopt;
    }
    const Rect r{list.x + 8, list.y + 6 + row * kLine, list.w - 16, kLine};
    if (r.y + r.h > list.y + list.h) {
        return std::nullopt;
    }
    return r;
}

std::optional<Rect> DiplomacyScreen::button(std::uint32_t id) const {
    if (sub_ && sub_->layout().find(id) != nullptr) {
        return sub_->rect(id);
    }
    if (panel_.layout().find(id) != nullptr) {
        return panel_.rect(id);
    }
    return std::nullopt;
}

std::vector<DiplomacyEvent> DiplomacyScreen::takeEvents() {
    std::vector<DiplomacyEvent> out;
    out.swap(events_);
    return out;
}

void DiplomacyScreen::send(Treaty t) {
    const std::size_t before = events_.size();
    const Reply r = propose(game_, game_.player, chosen_, t, events_);
    if (events_.size() > before) {
        reply_ = diplomacyText(assets_.lexicon(), game_, events_.back());
    } else {
        reply_ = r == Reply::Accepted ? "Accepted" : "Rejected";
    }
}

void DiplomacyScreen::click(std::uint32_t id) {
    const int me = game_.player;
    if (sub_ && sub_->layout().find(id) != nullptr && sub_->usable(id)) {
        switch (dialog_) {
        case Dialog::Peace:
            if (id == kNonAggression || id == kFriendship || id == kAffiliation) {
                peace_ = id == kNonAggression ? Treaty::NonAggression : id == kFriendship ? Treaty::Friendship : Treaty::Affiliation;
            } else if (id == kSend) {
                send(peace_);
                dialog_ = Dialog::None;
            } else if (id == kCancel || id == 0x1454) {
                dialog_ = Dialog::None;
            }
            break;
        case Dialog::Gift:
            if (id == kLess || id == kMore) {
                gift_ += id == kMore ? kGiftStep : -kGiftStep;
            } else if (id == kSend) {
                if (giveGift(game_, me, chosen_, gift_, events_)) {
                    reply_ = diplomacyText(assets_.lexicon(), game_, events_.back());
                }
                dialog_ = Dialog::None;
            } else if (id == kCancel || id == 0x1454) {
                dialog_ = Dialog::None;
            }
            break;
        case Dialog::War:
            if (id == kSend) {
                declareWar(game_, me, chosen_, events_);
                if (!events_.empty()) {
                    reply_ = diplomacyText(assets_.lexicon(), game_, events_.back());
                }
            }
            dialog_ = Dialog::None;
            break;
        case Dialog::None:
            if (tab_ == Tab::Propose) {
                if (id == kPeaceTreaty) {
                    dialog_ = Dialog::Peace;
                    for (const Treaty t : {Treaty::Affiliation, Treaty::Friendship, Treaty::NonAggression}) {
                        if (canPropose(game_, me, chosen_, t) && wouldAccept(game_, me, chosen_, t)) {
                            peace_ = t;
                        }
                    }
                    if (!canPropose(game_, me, chosen_, peace_)) {
                        peace_ = Treaty::NonAggression;
                    }
                } else if (id == kAlliance) {
                    send(Treaty::Alliance);
                } else if (id == kDeclareWar) {
                    dialog_ = Dialog::War;
                } else if (id == kGift) {
                    dialog_ = Dialog::Gift;
                    gift_ = std::clamp(gift_, kMinGift, std::max(kMinGift, game_.empires[static_cast<std::size_t>(me)].credits));
                } else if (id == kFriendship) {
                    send(Treaty::Friendship);
                } else if (id == kAffiliation) {
                    send(Treaty::Affiliation);
                } else if (id == kMembership) {
                    send(Treaty::Membership);
                }
            } else if (tab_ == Tab::Active && (id == kAccept || id == kReject)) {
                for (std::size_t i = 0; i < game_.offers.size(); ++i) {
                    if (game_.offers[i].from == chosen_ && game_.offers[i].to == game_.player) {
                        answerOffer(game_, i, id == kAccept, events_);
                        if (!events_.empty()) {
                            reply_ = diplomacyText(assets_.lexicon(), game_, events_.back());
                        }
                        break;
                    }
                }
            }
            break;
        }
        openSub();
        return;
    }
    if (panel_.layout().find(id) == nullptr || !panel_.usable(id)) {
        return;
    }
    switch (id) {
    case kActive: showTab(Tab::Active); break;
    case kPropose: showTab(Tab::Propose); break;
    case kEvents: showTab(Tab::Events); break;
    case kRaceInfo: showTab(Tab::RaceInfo); break;
    case kSummary: outcome_ = Outcome::Back; break;
    case kUp: first_ = std::max(0, first_ - 1); refresh(); break;
    case kDown: first_ += 1; refresh(); break;
    default: break;
    }
}

void DiplomacyScreen::draw(Canvas& canvas, std::uint64_t) {
    panel_.draw(canvas);
    if (sub_) {
        sub_->draw(canvas);
    }
    const Lexicon& lex = assets_.lexicon();
    const Font& f15 = font(15);
    const Font& f20 = font(20);
    const Font& f28 = font(28);
    const int me = game_.player;

    // The bottom list: every race met, its treaty and regard.
    for (int i = 0; i < static_cast<int>(races_.size()); ++i) {
        const auto r = raceLine(i);
        if (!r) {
            continue;
        }
        const int race = races_[static_cast<std::size_t>(i)];
        if (race == chosen_) {
            canvas.fill(*r, kChosenLine);
        }
        const Treaty t = treaty(game_, me, race);
        canvas.text(f15, raceName(game_, race), r->x + 6, r->y + 2, raceColour(race));
        canvas.text(f15, treatyName(lex, t), r->x + 260, r->y + 2, t == Treaty::War ? kWarColour : kText);
        canvas.text(f15, std::to_string(regard(game_, race, me)), {r->x, r->y + 2, r->w - 8, r->h}, kText, Align::Right);
    }
    if (races_.empty()) {
        const Rect list = panel_.rect(kList);
        canvas.text(f15, lex[kTreatyNames[0]], {list.x + 14, list.y + 8, list.w - 20, kLine}, kDim, Align::Left);
        return;
    }

    // On the left: the chosen race.
    const Treaty t = treaty(game_, me, chosen_);
    canvas.text(f28, upper(raceName(game_, chosen_)), panel_.rect(kName), raceColour(chosen_), Align::Left);
    canvas.text(f20, treatyName(lex, t), panel_.rect(kStatus), t == Treaty::War ? kWarColour : kText, Align::Left);
    if (static_cast<std::size_t>(chosen_) < game_.races.size()) {
        const Image& pic = assets_.image(game_.races[static_cast<std::size_t>(chosen_)].picture);
        const Rect place = panel_.rect(kPortrait);
        canvas.draw(pic, place.x + (place.w - pic.width) / 2, place.y + (place.h - pic.height) / 2);
    }
    canvas.text(f20, "Regard " + std::to_string(regard(game_, chosen_, me)), panel_.rect(kRegard), kText, Align::Center);
    // At the top: the answer to the last proposal.
    const Rect top = panel_.rect(kTop);
    drawParagraph(canvas, f15, reply_, {top.x + 8, top.y + 8, top.w - 16, top.h - 16}, kText);

    const Rect area = panel_.rect(kArea);
    if (dialog_ != Dialog::None && sub_) {
        std::string title;
        std::string text;
        if (dialog_ == Dialog::Peace) {
            title = lex[kNewPeace];
            text = lex[kTreatyTerms] + "  " + treatyName(lex, peace_);
        } else if (dialog_ == Dialog::Gift) {
            title = lex[kGiveGift];
            text = raceName(game_, chosen_);
            canvas.text(f15, std::to_string(gift_), sub_->rect(kAmount), kText, Align::Center);
        } else {
            title = lex[kDeclareWarText];
            text = diplomacyText(lex, game_, {278, me, chosen_});
        }
        canvas.text(f20, title, sub_->rect(kDialogTitle), kText, Align::Center);
        const Rect box = sub_->rect(kDialogText);
        drawParagraph(canvas, f15, text, box, kText);
        return;
    }
    switch (tab_) {
    case Tab::Active: {
        const auto offer = std::find_if(game_.offers.begin(), game_.offers.end(), [&](const Offer& o) { return o.from == chosen_ && o.to == game_.player; });
        if (sub_ && offer != game_.offers.end()) {
            canvas.text(f20, treatyName(lex, offer->treaty), sub_->rect(kOfferTitle), kText, Align::Left);
            const std::size_t texts[kTreaties] = {0, 0, 0, 868, 385, 11, 24, 635};
            const DiplomacyEvent e{texts[static_cast<std::size_t>(offer->treaty)], offer->from, me};
            drawParagraph(canvas, f15, diplomacyText(lex, game_, e), sub_->rect(kOfferText), kText);
        } else if (sub_) {
            drawParagraph(canvas, f15, "No proposals from the " + raceName(game_, chosen_) + ".", sub_->rect(kOfferText), kDim);
        }
        break;
    }
    case Tab::Events: {
        // The treaties between the empires the player knows.
        int y = area.y + 110;
        for (int a = 0; a < kEmpires; ++a) {
            for (int b = a + 1; b < kEmpires; ++b) {
                const bool known = (a == me || met(game_, me, a)) && (b == me || met(game_, me, b)) && met(game_, a, b);
                if (!known) {
                    continue;
                }
                const Treaty ab = treaty(game_, a, b);
                canvas.text(f15, raceName(game_, a), area.x + 30, y, raceColour(a));
                canvas.text(f15, raceName(game_, b), area.x + 180, y, raceColour(b));
                canvas.text(f15, treatyName(lex, ab), area.x + 330, y, ab == Treaty::War ? kWarColour : kText);
                y += kLine;
            }
        }
        for (int r = kEmpires; r < static_cast<int>(game_.races.size()); ++r) {
            for (int e = 0; e < kEmpires; ++e) {
                if (treaty(game_, e, r) == Treaty::Membership && met(game_, me, r)) {
                    canvas.text(f15, raceName(game_, r), area.x + 30, y, kMinorColour);
                    canvas.text(f15, raceName(game_, e), area.x + 180, y, raceColour(e));
                    canvas.text(f15, treatyName(lex, Treaty::Membership), area.x + 330, y, kText);
                    y += kLine;
                }
            }
        }
        break;
    }
    case Tab::RaceInfo:
        if (sub_ && static_cast<std::size_t>(chosen_) < game_.races.size()) {
            const RaceInfo& info = game_.races[static_cast<std::size_t>(chosen_)];
            canvas.text(f20, raceName(game_, chosen_), sub_->rect(kInfoTitle), kText, Align::Center);
            std::string text;
            if (assets_.resources().has("racedesc.rst")) {
                text = textAt(assets_.resources().file("racedesc.rst"), info.description);
            }
            drawParagraph(canvas, f15, text, sub_->rect(kInfoText), kText);
            drawParagraph(canvas, f15, treatyName(lex, t) + ".  Regard " + std::to_string(regard(game_, chosen_, me)) + ".",
                          sub_->rect(kInfoMore), kText);
        }
        break;
    case Tab::Propose: break;
    }
}

void DiplomacyScreen::pointerMoved(Point p) {
    pointer_ = p;
    panel_.pointerMoved(p);
    if (sub_) {
        sub_->pointerMoved(p);
    }
}

void DiplomacyScreen::pointerPressed(Point p) {
    pointer_ = p;
    panel_.pointerPressed(p);
    if (sub_) {
        sub_->pointerPressed(p);
    }
}

void DiplomacyScreen::pointerReleased(Point p) {
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
    for (int i = 0; i < static_cast<int>(races_.size()); ++i) {
        if (const auto r = raceLine(i); r && r->contains(p.x, p.y)) {
            choose(races_[static_cast<std::size_t>(i)]);
            return;
        }
    }
}

std::optional<Point> DiplomacyScreen::command(Command c) {
    if (c == Command::Back) {
        if (dialog_ != Dialog::None) {
            dialog_ = Dialog::None;
            openSub();
        } else {
            outcome_ = Outcome::Back;
        }
        return std::nullopt;
    }
    if ((c == Command::Next || c == Command::Previous) && !races_.empty()) {
        const auto at = std::find(races_.begin(), races_.end(), chosen_) - races_.begin();
        const auto n = static_cast<std::ptrdiff_t>(races_.size());
        const auto next = (at + (c == Command::Next ? 1 : n - 1)) % n;
        choose(races_[static_cast<std::size_t>(next)]);
        // Keep the chosen line in view.
        first_ = std::clamp(first_, std::max(0, static_cast<int>(next) - 6), static_cast<int>(next));
        refresh();
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
