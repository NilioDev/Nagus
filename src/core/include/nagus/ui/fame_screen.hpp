// SPDX-License-Identifier: GPL-3.0-or-later
//
// The hall of fame: the ten best finished games, kept in a small text
// file beside the settings ("score|name|description" lines).
#pragma once

#include "nagus/ui/menus.hpp"

#include <filesystem>

namespace nagus {

struct FameEntry {
    int score = 0;
    std::string name;
    std::string description;
};
inline constexpr std::size_t kFameEntries = 10;

std::vector<FameEntry> loadFame(const std::filesystem::path& file);
void saveFame(const std::filesystem::path& file, const std::vector<FameEntry>& entries);
// Adds an entry, keeping the best ten (best first). Returns its place, or
// -1 if it is not among them.
int addFame(std::vector<FameEntry>& entries, FameEntry entry);

class FameScreen : public MenuScreen {
public:
    static constexpr std::uint32_t kReturn = 0x26de;
    static constexpr std::uint32_t kRows = 0x26df; // + row
    static constexpr std::uint32_t kTitle = 0x26ed;
    // In fameitem.wdf, inside a row.
    static constexpr std::uint32_t kName = 0x26ea;
    static constexpr std::uint32_t kDescription = 0x26eb;
    static constexpr std::uint32_t kScore = 0x26ec;

    // `latest` is the row of the game just ended (-1 for none).
    FameScreen(Assets& assets, std::vector<FameEntry> entries, int latest);

    void draw(Canvas& canvas, std::uint64_t ms) override;
    void pointerMoved(Point p) override;
    void pointerPressed(Point p) override { panel_.pointerPressed(p); }
    void pointerReleased(Point p) override;
    std::optional<Point> command(Command c) override;
    const std::vector<FameEntry>& entries() const { return entries_; }

private:
    Assets& assets_;
    Panel panel_;
    std::vector<FameEntry> entries_;
    int latest_;
    Point last_{};
};

} // namespace nagus
