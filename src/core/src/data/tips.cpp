// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/data/tips.hpp"

namespace nagus {

namespace {
constexpr std::size_t kHeader = 16;
constexpr std::size_t kEntry = 16;
// No more tips than this; the original has a few hundred.
constexpr std::uint32_t kMaxTips = 10000;
} // namespace

Tips::Tips(ByteView data) {
    if (data.size() < kHeader) {
        throw FormatError("tooltip table too short");
    }
    Reader r(data);
    const std::uint32_t count = r.u32();
    if (count > kMaxTips || count > (data.size() - kHeader) / kEntry) {
        throw FormatError("tooltip table larger than its file");
    }
    const std::size_t textStart = kHeader + count * kEntry;
    for (std::uint32_t i = 0; i < count; ++i) {
        r.seek(kHeader + i * kEntry);
        const std::uint32_t id = r.u32();
        const std::uint32_t length = r.u32();
        r.skip(4); // an address in the original's memory
        const std::uint32_t offset = r.u32();
        if (offset > data.size() - textStart || length > data.size() - textStart - offset) {
            throw FormatError("tooltip text past the end of its file");
        }
        std::vector<std::string> lines;
        std::string line;
        for (std::size_t at = textStart + offset; at < textStart + offset + length; ++at) {
            const auto c = static_cast<char>(data[at]);
            if (c == '\0') {
                break;
            }
            if (c == '\n') {
                lines.push_back(line);
                line.clear();
            } else if (c != '\r' && c != '\v') {
                line.push_back(c);
            }
        }
        lines.push_back(line);
        // Each text starts with a space and ends with a line break.
        for (auto& l : lines) {
            while (!l.empty() && l.front() == ' ') {
                l.erase(l.begin());
            }
            while (!l.empty() && l.back() == ' ') {
                l.pop_back();
            }
        }
        while (!lines.empty() && lines.back().empty()) {
            lines.pop_back();
        }
        if (!lines.empty()) {
            tips_[id] = std::move(lines);
        }
    }
}

const std::vector<std::string>& Tips::operator[](std::uint32_t button) const {
    static const std::vector<std::string> kNone;
    const auto it = tips_.find(button);
    return it == tips_.end() ? kNone : it->second;
}

} // namespace nagus
