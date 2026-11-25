// SPDX-License-Identifier: GPL-3.0-or-later
//
// The multiplayer set-up (mpsetup.wdf, with mpconn.wdf and mpcnet.wdf in
// its slots): names and password, hosting a game or joining one over the
// network (TCP/IP), and the lobby where the players choose their empires
// until the host starts the game. Modem, serial and IPX games are not
// made.
#pragma once

#include "nagus/net/session.hpp"
#include "nagus/ui/menus.hpp"

#include <array>
#include <string>
#include <vector>

namespace nagus {

class MultiplayerScreen : public MenuScreen {
public:
    // mpsetup.wdf
    static constexpr std::uint32_t kHost = 0x6c;
    static constexpr std::uint32_t kJoin = 0x6d;
    static constexpr std::uint32_t kCancel = 0x72;
    static constexpr std::uint32_t kGameName = 0x70;
    static constexpr std::uint32_t kPlayerName = 0x71;
    static constexpr std::uint32_t kPassword = 0x2bc;
    static constexpr std::uint32_t kConnectionSlot = 0x18f;
    static constexpr std::uint32_t kNetworkSlot = 0xc8;
    // mpconn.wdf
    static constexpr std::uint32_t kNetwork = 0xc1;
    static constexpr std::uint32_t kModem = 0xc2;
    static constexpr std::uint32_t kSerial = 0xc4;
    static constexpr std::uint32_t kEmail = 0xc5;
    // mpcnet.wdf
    static constexpr std::uint32_t kTcp = 0xca;
    static constexpr std::uint32_t kIpx = 0xcb;
    static constexpr std::uint32_t kAddressButton = 0xcf;
    static constexpr std::uint32_t kConnect = 0xd2;
    static constexpr std::uint32_t kList = 0xd3;
    static constexpr std::uint32_t kAddress = 0x2ca;
    // Our own buttons in the lobby, where Host and Join are.
    static constexpr std::uint32_t kChooseEmpire = 0x10001;
    static constexpr std::uint32_t kStart = 0x10002;

    enum class Mode { Setup, Lobby };

    MultiplayerScreen(Assets& assets, const GameSettings& settings, std::array<std::string, kEmpires> empireNames);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;
    void typed(std::string_view text) override;
    bool wantsText() const override { return focus_ != 0; }

    Mode mode() const { return mode_; }
    const std::string& gameName() const { return texts_[0]; }
    const std::string& playerName() const { return texts_[1]; }
    const std::string& password() const { return texts_[2]; }
    const std::string& address() const { return texts_[3]; }
    std::uint32_t focus() const { return focus_; }
    void setStatus(std::string status) { status_ = std::move(status); }
    const std::string& status() const { return status_; }
    // The lobby, as the session has it: `me` is this player's place in it.
    void showLobby(const Lobby& lobby, int me, bool hosting, const std::string& address);
    std::optional<Rect> button(std::uint32_t id) const;
    // A text field's place.
    Rect field(std::uint32_t id) const;

private:
    void click(std::uint32_t id);
    void refresh();
    std::vector<std::uint32_t> items() const;
    std::string* text(std::uint32_t id);

    Assets& assets_;
    Panel panel_;
    Panel conn_;
    Panel net_;
    std::array<std::string, kEmpires> empireNames_;
    Mode mode_ = Mode::Setup;
    std::array<std::string, 4> texts_; // game, player, password, address
    std::uint32_t focus_ = 0;
    std::string status_;
    Lobby lobby_;
    int me_ = 0;
    bool hosting_ = false;
    std::string hostAddress_;
    Point pointer_{};
    std::uint32_t held_ = 0;
};

} // namespace nagus
