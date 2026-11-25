// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/data/lexicon.hpp"

namespace nagus {

namespace {

constexpr std::size_t kMaxTexts = 65535;
constexpr std::size_t kMaxText = 16384;

} // namespace

Lexicon::Lexicon(ByteView data) {
    Reader r(data);
    const std::uint32_t table = r.u32();
    r.seek(table);
    r.u32(); // used by the original's own look-up, not needed here
    const std::size_t count = r.u16();
    if (count > kMaxTexts) {
        throw FormatError("bad text table");
    }
    texts_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t offset = r.u32();
        const std::uint32_t length = r.u32();
        if (length == 0 || length > kMaxText) {
            throw FormatError("bad text in text table");
        }
        Reader t(data, offset);
        texts_.push_back(t.text(length));
    }
}

const std::string& Lexicon::operator[](std::size_t index) const {
    static const std::string none;
    return index < texts_.size() ? texts_[index] : none;
}

} // namespace nagus
