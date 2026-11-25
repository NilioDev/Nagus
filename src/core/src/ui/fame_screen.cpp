// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/fame_screen.hpp"

#include <algorithm>
#include <fstream>

namespace nagus {

namespace {

constexpr std::size_t kMaxText = 60;
constexpr Pixel kText = 0xffffcc66u;
constexpr Pixel kLatest = 0xffffffffu;

std::string clean(std::string s) {
    std::erase_if(s, [](char c) { return c == '|' || c == '\n' || c == '\r'; });
    if (s.size() > kMaxText) {
        s.resize(kMaxText);
    }
    return s;
}

} // namespace

std::vector<FameEntry> loadFame(const std::filesystem::path& file) {
    std::vector<FameEntry> out;
    std::ifstream in(file);
    std::string line;
    while (out.size() < kFameEntries && std::getline(in, line)) {
        const auto a = line.find('|');
        const auto b = a == std::string::npos ? a : line.find('|', a + 1);
        if (b == std::string::npos) {
            continue;
        }
        FameEntry e;
        try {
            e.score = std::clamp(std::stoi(line.substr(0, a)), 0, 99999999);
        } catch (const std::exception&) {
            continue;
        }
        e.name = clean(line.substr(a + 1, b - a - 1));
        e.description = clean(line.substr(b + 1));
        out.push_back(std::move(e));
    }
    std::stable_sort(out.begin(), out.end(), [](const FameEntry& x, const FameEntry& y) { return x.score > y.score; });
    return out;
}

void saveFame(const std::filesystem::path& file, const std::vector<FameEntry>& entries) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    const auto part = std::filesystem::path(file).concat(".part");
    {
        std::ofstream out(part, std::ios::trunc);
        for (std::size_t i = 0; i < entries.size() && i < kFameEntries; ++i) {
            out << entries[i].score << '|' << clean(entries[i].name) << '|' << clean(entries[i].description) << '\n';
        }
        if (!out) {
            return;
        }
    }
    std::filesystem::rename(part, file, ec);
}

int addFame(std::vector<FameEntry>& entries, FameEntry entry) {
    entry.name = clean(entry.name);
    entry.description = clean(entry.description);
    const auto at = std::find_if(entries.begin(), entries.end(), [&](const FameEntry& e) { return e.score < entry.score; });
    const auto place = static_cast<std::size_t>(at - entries.begin());
    if (place >= kFameEntries) {
        return -1;
    }
    entries.insert(at, std::move(entry));
    if (entries.size() > kFameEntries) {
        entries.resize(kFameEntries);
    }
    return static_cast<int>(place);
}

FameScreen::FameScreen(Assets& assets, std::vector<FameEntry> entries, int latest)
    : assets_(assets), panel_(assets, "fame.wdf"), entries_(std::move(entries)), latest_(latest) {}

void FameScreen::draw(Canvas& canvas, std::uint64_t) {
    panel_.draw(canvas);
    const Layout& item = assets_.layout("fameitem.wdf");
    const Font& font = assets_.font("human15.fnt");
    for (std::size_t i = 0; i < entries_.size() && i < kFameEntries; ++i) {
        const Rect row = panel_.rect(kRows + static_cast<std::uint32_t>(i));
        const Pixel colour = static_cast<int>(i) == latest_ ? kLatest : kText;
        auto field = [&](std::uint32_t id) {
            const auto* e = item.find(id);
            return e ? Rect{row.x + e->rect.x, row.y + e->rect.y, e->rect.w, e->rect.h} : row;
        };
        const FameEntry& f = entries_[i];
        canvas.text(font, f.name, field(kName), colour, Align::Left);
        canvas.text(font, f.description, field(kDescription), colour, Align::Left);
        canvas.text(font, std::to_string(f.score), field(kScore), colour, Align::Right);
    }
}

void FameScreen::pointerMoved(Point p) {
    last_ = p;
    panel_.pointerMoved(p);
}

void FameScreen::pointerReleased(Point p) {
    last_ = p;
    if (panel_.pointerReleased(p) == kReturn) {
        outcome_ = Outcome::Back;
    }
}

std::optional<Point> FameScreen::command(Command c) {
    if (c == Command::Back || c == Command::Start) {
        outcome_ = Outcome::Back;
        return std::nullopt;
    }
    return panel_.step(c, last_);
}

} // namespace nagus
