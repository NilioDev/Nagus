// SPDX-License-Identifier: GPL-3.0-or-later
//
// TCP connections for multiplayer games: a listener for the host and a
// connection to each player, never blocking. Everything sent is a message:
// its length, its kind and its bytes. Messages from the other side are
// outside data: their length is checked before anything is kept (see
// SECURITY.md).
#pragma once

#include "nagus/util/bytes.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace nagus {

// The port Nagus listens on when it hosts a game.
inline constexpr std::uint16_t kGamePort = 47621;
// No message is longer than this.
inline constexpr std::uint32_t kMaxMessage = 32u * 1024 * 1024;

struct Message {
    std::uint8_t kind = 0;
    Bytes data;
};

class Connection {
public:
    enum class State { Connecting, Open, Closed };

    // Starts connecting to a host ("192.168.1.20" or a name, with an
    // optional ":port"). The connection is Closed when the address is bad.
    static std::unique_ptr<Connection> connect(const std::string& address);
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Moves the connection on: finishes connecting, sends what waits and
    // reads what came. Call it often.
    State poll();
    State state() const { return state_; }
    void send(std::uint8_t kind, ByteView data);
    // The next message that came in whole, if any.
    std::optional<Message> receive();
    void close();
    // Why the connection closed, for showing.
    const std::string& error() const { return error_; }

private:
    friend class Listener;
    explicit Connection(std::intptr_t socket, State state);
    void fail(const std::string& why);

    std::intptr_t socket_;
    State state_;
    Bytes in_;
    Bytes out_;
    std::string error_;
};

class Listener {
public:
    // Listens on the port on every network; null when it cannot.
    static std::unique_ptr<Listener> open(std::uint16_t port = kGamePort);
    ~Listener();
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    // A player who has connected, if any.
    std::unique_ptr<Connection> accept();
    std::uint16_t port() const { return port_; }

private:
    Listener(std::intptr_t socket, std::uint16_t port) : socket_(socket), port_(port) {}
    std::intptr_t socket_;
    std::uint16_t port_;
};

// This computer's address on the local network, for telling the other
// players; empty when there is none.
std::string localAddress();

} // namespace nagus
