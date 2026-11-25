// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/net/session.hpp"

#include "nagus/game/save.hpp"

#include <algorithm>

namespace nagus {

namespace {

// Both sides must run the same rules.
constexpr std::string_view kVersion = "nagus-multiplayer-1";
constexpr std::size_t kMaxName = 24;
constexpr std::uint32_t kMaxPlayers = kEmpires;

std::string readText(Reader& r) {
    const ByteView b = r.bytes(r.u16());
    return std::string(b.begin(), b.end());
}

int number(Reader& r, int lo, int hi) {
    const auto v = static_cast<std::int32_t>(r.u32());
    if (v < lo || v > hi) {
        throw FormatError("damaged message");
    }
    return v;
}

// A game, or a game and a report: their length first.
void writeBlock(Writer& w, ByteView b) {
    w.u32(static_cast<std::uint32_t>(b.size()));
    w.bytes(b);
}

ByteView readBlock(Reader& r) { return r.bytes(r.u32()); }

Bytes lobbyMessage(const Lobby& l) {
    Writer w;
    w.text(l.game);
    const GameSettings& s = l.settings;
    for (const int v : {s.galaxyShape, s.galaxySize, s.minorRaces, s.difficulty, s.victory}) {
        w.i32(v);
    }
    for (const int c : s.civilization) {
        w.i32(c);
    }
    w.u8(s.randomEvents ? 1 : 0);
    w.u32(static_cast<std::uint32_t>(l.players.size()));
    for (const auto& p : l.players) {
        w.text(p.name);
        w.i32(p.empire);
    }
    w.u8(l.started ? 1 : 0);
    return w.take();
}

Lobby readLobby(ByteView data) {
    Reader r(data);
    Lobby l;
    l.game = cleanName(readText(r));
    GameSettings& s = l.settings;
    for (int* v : {&s.galaxyShape, &s.galaxySize, &s.minorRaces, &s.difficulty, &s.victory}) {
        *v = number(r, 0, 100);
    }
    for (int& c : s.civilization) {
        c = number(r, 0, 100);
    }
    s.randomEvents = r.u8() != 0;
    s.automaticCombat = true;
    s.clamp();
    const std::uint32_t n = r.u32();
    if (n > kMaxPlayers) {
        throw FormatError("damaged message");
    }
    for (std::uint32_t i = 0; i < n; ++i) {
        LobbyPlayer p;
        p.name = cleanName(readText(r));
        p.empire = number(r, -1, kEmpires - 1);
        l.players.push_back(p);
    }
    l.started = r.u8() != 0;
    return l;
}

Bytes gameMessage(const Game& g) {
    Writer w;
    w.i32(g.turn);
    writeBlock(w, saveGame(g, {}));
    return w.take();
}

} // namespace

std::string cleanName(std::string_view name) {
    std::string out;
    for (const char c : name) {
        if (c >= 32 && c < 127 && out.size() < kMaxName) {
            out += c;
        }
    }
    return out;
}

std::string cleanChat(std::string_view text) {
    std::string out;
    for (const char c : text) {
        if (c >= 32 && c < 127 && out.size() < kMaxChat) {
            out += c;
        }
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

// --- The host ---------------------------------------------------------

std::unique_ptr<HostSession> HostSession::open(const std::string& game, const std::string& password, const std::string& player,
                                               const EconomyData& economy, std::uint16_t port) {
    auto listener = Listener::open(port);
    if (!listener) {
        return nullptr;
    }
    auto h = std::unique_ptr<HostSession>(new HostSession(std::move(listener), economy));
    h->password_ = password;
    h->lobby_.game = cleanName(game);
    h->lobby_.players.push_back({cleanName(player), -1});
    return h;
}

void HostSession::poll() {
    while (auto c = listener_->accept()) {
        Player p;
        p.link = std::move(c);
        players_.push_back(std::move(p));
    }
    for (auto& p : players_) {
        if (!p.link) {
            continue;
        }
        p.link->poll();
        while (p.link) {
            const auto m = p.link->receive();
            if (!m) {
                break;
            }
            try {
                handle(p, *m);
            } catch (const FormatError&) {
                drop(p, "Damaged message");
            }
        }
        if (p.link && p.link->state() == Connection::State::Closed) {
            drop(p, p.link->error());
        }
    }
    std::erase_if(players_, [](const Player& p) { return !p.link; });
}

void HostSession::drop(Player& p, const std::string&) {
    if (p.welcomed) {
        notices_.push_back(p.name + " has left");
        // Their empire is the computer's from now on.
        if (start_ && p.empire >= 0) {
            start_->humans[static_cast<std::size_t>(p.empire)] = false;
        }
    }
    if (p.link) {
        p.link->close();
    }
    p.link.reset();
    const bool welcomed = p.welcomed;
    p.welcomed = false;
    if (welcomed) {
        rebuildLobby();
        if (start_) {
            sendWaiting();
        }
    }
}

void HostSession::handle(Player& p, const Message& m) {
    Reader r(m.data);
    if (!p.welcomed) {
        if (m.kind != msg::kHello) {
            throw FormatError("no hello");
        }
        const std::string version = readText(r);
        const std::string name = cleanName(readText(r));
        const std::string password = readText(r);
        Writer w;
        std::size_t refused = 0;
        if (version != kVersion) {
            refused = 1213; // "A synchronization error has occurred..."
        } else if (!password_.empty() && password != password_) {
            refused = kTextBadPassword;
        } else if (lobby_.players.size() >= kMaxPlayers || lobby_.started) {
            refused = kTextAllTaken;
        }
        if (refused != 0 || name.empty()) {
            w.u32(static_cast<std::uint32_t>(refused != 0 ? refused : kTextAllTaken));
            p.link->send(msg::kRefused, w.data());
            p.link->poll();
            p.link->close();
            p.link.reset();
            return;
        }
        // Every name different.
        std::string unique = name;
        for (int n = 2; std::any_of(lobby_.players.begin(), lobby_.players.end(), [&](const LobbyPlayer& o) { return o.name == unique; }); ++n) {
            unique = cleanName(name.substr(0, 20) + " " + std::to_string(n));
        }
        p.name = unique;
        p.welcomed = true;
        Writer welcome;
        welcome.text(unique);
        p.link->send(msg::kWelcome, welcome.data());
        rebuildLobby();
        return;
    }
    switch (m.kind) {
    case msg::kChoose: {
        const int e = number(r, -1, kEmpires - 1);
        if (lobby_.started) {
            break;
        }
        const bool taken = std::any_of(lobby_.players.begin(), lobby_.players.end(), [&](const LobbyPlayer& o) {
            return e >= 0 && o.empire == e && o.name != p.name;
        });
        if (!taken) {
            p.empire = e;
        }
        rebuildLobby();
        break;
    }
    case msg::kOrders: {
        const int turn = number(r, 0, 1 << 30);
        const ByteView game = readBlock(r);
        if (start_ && turn == start_->turn && p.empire >= 0) {
            Game played = loadGame(game, economy_);
            p.orders = std::move(played);
            sendWaiting();
        }
        break;
    }
    case msg::kChat: broadcast({p.name, p.empire, cleanChat(readText(r))}); break;
    default: break;
    }
}

void HostSession::rebuildLobby() {
    lobby_.players.resize(1);
    for (const auto& o : players_) {
        if (o.link && o.welcomed) {
            lobby_.players.push_back({o.name, o.empire});
        }
    }
    sendLobby();
}

void HostSession::sendLobby() {
    const Bytes b = lobbyMessage(lobby_);
    for (auto& p : players_) {
        if (p.link && p.welcomed) {
            p.link->send(msg::kLobby, b);
        }
    }
}

void HostSession::sendWaiting() {
    Writer w;
    const auto waiting = waitingFor();
    w.u32(static_cast<std::uint32_t>(waiting.size()));
    for (const int e : waiting) {
        w.i32(e);
    }
    for (auto& p : players_) {
        if (p.link && p.welcomed) {
            p.link->send(msg::kWaiting, w.data());
        }
    }
}

void HostSession::setSettings(const GameSettings& settings) {
    lobby_.settings = settings;
    sendLobby();
}

bool HostSession::chooseEmpire(int empire) {
    for (std::size_t i = 1; i < lobby_.players.size(); ++i) {
        if (empire >= 0 && lobby_.players[i].empire == empire) {
            return false;
        }
    }
    lobby_.players.front().empire = empire;
    sendLobby();
    return true;
}

bool HostSession::canStart() const {
    return lobby_.players.size() >= 2 &&
           std::all_of(lobby_.players.begin(), lobby_.players.end(), [](const LobbyPlayer& p) { return p.empire >= 0; });
}

void HostSession::start(const Game& game) {
    lobby_.started = true;
    sendLobby();
    start_ = game;
    for (auto& p : players_) {
        if (p.link && p.welcomed && p.empire >= 0) {
            Game theirs = game;
            theirs.player = p.empire;
            Writer w;
            w.i32(p.empire);
            writeBlock(w, saveGame(theirs, {}));
            p.link->send(msg::kStart, w.data());
        }
    }
}

void HostSession::submit(const Game& played) {
    mine_ = played;
    sendWaiting();
}

std::vector<int> HostSession::waitingFor() const {
    std::vector<int> out;
    if (!start_) {
        return out;
    }
    if (!mine_) {
        out.push_back(start_->player);
    }
    for (const auto& p : players_) {
        if (p.link && p.welcomed && p.empire >= 0 && !p.orders && start_->humans[static_cast<std::size_t>(p.empire)]) {
            out.push_back(p.empire);
        }
    }
    return out;
}

std::optional<PlayerTurn> HostSession::finishTurn() {
    if (!start_ || !waitingFor().empty()) {
        return std::nullopt;
    }
    Game merged = *start_;
    mergeOrders(merged, *start_, *mine_, start_->player);
    for (const auto& p : players_) {
        if (p.orders && p.empire >= 0 && start_->humans[static_cast<std::size_t>(p.empire)]) {
            mergeOrders(merged, *start_, *p.orders, p.empire);
        }
    }
    auto turns = endMultiplayerTurn(merged);
    for (auto& p : players_) {
        p.orders.reset();
        if (!p.link || !p.welcomed) {
            continue;
        }
        for (const auto& t : turns) {
            if (t.empire == p.empire) {
                Writer w;
                writeBlock(w, saveGame(t.game, {}));
                writeBlock(w, saveSummary(t.summary));
                p.link->send(msg::kTurn, w.data());
            }
        }
    }
    mine_.reset();
    start_ = turns.front().game;
    return std::move(turns.front());
}

void HostSession::broadcast(const ChatLine& line) {
    if (line.text.empty()) {
        return;
    }
    chat_.push_back(line);
    Writer w;
    w.text(line.name);
    w.i32(line.empire);
    w.text(line.text);
    for (auto& p : players_) {
        if (p.link && p.welcomed) {
            p.link->send(msg::kChat, w.data());
        }
    }
}

void HostSession::say(const std::string& text) {
    const LobbyPlayer& me = lobby_.players.front();
    broadcast({me.name, me.empire, cleanChat(text)});
}

std::vector<ChatLine> HostSession::takeChat() {
    std::vector<ChatLine> out;
    out.swap(chat_);
    return out;
}

std::vector<std::string> HostSession::takeNotices() {
    std::vector<std::string> out;
    out.swap(notices_);
    return out;
}

// --- A player ---------------------------------------------------------

std::unique_ptr<ClientSession> ClientSession::connect(const std::string& address, const std::string& player, const std::string& password,
                                                      const EconomyData& economy) {
    auto c = std::unique_ptr<ClientSession>(new ClientSession(Connection::connect(address), economy));
    Writer w;
    w.text(kVersion);
    w.text(cleanName(player));
    w.text(password);
    c->link_->send(msg::kHello, w.data());
    return c;
}

void ClientSession::poll() {
    if (state_ == State::Refused || state_ == State::Lost) {
        return;
    }
    link_->poll();
    while (const auto m = link_->receive()) {
        try {
            handle(*m);
        } catch (const FormatError&) {
            error_ = "Damaged message";
            link_->close();
        }
        if (state_ == State::Refused) {
            return;
        }
    }
    if (link_->state() == Connection::State::Closed) {
        error_ = error_.empty() ? link_->error() : error_;
        state_ = State::Lost;
    }
}

void ClientSession::handle(const Message& m) {
    Reader r(m.data);
    switch (m.kind) {
    case msg::kWelcome:
        if (state_ == State::Connecting) {
            name_ = cleanName(readText(r));
            state_ = State::Lobby;
        }
        break;
    case msg::kRefused:
        refused_ = r.u32();
        state_ = State::Refused;
        link_->close();
        break;
    case msg::kLobby: lobby_ = readLobby(m.data); break;
    case msg::kStart: {
        const int empire = number(r, 0, kEmpires - 1);
        Game g = loadGame(readBlock(r), economy_);
        g.player = empire;
        turnNumber_ = g.turn;
        start_ = std::move(g);
        state_ = State::Playing;
        break;
    }
    case msg::kWaiting: {
        waiting_.clear();
        const std::uint32_t n = r.u32();
        if (n > kMaxPlayers) {
            throw FormatError("damaged message");
        }
        for (std::uint32_t i = 0; i < n; ++i) {
            waiting_.push_back(number(r, 0, kEmpires - 1));
        }
        break;
    }
    case msg::kTurn: {
        PlayerTurn t;
        t.game = loadGame(readBlock(r), economy_);
        t.summary = loadSummary(readBlock(r));
        t.empire = t.game.player;
        turnNumber_ = t.game.turn;
        submitted_ = false;
        turn_ = std::move(t);
        break;
    }
    case msg::kChat: {
        ChatLine line;
        line.name = cleanName(readText(r));
        line.empire = number(r, -1, kEmpires - 1);
        line.text = cleanChat(readText(r));
        if (!line.text.empty()) {
            chat_.push_back(std::move(line));
        }
        break;
    }
    default: break;
    }
}

void ClientSession::say(const std::string& text) {
    const std::string line = cleanChat(text);
    if (line.empty() || !link_) {
        return;
    }
    Writer w;
    w.text(line);
    link_->send(msg::kChat, w.data());
}

std::vector<ChatLine> ClientSession::takeChat() {
    std::vector<ChatLine> out;
    out.swap(chat_);
    return out;
}

int ClientSession::place() const {
    for (std::size_t i = 0; i < lobby_.players.size(); ++i) {
        if (lobby_.players[i].name == name_) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void ClientSession::chooseEmpire(int empire) {
    Writer w;
    w.i32(empire);
    link_->send(msg::kChoose, w.data());
}

std::optional<Game> ClientSession::takeStart() {
    auto out = std::move(start_);
    start_.reset();
    return out;
}

void ClientSession::submit(const Game& played) {
    link_->send(msg::kOrders, gameMessage(played));
    submitted_ = true;
}

std::optional<PlayerTurn> ClientSession::takeTurn() {
    auto out = std::move(turn_);
    turn_.reset();
    return out;
}

} // namespace nagus
