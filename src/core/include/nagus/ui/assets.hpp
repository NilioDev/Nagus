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
#include "nagus/data/model.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/data/tips.hpp"

#include <map>
#include <memory>
#include <optional>
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
    // A 3D model by name (such as "hh1_a.hob"); nothing when the game lacks
    // it or it is damaged or empty.
    const Model* model(const std::string& name);
    const Lexicon& lexicon() const { return lexicon_; }
    // The texts of a file of NUL-separated texts (such as racedesc.rst).
    const std::vector<std::string>& texts(const std::string& name);
    // Counts the buttons pressed, for the click sound.
    void clicked() { ++clicks_; }
    std::uint64_t clicks() const { return clicks_; }
    // The tooltips (empty when the game has none, or they are damaged).
    const Tips& tips();

    // The button under the pointer, for its tooltip. Each panel says when the
    // pointer comes onto one of its buttons and when it leaves it.
    void hover(const void* owner, std::optional<std::uint32_t> button);
    // Forgets the button (when the screen changes or the panel goes).
    void unhover(const void* owner) {
        if (hoverOwner_ == owner) {
            hovered_.reset();
            hoverOwner_ = nullptr;
        }
    }
    std::optional<std::uint32_t> hovered() const { return hovered_; }
    void clearHover() {
        hovered_.reset();
        hoverOwner_ = nullptr;
    }

private:
    std::shared_ptr<Resources> resources_;
    Lexicon lexicon_;
    std::map<std::string, Image> images_;
    std::map<std::string, std::unique_ptr<Font>> fonts_;
    std::map<std::string, Layout> layouts_;
    std::map<std::string, Cursor> cursors_;
    std::map<std::string, Animation> animations_;
    std::map<std::string, std::optional<Model>> models_;
    std::map<std::string, std::vector<std::string>> texts_;
    std::uint64_t clicks_ = 0;
    std::optional<Tips> tips_;
    const void* hoverOwner_ = nullptr;
    std::optional<std::uint32_t> hovered_;
};

} // namespace nagus
