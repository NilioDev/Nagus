// SPDX-License-Identifier: GPL-3.0-or-later
//
// The intelligence screen: how much intelligence the empire gathers, its
// plan against each other empire (management), what its agents did and
// what was done to it (report), and how the empires compare (empire
// status).
#pragma once

#include "nagus/game/intel.hpp"
#include "nagus/ui/menus.hpp"

#include <memory>

namespace nagus {

// The report's line for an operation.
std::string intelText(const Lexicon& lexicon, const Game& game, const IntelReport& r);

class IntelScreen : public MenuScreen {
public:
    // <x>intel.wdf
    static constexpr std::uint32_t kSummary = 0xfa1;
    static constexpr std::uint32_t kManagement = 0xfaa;
    static constexpr std::uint32_t kReport = 0xfab;
    static constexpr std::uint32_t kStatus = 0xfac;
    static constexpr std::uint32_t kTitle = 0xfa2;
    static constexpr std::uint32_t kSubtitle = 0xfb0;
    static constexpr std::uint32_t kSmall = 0xfb1;
    static constexpr std::uint32_t kLines[7] = {0xfa4, 0xfa5, 0xfae, 0xfa6, 0xfa7, 0xfa8, 0xfa9};
    static constexpr std::uint32_t kArea = 0xfad;
    // <x>intelm.wdf: the security share at the top, and a quarter for each
    // other empire.
    static constexpr std::uint32_t kPlanTitle = 0x1005;
    static constexpr std::uint32_t kSecurityBar = 0x1006;
    static constexpr std::uint32_t kSecurityLess = 0x1008;
    static constexpr std::uint32_t kSecurityMore = 0x1007;
    static constexpr std::uint32_t kSecurityText = 0x1009;
    static constexpr std::uint32_t kQuarterName = 0x100a;    // + quarter
    static constexpr std::uint32_t kQuarterStatus[4] = {0x1054, 0x1056, 0x1057, 0x1058};
    static constexpr std::uint32_t kFocus[kIntelFocuses] = {0x100e, 0x1012, 0x1016, 0x101a}; // + quarter
    // For espionage (row 0) and sabotage (row 1): label, less, more, bar,
    // percent (+ quarter; the espionage labels are not in order).
    static constexpr std::uint32_t kRowLabels[2][4] = {{0x101e, 0x101f, 0x1022, 0x1023}, {0x1038, 0x1039, 0x103a, 0x103b}};
    static constexpr std::uint32_t kRowLock[2] = {0x1024, 0x103c};
    static constexpr std::uint32_t kRowLess[2] = {0x1030, 0x1048};
    static constexpr std::uint32_t kRowMore[2] = {0x102c, 0x1044};
    static constexpr std::uint32_t kRowBar[2] = {0x1028, 0x1040};
    static constexpr std::uint32_t kRowText[2] = {0x1034, 0x104c};
    // <x>intelr.wdf
    static constexpr std::uint32_t kReportList = 0x106e;
    static constexpr std::uint32_t kReportTitle = 0x1074;
    static constexpr std::uint32_t kReportText = 0x1071;
    static constexpr std::uint32_t kReportUp = 0x106f;
    static constexpr std::uint32_t kReportDown = 0x1070;
    // <x>intele.wdf: six tables of five empires: title, then for each row
    // the rank, name (or bar) and value.
    static constexpr std::uint32_t kTableTitle[6] = {0x10cd, 0x1131, 0x1195, 0x11f9, 0x125d, 0x12c1};
    static constexpr std::uint32_t kTableRank[6] = {0x10ce, 0x1132, 0x1196, 0x11fa, 0x125e, 0x12c2};
    static constexpr std::uint32_t kTableName[6] = {0x10d3, 0x1137, 0x119b, 0x11ff, 0x1263, 0x12c7};
    static constexpr std::uint32_t kTableValue[6] = {0x10d8, 0x113c, 0x11a0, 0x1204, 0x1268, 0x12cc};

    enum class Tab { Management, Report, Status };

    IntelScreen(Assets& assets, Game& game);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    Tab tab() const { return tab_; }
    void showTab(Tab t);
    // The empire shown in each quarter of the management page (-1: none).
    int quarterEmpire(int quarter) const;
    void click(std::uint32_t id);
    std::optional<Rect> button(std::uint32_t id) const;
    // The reports shown, newest first.
    std::vector<IntelReport> reports() const;
    std::optional<Rect> reportLine(int i) const;
    int chosenReport() const { return chosenReport_; }

private:
    void refresh();
    char prefix() const;
    const Font& font(int size) const;
    EmpireState& mine();

    Assets& assets_;
    Game& game_;
    Panel panel_;
    std::unique_ptr<Panel> sub_;
    Tab tab_ = Tab::Management;
    int chosenReport_ = 0;
    int firstReport_ = 0;
    Point pointer_;
};

} // namespace nagus
