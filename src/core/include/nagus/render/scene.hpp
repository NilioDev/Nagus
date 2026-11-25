// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small 3D drawing of models into a part of the canvas, with a depth
// buffer, textures and one light: the combat view.
#pragma once

#include "nagus/data/model.hpp"
#include "nagus/render/canvas.hpp"

#include <optional>
#include <vector>

namespace nagus {

struct Camera {
    Vec3 eye{0, 0, -1000};
    Vec3 target{};
    // Pixels from the middle of the view to its side, for a point one unit
    // away and one unit off.
    float focal = 600;
};

class Scene {
public:
    Scene(Canvas& canvas, const Rect& view, const Camera& camera);

    struct Projected {
        float x = 0;
        float y = 0;
        float depth = 0;
    };
    // Where a point shows; nothing when it is behind the camera.
    std::optional<Projected> project(Vec3 p) const;
    // How many pixels a length shows as at a depth.
    float scale(float depth) const { return camera_.focal / depth; }

    // A model tipped by `pitch` (its front, +z, up at a quarter turn), then
    // turned by `yaw` about the vertical (its front towards +x at a quarter
    // turn), scaled and moved; `light` 0..1 dims it.
    void model(const Model& m, const Image& texture, Vec3 at, float yaw, float size, float light = 1.0f, float pitch = 0.0f);
    // A ball, lit from the side of the light.
    void ball(Vec3 at, float radius, Pixel colour);
    // A line of light between two points, `width` pixels wide.
    void beam(Vec3 a, Vec3 b, Pixel colour, int width = 2);
    // A round glow, brightest in the middle; `strength` 0..1.
    void glow(Vec3 at, float radius, Pixel colour, float strength);

private:
    struct Point2 {
        float x, y, w, u, v; // w is 1/depth
    };
    void triangle(const Point2& a, const Point2& b, const Point2& c, const Image& texture, float light);
    Vec3 view(Vec3 p) const;
    void add(int x, int y, Pixel colour, float strength);

    Image& image_;
    Rect view_;
    Camera camera_;
    Vec3 right_, up_, forward_;
    Vec3 light_;
    std::vector<float> depth_; // 1/depth per pixel of the view, 0 for none
};

} // namespace nagus
