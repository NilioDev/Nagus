// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/net/connection.hpp"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <poll.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace nagus {

namespace {

#ifdef _WIN32
using Native = SOCKET;
using AddrLen = int;
using IoLen = int;
constexpr Native kNone = INVALID_SOCKET;
bool wouldBlock() {
    const int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY;
}
void closeNative(Native s) { closesocket(s); }
int pollOne(pollfd& p) { return WSAPoll(&p, 1, 0); }
void startNetwork() {
    static const bool started = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    (void)started;
}
#else
using Native = int;
using AddrLen = socklen_t;
using IoLen = std::size_t;
constexpr Native kNone = -1;
bool wouldBlock() { return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS; }
void closeNative(Native s) { ::close(s); }
int pollOne(pollfd& p) { return ::poll(&p, 1, 0); }
void startNetwork() {}
#endif

#if defined(MSG_NOSIGNAL)
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

Native native(std::intptr_t s) { return static_cast<Native>(s); }
std::intptr_t stored(Native s) { return static_cast<std::intptr_t>(s); }

void setNonBlocking(Native s) {
#ifdef _WIN32
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    int yes = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&yes), sizeof yes);
}

// "host", "host:port", "[v6]:port".
void splitAddress(const std::string& address, std::string& host, std::string& port) {
    host = address;
    port = std::to_string(kGamePort);
    if (!address.empty() && address.front() == '[') {
        const auto end = address.find(']');
        if (end != std::string::npos) {
            host = address.substr(1, end - 1);
            if (end + 1 < address.size() && address[end + 1] == ':') {
                port = address.substr(end + 2);
            }
        }
        return;
    }
    const auto colon = address.find(':');
    if (colon != std::string::npos && address.find(':', colon + 1) == std::string::npos) {
        host = address.substr(0, colon);
        port = address.substr(colon + 1);
    }
}

} // namespace

Connection::Connection(std::intptr_t socket, State state) : socket_(socket), state_(state) {}

Connection::~Connection() { close(); }

std::unique_ptr<Connection> Connection::connect(const std::string& address) {
    startNetwork();
    std::string host;
    std::string port;
    splitAddress(address, host, port);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    auto c = std::unique_ptr<Connection>(new Connection(stored(kNone), State::Closed));
    if (host.empty() || getaddrinfo(host.c_str(), port.c_str(), &hints, &found) != 0 || found == nullptr) {
        c->error_ = "Unknown address";
        return c;
    }
    for (addrinfo* a = found; a != nullptr; a = a->ai_next) {
        const Native s = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == kNone) {
            continue;
        }
        setNonBlocking(s);
        if (::connect(s, a->ai_addr, static_cast<AddrLen>(a->ai_addrlen)) == 0 || wouldBlock()) {
            c->socket_ = stored(s);
            c->state_ = State::Connecting;
            break;
        }
        closeNative(s);
    }
    freeaddrinfo(found);
    if (c->state_ == State::Closed) {
        c->error_ = "Cannot connect";
    }
    return c;
}

void Connection::fail(const std::string& why) {
    if (error_.empty()) {
        error_ = why;
    }
    close();
}

void Connection::close() {
    if (native(socket_) != kNone) {
        closeNative(native(socket_));
        socket_ = stored(kNone);
    }
    state_ = State::Closed;
}

Connection::State Connection::poll() {
    if (state_ == State::Closed) {
        return state_;
    }
    const Native s = native(socket_);
    if (state_ == State::Connecting) {
        pollfd p{};
        p.fd = s;
        p.events = POLLOUT;
        if (pollOne(p) <= 0) {
            return state_;
        }
        int err = 0;
        socklen_t len = sizeof err;
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
        if (err != 0 || (p.revents & (POLLERR | POLLHUP)) != 0) {
            fail("Cannot connect");
            return state_;
        }
        state_ = State::Open;
    }
    // Send what waits.
    while (!out_.empty()) {
        const auto n = ::send(s, reinterpret_cast<const char*>(out_.data()), static_cast<IoLen>(std::min<std::size_t>(out_.size(), 1 << 20)), kSendFlags);
        if (n > 0) {
            out_.erase(out_.begin(), out_.begin() + n);
        } else if (n < 0 && wouldBlock()) {
            break;
        } else {
            fail("Connection lost");
            return state_;
        }
    }
    // Read what came.
    std::uint8_t buffer[16384];
    for (;;) {
        const auto n = ::recv(s, reinterpret_cast<char*>(buffer), static_cast<IoLen>(sizeof buffer), 0);
        if (n > 0) {
            in_.insert(in_.end(), buffer, buffer + n);
            // A message may not be longer than allowed; more than that
            // waiting unread is a broken or hostile other side.
            if (in_.size() > kMaxMessage + 64) {
                fail("Message too long");
                return state_;
            }
        } else if (n < 0 && wouldBlock()) {
            break;
        } else {
            fail(n == 0 ? "Disconnected" : "Connection lost");
            return state_;
        }
    }
    return state_;
}

void Connection::send(std::uint8_t kind, ByteView data) {
    if (state_ == State::Closed || data.size() + 1 > kMaxMessage) {
        return;
    }
    Writer w;
    w.u32(static_cast<std::uint32_t>(data.size() + 1));
    w.u8(kind);
    w.bytes(data);
    const Bytes& b = w.data();
    out_.insert(out_.end(), b.begin(), b.end());
}

std::optional<Message> Connection::receive() {
    if (in_.size() < 4) {
        return std::nullopt;
    }
    Reader r(in_);
    const std::uint32_t length = r.u32();
    if (length == 0 || length > kMaxMessage) {
        fail("Damaged message");
        return std::nullopt;
    }
    if (in_.size() - 4 < length) {
        return std::nullopt;
    }
    Message m;
    m.kind = in_[4];
    m.data.assign(in_.begin() + 5, in_.begin() + 4 + length);
    in_.erase(in_.begin(), in_.begin() + 4 + length);
    return m;
}

std::unique_ptr<Listener> Listener::open(std::uint16_t port) {
    startNetwork();
    // IPv6 with IPv4 too where there is IPv6; IPv4 alone otherwise.
    for (const int family : {AF_INET6, AF_INET}) {
        const Native s = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
        if (s == kNone) {
            continue;
        }
        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof yes);
        sockaddr_storage addr{};
        socklen_t len = 0;
        if (family == AF_INET6) {
            int no = 0;
            setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&no), sizeof no);
            auto* a = reinterpret_cast<sockaddr_in6*>(&addr);
            a->sin6_family = AF_INET6;
            a->sin6_addr = in6addr_any;
            a->sin6_port = htons(port);
            len = sizeof(sockaddr_in6);
        } else {
            auto* a = reinterpret_cast<sockaddr_in*>(&addr);
            a->sin_family = AF_INET;
            a->sin_addr.s_addr = htonl(INADDR_ANY);
            a->sin_port = htons(port);
            len = sizeof(sockaddr_in);
        }
        if (bind(s, reinterpret_cast<sockaddr*>(&addr), len) != 0 || listen(s, 8) != 0) {
            closeNative(s);
            continue;
        }
        setNonBlocking(s);
        // The port given out, when 0 asked for any.
        sockaddr_storage bound{};
        socklen_t blen = sizeof bound;
        std::uint16_t actual = port;
        if (getsockname(s, reinterpret_cast<sockaddr*>(&bound), &blen) == 0) {
            actual = ntohs(family == AF_INET6 ? reinterpret_cast<sockaddr_in6*>(&bound)->sin6_port
                                              : reinterpret_cast<sockaddr_in*>(&bound)->sin_port);
        }
        return std::unique_ptr<Listener>(new Listener(stored(s), actual));
    }
    return nullptr;
}

Listener::~Listener() { closeNative(native(socket_)); }

std::unique_ptr<Connection> Listener::accept() {
    const Native s = ::accept(native(socket_), nullptr, nullptr);
    if (s == kNone) {
        return nullptr;
    }
    setNonBlocking(s);
    return std::unique_ptr<Connection>(new Connection(stored(s), Connection::State::Open));
}

std::string localAddress() {
    startNetwork();
    // A datagram socket "connected" to a public address sends nothing, but
    // tells which local address would be used.
    const Native s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s == kNone) {
        return {};
    }
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(53);
    inet_pton(AF_INET, "192.0.2.1", &to.sin_addr);
    std::string out;
    if (::connect(s, reinterpret_cast<sockaddr*>(&to), sizeof to) == 0) {
        sockaddr_in me{};
        socklen_t len = sizeof me;
        char text[INET_ADDRSTRLEN] = {};
        if (getsockname(s, reinterpret_cast<sockaddr*>(&me), &len) == 0 && inet_ntop(AF_INET, &me.sin_addr, text, sizeof text) != nullptr) {
            out = text;
        }
    }
    closeNative(s);
    return out == "0.0.0.0" ? std::string() : out;
}

} // namespace nagus
