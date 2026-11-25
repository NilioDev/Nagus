// SPDX-License-Identifier: GPL-3.0-or-later
//
// The research screen: for each of the six fields its level, what it is
// working towards, the share of research it gets (with a lock that keeps a
// share when others change) and how far it has come.
#pragma once

#include "nagus/game/game.hpp"
#include "nagus/ui/menus.hpp"

namespace nagus {

class ResearchScreen : public MenuScreen {
public:
    // Element numbers in the research screen's layout.
    static constexpr std::uint32_t kSummary = 0xbb9;
    static constexpr std::uint32_t kManagement = 0xbc3;
    static constexpr std::uint32_t kFieldDatabase = 0xbc4;
    static constexpr std::uint32_t kObjectDatabase = 0xbc5;
    static constexpr std::uint32_t kPoints = 0xbba;
    static constexpr std::uint32_t kPointsLabel = 0xbc9;
    static constexpr std::uint32_t kListTitle = 0xbca;
    static constexpr std::uint32_t kList[7] = {0xbbc, 0xbbd, 0xbbe, 0xbbf, 0xbc0, 0xbc1, 0xbc2};
    static constexpr std::uint32_t kArea = 0xbc8;
    // In the management panel: the elements of a field's block are these
    // numbers plus 14 times the field.
    static constexpr std::uint32_t kFieldName = 0xc1d;
    static constexpr std::uint32_t kFieldPicture = 0xc1e;
    static constexpr std::uint32_t kFieldText = 0xc1f;
    static constexpr std::uint32_t kFieldLevelLabel = 0xc20;
    static constexpr std::uint32_t kFieldNext = 0xc21;
    static constexpr std::uint32_t kShareLabel = 0xc22;
    static constexpr std::uint32_t kLock = 0xc23;
    static constexpr std::uint32_t kLess = 0xc24;
    static constexpr std::uint32_t kMore = 0xc25;
    static constexpr std::uint32_t kShareBar = 0xc26;
    static constexpr std::uint32_t kShareText = 0xc27;
    static constexpr std::uint32_t kProgressLabel = 0xc28;
    static constexpr std::uint32_t kProgressBar = 0xc29;
    static constexpr std::uint32_t kProgressText = 0xc2a;
    static constexpr std::uint32_t kStep = 14;
    static constexpr std::uint32_t kFieldLevel[kResearchFields] = {0xc71, 0xc72, 0xc73, 0xc74, 0xc75, 0xcda};
    static constexpr int kShareStep = 5;

    ResearchScreen(Assets& assets, Game& game);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    // Where an element of a field's block is on the screen.
    Rect fieldRect(int field, std::uint32_t element) const;

private:
    EmpireState& empire();
    void refresh();
    std::string fieldName(int field) const;

    Assets& assets_;
    Game& game_;
    Panel panel_;
    Panel fields_;
    Point pointer_;
};

} // namespace nagus
