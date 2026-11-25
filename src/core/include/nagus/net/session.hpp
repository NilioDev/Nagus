// SPDX-License-Identifier: GPL-3.0-or-later
//
// A multiplayer game over the network. The host listens, the players
// connect and choose their empires, the host starts the game; then every
// turn each player sends their orders (their copy of the game) and the host
// sends back the game after the turn with each player's report (see
// game/multiplayer.hpp and docs/rules/multiplayer.md).
#pragma once

#include "nagus/game/multiplayer.hpp"
#include "nagus/net/connection.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nagus {

// The kinds of messages.
namespace msg {
inline constexpr std::uint8_t kHello = 1;   // player → host: version, name, password
inline constexpr std::uint8_t kWelcome = 2; // host → player
inline constexpr std::uint8_t kRefused = 3; // host → player: why (a lexicon text, or our own)
inline constexpr std::uint8_t kLobby = 4;   // host → players: the game's name, settings and players
inline constexpr std::uint8_t kChoose = 5;  // player → host: the empire they want
inline constexpr std::uint8_t kStart = 6;   // host → player: the game
inline constexpr std::uint8_t kOrders = 7;  // player → host: the turn and their copy of the game
inline constexpr std::uint8_t kWaiting = 8; // host → players: whose orders have not come
inline constexpr std::uint8_t kTurn = 9;    // host → player: the game after the turn and the report
inline constexpr std::uint8_t kChat = 10;   // player → host: a line; host → players: who said it and the line
} // namespace msg

// Players are refused for a wrong password or when every empire is taken.
inline constexpr std::size_t kTextBadPassword = 554;
inline constexpr std::size_t kTextAllTaken = 360;

struct LobbyPlayer {
    std::string name;
    int empire = -1; // chosen, or -1
};

struct Lobby {
    std::string game;
    GameSettings settings;
    std::vector<LobbyPlayer> players; // the host first
    bool started = false;
};

// Names: printable characters only, and not too long.
std::string cleanName(std::string_view name);
// A chat line: printable characters only, and not too long.
inline constexpr std::size_t kMaxChat = 160;
std::string cleanChat(std::string_view text);

// A line said in the chat, and who said it.
struct ChatLine {
    std::string name;
    int empire = -1;
    std::string text;
};

class HostSession {
public:
    // Listens for players; null when the port cannot be used.
    static std::unique_ptr<HostSession> open(const std::string& game, const std::string& password, const std::string& player,
                                             const EconomyData& economy, std::uint16_t port = kGamePort);

    std::uint16_t port() const { return listener_->port(); }
    // Takes in players and their messages. Call it often.
    void poll();
    const Lobby& lobby() const { return lobby_; }
    void setSettings(const GameSettings& settings);
    // The host's own empire; false when another player has it.
    bool chooseEmpire(int empire);
    // At least two players, each with an empire.
    bool canStart() const;
    // Starts the game, made with every player's empire human; sends each
    // player their copy.
    void start(const Game& game);

    // The host's own orders for this turn.
    void submit(const Game& played);
    // The empires whose orders have not come yet.
    std::vector<int> waitingFor() const;
    // When every player's orders are in: ends the turn, sends every player
    // theirs and returns the host's.
    std::optional<PlayerTurn> finishTurn();
    // What happened that the host should hear of ("X has left").
    std::vector<std::string> takeNotices();
    // Says a line to every player.
    void say(const std::string& text);
    // The lines said since last asked, the host's own too.
    std::vector<ChatLine> takeChat();

private:
    struct Player {
        std::unique_ptr<Connection> link;
        std::string name;
        int empire = -1;
        bool welcomed = false;
        std::optional<Game> orders;
    };
    HostSession(std::unique_ptr<Listener> listener, const EconomyData& economy) : listener_(std::move(listener)), economy_(economy) {}
    void handle(Player& p, const Message& m);
    void rebuildLobby();
    void sendLobby();
    void sendWaiting();
    void drop(Player& p, const std::string& why);
    void broadcast(const ChatLine& line);

    std::unique_ptr<Listener> listener_;
    EconomyData economy_;
    std::string password_;
    Lobby lobby_;
    std::vector<Player> players_; // the others
    std::optional<Game> start_;   // the game as this turn began (host's copy)
    std::optional<Game> mine_;    // the host's orders
    std::vector<std::string> notices_;
    std::vector<ChatLine> chat_;
};

class ClientSession {
public:
    enum class State { Connecting, Lobby, Playing, Refused, Lost };

    static std::unique_ptr<ClientSession> connect(const std::string& address, const std::string& player, const std::string& password,
                                                  const EconomyData& economy);
    void poll();
    State state() const { return state_; }
    const Lobby& lobby() const { return lobby_; }
    // The name the host gave this player (made different from the others').
    const std::string& name() const { return name_; }
    // This player's place in the lobby, or -1.
    int place() const;
    // Why the host refused, or why the connection was lost.
    std::size_t refusedText() const { return refused_; }
    const std::string& error() const { return error_; }
    void chooseEmpire(int empire);
    // The game, once the host starts it (given once).
    std::optional<Game> takeStart();
    void submit(const Game& played);
    bool submitted() const { return submitted_; }
    // The empires whose orders the host still waits for.
    const std::vector<int>& waitingFor() const { return waiting_; }
    // The turn's end, once the host sends it (given once).
    std::optional<PlayerTurn> takeTurn();
    // Says a line to every player (it comes back through the host).
    void say(const std::string& text);
    std::vector<ChatLine> takeChat();

private:
    ClientSession(std::unique_ptr<Connection> link, const EconomyData& economy) : link_(std::move(link)), economy_(economy) {}
    void handle(const Message& m);

    std::unique_ptr<Connection> link_;
    EconomyData economy_;
    State state_ = State::Connecting;
    Lobby lobby_;
    std::string name_;
    std::size_t refused_ = 0;
    std::string error_;
    std::optional<Game> start_;
    std::optional<PlayerTurn> turn_;
    std::vector<int> waiting_;
    bool submitted_ = false;
    int turnNumber_ = 0;
    std::vector<ChatLine> chat_;
};

} // namespace nagus
