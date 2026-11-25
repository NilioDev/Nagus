// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game's pictures, fonts, layouts and texts, read from the resources
// once and kept.
#pragma once

#include "nagus/data/animation.hpp"
#include "nagus/data/cursor.hpp"
#include "nagus/data/font.hpp"
#include "nagus/data/image.hpp"
#include "nagus/data/layout.hpp"
#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nagus {

class Assets {
public:
    explicit Assets(std::shared_ptr<Resources> resources);

    const Resources& resources() const { return *resources_; }
    // A picture by name; a missing or damaged picture gives an empty one.
    const Image& image(const std::string& name);
    const Font& font(const std::string& name);
    const Layout& layout(const std::string& name);
    const Cursor& cursor(const std::string& name);
    // An animation by name; a missing or damaged one has no frames.
    const Animation& animation(const std::string& name);
    const Lexicon& lexicon() const { return lexicon_; }
    // The texts of a file of NUL-separated texts (such as racedesc.rst).
    const std::vector<std::string>& texts(const std::string& name);

private:
    std::shared_ptr<Resources> resources_;
    Lexicon lexicon_;
    std::map<std::string, Image> images_;
    std::map<std::string, std::unique_ptr<Font>> fonts_;
    std::map<std::string, Layout> layouts_;
    std::map<std::string, Cursor> cursors_;
    std::map<std::string, Animation> animations_;
    std::map<std::string, std::vector<std::string>> texts_;
};

} // namespace nagus
