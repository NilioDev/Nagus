// SPDX-License-Identifier: GPL-3.0-or-later
//
// Screens and the controls that drive them. The app turns mouse, touch,
// keyboard and gamepad input into these calls; everything below is the same
// on every system.
//
// A gamepad works the original's screens the way a mouse does: the d-pad
// makes the pointer jump from button to button, the left stick moves it
// freely, A clicks where it points and B goes back.
#pragma once

#include "nagus/data/layout.hpp"
#include "nagus/render/canvas.hpp"
#include "nagus/ui/assets.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <string>

namespace nagus {

enum class Command { Up, Down, Left, Right, Back, Start, Next, Previous };

struct Point {
    int x = 0;
    int y = 0;
};

class Screen {
public:
    virtual ~Screen() = default;
    virtual void draw(Canvas& canvas, std::uint64_t ms) = 0;
    virtual void pointerMoved(Point) {}
    virtual void pointerPressed(Point) {}
    virtual void pointerReleased(Point) {}
    // A command from the keyboard or gamepad. May ask the pointer to jump.
    virtual std::optional<Point> command(Command) { return std::nullopt; }
    // Typed text, for a screen with a text field ("\b" erases a letter).
    virtual void typed(std::string_view) {}
    // Whether a text field waits for typing (the app then shows a
    // keyboard where there is none).
    virtual bool wantsText() const { return false; }
};

// The buttons of a screen layout: drawing them up or down, clicking them
// with a pointer, and moving between them with a gamepad.
class Panel {
public:
    Panel(Assets& assets, const std::string& layoutName);

    const Layout& layout() const { return *layout_; }
    // Where an element is on the screen.
    Rect rect(std::uint32_t id) const;
    // Places the layout's top left corner (by default where the layout
    // says), for layouts drawn inside another screen's area.
    void setOrigin(Point p) { origin_ = p; }
    Point origin() const;

    // Draws the background and the buttons. Buttons that cannot be used now
    // are drawn darker.
    void draw(Canvas& canvas);

    void setEnabled(std::uint32_t id, bool enabled);
    void setVisible(std::uint32_t id, bool visible);
    // A toggle button shows its "down" picture while on.
    void setOn(std::uint32_t id, bool on);
    bool usable(std::uint32_t id) const;

    // Pointer handling: a click is a press and a release on the same button.
    void pointerMoved(Point p);
    void pointerPressed(Point p);
    std::optional<std::uint32_t> pointerReleased(Point p);

    // The usable button nearest in the given direction from the one under
    // the pointer (or the first one), for the gamepad.
    std::optional<Point> step(Command direction, Point from) const;

private:
    std::optional<Point> origin_;
    struct State {
        bool enabled = true;
        bool visible = true;
        bool on = false;
    };
    std::optional<std::uint32_t> buttonAt(Point p) const;

    Assets& assets_;
    const Layout* layout_;
    std::map<std::uint32_t, State> states_;
    std::optional<std::uint32_t> held_;
    bool overHeld_ = false;
};

// Puts text into a box, breaking lines between words.
void drawParagraph(Canvas& canvas, const Font& font, const std::string& text, const Rect& box, Pixel colour);

} // namespace nagus
