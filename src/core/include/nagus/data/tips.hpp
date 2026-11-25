// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game's tooltips (teachtip.tdb): a short help text for each button,
// found by the button's number. See docs/formats/texts.md.
#pragma once

#include "nagus/util/bytes.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nagus {

class Tips {
public:
    Tips() = default;
    // Throws FormatError when the table does not fit in the data.
    explicit Tips(ByteView data);

    std::size_t size() const { return tips_.size(); }
    // The lines of a button's tooltip; empty when it has none.
    const std::vector<std::string>& operator[](std::uint32_t button) const;

private:
    std::map<std::uint32_t, std::vector<std::string>> tips_;
};

} // namespace nagus
