// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game's table of interface texts (lexicon.dic). The engine refers to a
// text by its number in the table; the numbers are the same with and without
// the 1.0.2 patch, which only changes some of the texts. See
// docs/formats/texts.md.
#pragma once

#include "nagus/util/bytes.hpp"

#include <string>
#include <vector>

namespace nagus {

class Lexicon {
public:
    Lexicon() = default;
    explicit Lexicon(ByteView data);

    std::size_t size() const { return texts_.size(); }
    // The text with that number, or an empty text when there is none.
    const std::string& operator[](std::size_t index) const;

private:
    std::vector<std::string> texts_;
};

// Fills a text's places: "%1", "%s1", "%d1" take the first value, and so
// on; a place without a number ("%s", "%d") takes the next one in turn.
std::string fillText(std::string_view pattern, const std::vector<std::string>& values);

// Numbers of the texts the engine uses (see docs/rules/texts.md for how each
// was found).
namespace text {
inline constexpr std::size_t kAccept = 1;
inline constexpr std::size_t kCancel = 117;
} // namespace text

} // namespace nagus
