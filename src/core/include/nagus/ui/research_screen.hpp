// SPDX-License-Identifier: GPL-3.0-or-later
//
// The research screen: for each of the six fields its level, what it is
// working towards, the share of research it gets (with a lock that keeps a
// share when others change) and how far it has come. Its two databases (the
// original's encyclopedia) describe each technology level, and each
// building and ship class of the player's empire.
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
    // The technology field database (<x>techf.wdf).
    static constexpr std::uint32_t kDbFields[kResearchFields] = {0xc80, 0xc81, 0xc82, 0xc83, 0xc84, 0xc85};
    static constexpr std::uint32_t kDbTitle = 0xc86;
    static constexpr std::uint32_t kDbText = 0xc87;
    static constexpr std::uint32_t kDbTextUp = 0xc88;
    static constexpr std::uint32_t kDbTextDown = 0xc89;
    static constexpr std::uint32_t kDbList = 0xc8a;
    static constexpr std::uint32_t kDbPicture = 0xc8f;
    // The object database (<x>techo.wdf, with <x>techosh.wdf for ships and
    // <x>techost.wdf for buildings in its area).
    static constexpr int kCategories = 6; // research, development, food, energy, defence, ships
    static constexpr std::uint32_t kObjCategories[kCategories] = {0xce4, 0xce5, 0xce6, 0xce7, 0xce8, 0xce9};
    static constexpr std::uint32_t kObjArea = 0xcf8;
    static constexpr std::uint32_t kObjTitle = 0xcea;
    static constexpr std::uint32_t kObjDescription = 0xceb;
    static constexpr std::uint32_t kObjStatistics = 0xcec;
    static constexpr std::uint32_t kObjText = 0xced;
    static constexpr std::uint32_t kObjTextUp = 0xcee;
    static constexpr std::uint32_t kObjTextDown = 0xcef;
    static constexpr std::uint32_t kObjList = 0xcf0;
    static constexpr std::uint32_t kObjPicture = 0xcf8;
    // In the ship panel: scroll the list, turn the ship.
    static constexpr std::uint32_t kObjListUp = 0xcf1;
    static constexpr std::uint32_t kObjListDown = 0xcf2;
    static constexpr std::uint32_t kTurnUp = 0xcf3;
    static constexpr std::uint32_t kTurnDown = 0xcf4;
    static constexpr std::uint32_t kTurnLeft = 0xcf5;
    static constexpr std::uint32_t kTurnRight = 0xcf6;
    static constexpr int kListLine = 15;
    static constexpr int kListHeading = 24;

    enum class Page { Management, Fields, Objects };
    // How the ship in the object database turns: not, about the vertical,
    // or end over end.
    enum class Spin { None, Left, Right, Up, Down };

    ResearchScreen(Assets& assets, Game& game);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override;
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    // Where an element of a field's block is on the screen.
    Rect fieldRect(int field, std::uint32_t element) const;

    Page page() const { return page_; }
    void showPage(Page p);
    // The databases: the chosen field or category, the entries of its list
    // (technologies, buildings or ship classes) and the chosen one.
    int chosenField() const { return dbField_; }
    int chosenCategory() const { return category_; }
    int entries() const;
    int chosenEntry() const { return entry_; }
    std::optional<Rect> entryRect(int i) const;
    // The chosen entry's name and text (its description, or with
    // statistics on, its numbers).
    std::string entryTitle() const;
    std::string entryText() const;
    bool statistics() const { return statistics_; }
    Spin spin() const { return spin_; }
    // Whether the chosen ship class shows as a turning 3D model.
    bool shipModelShown() const { return modelShown_; }
    int listFirst() const { return listFirst_; }
    // Where an element of the object database's ship or building panel is.
    Rect objectRect(std::uint32_t id) const { return objectPanel().rect(id); }
    // The buildings and ship classes in an object category.
    std::vector<int> categoryItems(int category) const;

private:
    EmpireState& empire();
    void refresh();
    std::string fieldName(int field) const;

    Assets& assets_;
    Game& game_;
    Panel panel_;
    Panel fields_;
    Panel fieldDb_;
    Panel objects_;
    Panel objShips_;
    Panel objBuildings_;
    Point pointer_;
    Page page_ = Page::Management;
    int dbField_ = 0;
    int category_ = 0;
    int entry_ = 0;
    int listFirst_ = 0;
    int textFirst_ = 0;
    bool statistics_ = false;
    Spin spin_ = Spin::Right;
    float yaw_ = 0.6f;
    float pitch_ = 0.3f;
    std::uint64_t lastMs_ = 0;
    bool modelShown_ = false;

    Panel& objectPanel() { return category_ == kCategories - 1 ? objShips_ : objBuildings_; }
    const Panel& objectPanel() const { return category_ == kCategories - 1 ? objShips_ : objBuildings_; }
    Rect listRect() const;
    int listLines() const;
    void drawDatabase(Canvas& canvas, const Font& f15, const Font& f20, std::uint64_t ms);
    void refreshDatabase();
};

} // namespace nagus
