// SPDX-License-Identifier: GPL-3.0-or-later
//
// The empire summary (the Summary button on the galaxy screen): what
// happened last turn, the treaties with the races met, and the player's
// systems, each in a list. A system opens on the system screen.
#pragma once

#include "nagus/game/game.hpp"
#include "nagus/ui/menus.hpp"

#include <vector>

namespace nagus {

class SummaryScreen : public MenuScreen {
public:
    static constexpr std::uint32_t kEvents = 0x1b59;
    static constexpr std::uint32_t kRelations = 0x1b5a;
    static constexpr std::uint32_t kSystems = 0x1b5b;
    static constexpr std::uint32_t kClose = 0x1b5c;
    static constexpr std::uint32_t kUp = 0x1b61;
    static constexpr std::uint32_t kDown = 0x1b62;
    static constexpr std::uint32_t kList = 0x1b5d;

    enum class Page { Events, Relations, Systems };
    struct Row {
        std::array<std::string, 3> cells;
        Pixel colour = 0;
        int colony = -1; // the system's colony, on the systems page
    };

    SummaryScreen(Assets& assets, const Game& game, const std::vector<std::pair<std::string, Pixel>>& events,
                  Page page = Page::Events);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override { panel_.pointerPressed(p); }
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;

    Page page() const { return page_; }
    void show(Page page);
    const std::vector<Row>& rows() const { return rows_; }
    // Where a row is, while it is in view.
    std::optional<Rect> rowRect(std::size_t row) const;
    // The colony chosen to be opened (with Outcome::Open).
    int chosenColony() const { return chosenColony_; }
    Rect rect(std::uint32_t id) const { return panel_.rect(id); }

private:
    int visibleRows() const;
    void scrollBy(int rows);

    Assets& assets_;
    const Game& game_;
    std::vector<std::pair<std::string, Pixel>> events_;
    Panel panel_;
    Page page_ = Page::Events;
    std::vector<Row> rows_;
    int first_ = 0;
    int row_ = 0; // the row the keys and gamepad are on
    int chosenColony_ = -1;
    Point pointer_{};
};

} // namespace nagus
