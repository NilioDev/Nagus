// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/multiplayer_screen.hpp"

#include <algorithm>

namespace nagus {

namespace {

constexpr Pixel kText = 0xffffcc66u;
constexpr Pixel kDim = 0xff8890a0u;
constexpr Pixel kChosen = 0xffffffffu;
constexpr Pixel kButton = 0xff80c8f0u;
constexpr Pixel kButtonDown = 0xffffcc66u;
constexpr Pixel kEmpireColours[kEmpires] = {0xffe0c040u, 0xff60a0ffu, 0xffffa040u, 0xffff5050u, 0xff40e070u};
constexpr std::size_t kMaxText = 40;
// Lexicon numbers.
constexpr std::size_t kNeedGameName = 806;
constexpr std::size_t kNeedPlayerName = 807;
constexpr std::size_t kNeedPlayer = 441; // "Another player must join before a game can begin"

} // namespace

MultiplayerScreen::MultiplayerScreen(Assets& assets, const GameSettings& settings, std::array<std::string, kEmpires> empireNames)
    : assets_(assets), panel_(assets, "mpsetup.wdf"), conn_(assets, "mpconn.wdf"), net_(assets, "mpcnet.wdf"),
      empireNames_(std::move(empireNames)) {
    const Rect c = panel_.rect(kConnectionSlot);
    conn_.setOrigin({c.x, c.y});
    const Rect n = panel_.rect(kNetworkSlot);
    net_.setOrigin({n.x, n.y});
    texts_[0] = cleanName(settings.gameName.empty() ? std::string("Nagus") : settings.gameName);
    texts_[1] = cleanName(settings.playerName);
    texts_[3] = cleanName(settings.hostAddress);
    // Only games over a network, with TCP/IP.
    conn_.setOn(kNetwork, true);
    for (const auto id : {kModem, kSerial}) {
        conn_.setEnabled(id, false);
    }
    conn_.setVisible(kEmail, false);
    net_.setOn(kTcp, true);
    net_.setEnabled(kIpx, false);
    refresh();
}

void MultiplayerScreen::refresh() {
    const bool setup = mode_ == Mode::Setup;
    panel_.setVisible(kHost, setup);
    panel_.setVisible(kJoin, setup);
    for (const auto id : {kTcp, kIpx, kAddressButton, kConnect}) {
        net_.setVisible(id, setup);
    }
    if (!setup) {
        focus_ = 0;
    }
}

std::string* MultiplayerScreen::text(std::uint32_t id) {
    switch (id) {
    case kGameName: return &texts_[0];
    case kPlayerName: return &texts_[1];
    case kPassword: return &texts_[2];
    case kAddress: return &texts_[3];
    default: return nullptr;
    }
}

Rect MultiplayerScreen::field(std::uint32_t id) const { return id == kAddress ? net_.rect(id) : panel_.rect(id); }

std::optional<Rect> MultiplayerScreen::button(std::uint32_t id) const {
    switch (id) {
    case kChooseEmpire:
        return mode_ == Mode::Lobby ? std::optional<Rect>(panel_.rect(kHost)) : std::nullopt;
    case kStart:
        return mode_ == Mode::Lobby && hosting_ ? std::optional<Rect>(panel_.rect(kJoin)) : std::nullopt;
    case kHost:
    case kJoin:
    case kCancel: return panel_.usable(id) ? std::optional<Rect>(panel_.rect(id)) : std::nullopt;
    default: return net_.usable(id) ? std::optional<Rect>(net_.rect(id)) : std::nullopt;
    }
}

void MultiplayerScreen::showLobby(const Lobby& lobby, int me, bool hosting, const std::string& address) {
    // A change in the lobby makes an old message stale.
    bool changed = lobby.players.size() != lobby_.players.size();
    for (std::size_t i = 0; !changed && i < lobby.players.size(); ++i) {
        changed = lobby.players[i].empire != lobby_.players[i].empire;
    }
    if (changed && mode_ == Mode::Lobby) {
        status_.clear();
    }
    lobby_ = lobby;
    me_ = me;
    hosting_ = hosting;
    hostAddress_ = address;
    if (mode_ != Mode::Lobby) {
        mode_ = Mode::Lobby;
        status_.clear();
        refresh();
    }
}

std::vector<std::uint32_t> MultiplayerScreen::items() const {
    if (mode_ == Mode::Lobby) {
        if (hosting_) {
            return {kChooseEmpire, kStart, kCancel};
        }
        return {kChooseEmpire, kCancel};
    }
    return {kPlayerName, kGameName, kPassword, kHost, kAddress, kConnect, kCancel};
}

void MultiplayerScreen::draw(Canvas& canvas, std::uint64_t ms) {
    panel_.draw(canvas);
    conn_.draw(canvas);
    net_.draw(canvas);
    const Font& font = assets_.font("human15.fnt");
    const Font& small = assets_.font("human9.fnt");
    // The labels of the text fields, and what is in them.
    const std::pair<std::uint32_t, const char*> labels[] = {{0x2be, "M_gmnm0.tga"}, {0x2bf, "M_plynm0.tga"}, {0x2bd, "M_pswrd0.tga"}};
    for (const auto& [slot, picture] : labels) {
        const Rect r = panel_.rect(slot);
        canvas.draw(assets_.image(picture), r.x, r.y);
    }
    if (mode_ == Mode::Setup) {
        const Rect a = net_.rect(kAddressButton);
        canvas.draw(assets_.image(focus_ == kAddress ? "M_hadd1.tga" : "M_hadd0.tga"), a.x, a.y);
    }
    for (const auto id : {kGameName, kPlayerName, kPassword, kAddress}) {
        if (id == kAddress && mode_ != Mode::Setup) {
            continue;
        }
        const Rect r = field(id);
        std::string shown = *const_cast<MultiplayerScreen*>(this)->text(id);
        if (id == kPassword) {
            shown = std::string(shown.size(), '*');
        }
        if (focus_ == id && (ms / 500) % 2 == 0) {
            shown += "_";
        }
        canvas.text(font, shown, {r.x + 2, r.y, r.w - 4, r.h}, mode_ == Mode::Lobby ? kDim : (focus_ == id ? kChosen : kText), Align::Left);
    }
    const Rect list = net_.rect(kList);
    if (mode_ == Mode::Setup) {
        drawParagraph(canvas, font, status_, {list.x + 4, list.y + 4, list.w - 8, list.h - 8}, kText);
        return;
    }
    // The lobby: our own buttons where Host and Join are.
    for (const auto id : items()) {
        if (id == kCancel) {
            continue;
        }
        const Rect r = *button(id);
        canvas.fill({r.x, r.y + 2, r.w - 30, r.h - 4}, held_ == id ? kButtonDown : kButton);
        canvas.fill({r.x + r.w - 24, r.y + 2, 24, r.h - 4}, held_ == id ? kButtonDown : kButton);
        const bool usable = id != kStart || std::all_of(lobby_.players.begin(), lobby_.players.end(), [](const LobbyPlayer& p) { return p.empire >= 0; });
        canvas.text(font, id == kStart ? "START" : "EMPIRE", {r.x + 4, r.y + 2, r.w - 34, r.h - 4}, usable ? 0xff000000u : 0xff505860u, Align::Left);
    }
    const Rect area{net_.origin().x + 58, net_.origin().y + 66, 236, 210};
    int y = area.y;
    canvas.text(font, lobby_.game, {area.x, y, area.w, 18}, kChosen, Align::Left);
    y += 22;
    if (hosting_) {
        canvas.text(small, "Others join at", area.x, y, kText);
        y += 12;
        canvas.text(font, hostAddress_.empty() ? std::string("this computer") : hostAddress_, area.x, y, kChosen);
        y += 20;
    }
    y += 4;
    for (std::size_t i = 0; i < lobby_.players.size(); ++i) {
        const auto& p = lobby_.players[i];
        const bool chosen = p.empire >= 0 && p.empire < kEmpires;
        canvas.text(font, p.name, {area.x, y, area.w / 2, 18}, static_cast<int>(i) == me_ ? kChosen : kText, Align::Left);
        canvas.text(small, chosen ? empireNames_[static_cast<std::size_t>(p.empire)] : std::string("choosing an empire"),
                    {area.x + area.w / 2, y, area.w / 2, 18}, chosen ? kEmpireColours[p.empire] : kDim, Align::Left);
        y += 20;
    }
    y += 8;
    const bool ready = std::all_of(lobby_.players.begin(), lobby_.players.end(), [](const LobbyPlayer& p) { return p.empire >= 0; });
    const std::string waiting = !status_.empty() ? status_
                                : !hosting_      ? std::string("Waiting for the host to start")
                                : lobby_.players.size() < 2 ? assets_.lexicon()[kNeedPlayer]
                                : ready          ? std::string("Everyone has an empire: start the game")
                                                 : std::string("Start when everyone has an empire");
    drawParagraph(canvas, small, waiting, {area.x, y, area.w, area.y + area.h - y}, kText);
}

void MultiplayerScreen::pointerMoved(Point p) {
    pointer_ = p;
    panel_.pointerMoved(p);
    net_.pointerMoved(p);
}

void MultiplayerScreen::pointerPressed(Point p) {
    pointer_ = p;
    held_ = 0;
    for (const auto id : {kChooseEmpire, kStart}) {
        if (const auto r = button(id); r && r->contains(p.x, p.y)) {
            held_ = id;
        }
    }
    panel_.pointerPressed(p);
    net_.pointerPressed(p);
}

void MultiplayerScreen::pointerReleased(Point p) {
    pointer_ = p;
    const std::uint32_t held = held_;
    held_ = 0;
    if (held != 0) {
        if (const auto r = button(held); r && r->contains(p.x, p.y)) {
            assets_.clicked();
            click(held);
        }
        return;
    }
    if (const auto id = panel_.pointerReleased(p)) {
        click(*id);
        return;
    }
    if (const auto id = net_.pointerReleased(p)) {
        click(*id);
        return;
    }
    // A click in a text field: typing goes there.
    focus_ = 0;
    if (mode_ == Mode::Setup) {
        for (const auto id : {kGameName, kPlayerName, kPassword, kAddress}) {
            if (field(id).contains(p.x, p.y)) {
                focus_ = id;
            }
        }
    }
}

void MultiplayerScreen::click(std::uint32_t id) {
    switch (id) {
    case kHost:
        focus_ = 0;
        if (texts_[1].empty()) {
            status_ = assets_.lexicon()[kNeedPlayerName];
        } else if (texts_[0].empty()) {
            status_ = assets_.lexicon()[kNeedGameName];
        } else {
            outcome_ = Outcome::Host;
        }
        break;
    case kJoin:
    case kAddressButton: focus_ = kAddress; break;
    case kConnect:
        focus_ = 0;
        if (texts_[1].empty()) {
            status_ = assets_.lexicon()[kNeedPlayerName];
        } else if (texts_[3].empty()) {
            focus_ = kAddress;
        } else {
            status_ = assets_.lexicon()[181]; // "Connecting"
            outcome_ = Outcome::Join;
        }
        break;
    case kCancel: outcome_ = Outcome::Back; break;
    case kChooseEmpire: outcome_ = Outcome::ChooseEmpire; break;
    case kStart: outcome_ = Outcome::Start; break;
    default:
        if (text(id) != nullptr) {
            focus_ = id;
        }
        break;
    }
}

void MultiplayerScreen::typed(std::string_view s) {
    std::string* t = text(focus_);
    if (t == nullptr) {
        return;
    }
    for (const char c : s) {
        if (c == '\b') {
            if (!t->empty()) {
                t->pop_back();
            }
        } else if (c == '\n' || c == '\r') {
            focus_ = 0;
        } else if (c >= 32 && c < 127 && t->size() < kMaxText) {
            *t += c;
        }
    }
}

std::optional<Point> MultiplayerScreen::command(Command c) {
    if (c == Command::Back) {
        if (focus_ != 0) {
            focus_ = 0;
        } else {
            outcome_ = Outcome::Back;
        }
        return std::nullopt;
    }
    if (c != Command::Up && c != Command::Down && c != Command::Left && c != Command::Right) {
        return std::nullopt;
    }
    // The gamepad steps through the fields and buttons in turn.
    const auto list = items();
    auto rectOf = [&](std::uint32_t id) {
        if (const auto b = button(id)) {
            return *b;
        }
        return field(id);
    };
    int at = -1;
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (rectOf(list[i]).contains(pointer_.x, pointer_.y)) {
            at = static_cast<int>(i);
        }
    }
    const int n = static_cast<int>(list.size());
    const bool forward = c == Command::Down || c == Command::Right;
    at = at < 0 ? 0 : (at + (forward ? 1 : n - 1)) % n;
    const Rect r = rectOf(list[static_cast<std::size_t>(at)]);
    return Point{r.centerX(), r.centerY()};
}

} // namespace nagus
