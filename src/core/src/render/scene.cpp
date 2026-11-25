// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/render/scene.hpp"

#include <algorithm>
#include <cmath>

namespace nagus {

namespace {

constexpr float kNear = 10.0f;
constexpr float kAmbient = 0.35f;

Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vec3 normalised(Vec3 a) {
    const float l = std::sqrt(dot(a, a));
    return l > 0 ? a * (1.0f / l) : Vec3{0, 0, 1};
}
// Turns about the vertical: +z towards +x at a quarter turn.
Vec3 turned(Vec3 p, float c, float s) { return {p.x * c + p.z * s, p.y, p.z * c - p.x * s}; }
// Tips about the side axis: +z towards +y at a quarter turn.
Vec3 tipped(Vec3 p, float c, float s) { return {p.x, p.y * c + p.z * s, p.z * c - p.y * s}; }

int channel(Pixel p, int shift) { return static_cast<int>((p >> shift) & 255u); }

Pixel shade(Pixel p, float light) {
    const auto f = [&](int shift) { return std::min(255, static_cast<int>(static_cast<float>(channel(p, shift)) * light)); };
    return rgb(f(16), f(8), f(0));
}

} // namespace

Scene::Scene(Canvas& canvas, const Rect& view, const Camera& camera)
    : image_(canvas.target()), camera_(camera), light_(normalised({-0.4f, 0.7f, -0.6f})) {
    // Only the part of the view on the canvas.
    const int x0 = std::max(0, view.x);
    const int y0 = std::max(0, view.y);
    const int x1 = std::min(image_.width, view.x + view.w);
    const int y1 = std::min(image_.height, view.y + view.h);
    view_ = {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
    forward_ = normalised(camera_.target - camera_.eye);
    right_ = normalised(cross({0, 1, 0}, forward_));
    up_ = cross(forward_, right_);
    depth_.assign(static_cast<std::size_t>(view_.w) * static_cast<std::size_t>(view_.h), 0.0f);
}

Vec3 Scene::view(Vec3 p) const {
    const Vec3 d = p - camera_.eye;
    return {dot(d, right_), dot(d, up_), dot(d, forward_)};
}

std::optional<Scene::Projected> Scene::project(Vec3 p) const {
    const Vec3 v = view(p);
    if (v.z < kNear) {
        return std::nullopt;
    }
    const float s = camera_.focal / v.z;
    return Projected{static_cast<float>(view_.x) + static_cast<float>(view_.w) / 2 + v.x * s,
                     static_cast<float>(view_.y) + static_cast<float>(view_.h) / 2 - v.y * s, v.z};
}

void Scene::model(const Model& m, const Image& texture, Vec3 at, float yaw, float size, float light, float pitch) {
    if (m.radius <= 0 || texture.empty()) {
        return;
    }
    const float k = size / m.radius;
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    const float pc = std::cos(pitch);
    const float ps = std::sin(pitch);
    std::vector<Vec3> world(m.vertices.size());
    std::vector<std::optional<Projected>> shown(m.vertices.size());
    for (std::size_t i = 0; i < m.vertices.size(); ++i) {
        world[i] = turned(tipped(m.vertices[i], pc, ps), c, s) * k + at;
        shown[i] = project(world[i]);
    }
    std::vector<Point2> pts;
    for (const Model::Face& f : m.faces) {
        pts.clear();
        bool ok = true;
        for (const Model::Corner& corner : f.corners) {
            const auto& p = shown[static_cast<std::size_t>(corner.vertex)];
            if (!p) {
                ok = false;
                break;
            }
            pts.push_back({p->x, p->y, 1.0f / p->depth, corner.u, corner.v});
        }
        if (!ok) {
            continue;
        }
        // Lit on the side the camera sees.
        Vec3 n = normalised(turned(tipped(f.normal, pc, ps), c, s));
        if (dot(n, camera_.eye - world[static_cast<std::size_t>(f.corners[0].vertex)]) < 0) {
            n = n * -1.0f;
        }
        const float lit = light * (kAmbient + (1.0f - kAmbient) * std::max(0.0f, dot(n, light_)));
        for (std::size_t i = 1; i + 1 < pts.size(); ++i) {
            triangle(pts[0], pts[i], pts[i + 1], texture, lit);
        }
    }
}

void Scene::triangle(const Point2& a, const Point2& b, const Point2& c, const Image& texture, float light) {
    const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::fabs(area) < 0.01f) {
        return;
    }
    const int x0 = std::max(view_.x, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
    const int x1 = std::min(view_.x + view_.w - 1, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
    const int y0 = std::max(view_.y, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
    const int y1 = std::min(view_.y + view_.h - 1, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));
    const float inv = 1.0f / area;
    const float tw = static_cast<float>(texture.width);
    const float th = static_cast<float>(texture.height);
    for (int y = y0; y <= y1; ++y) {
        const float py = static_cast<float>(y) + 0.5f;
        for (int x = x0; x <= x1; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float la = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) * inv;
            const float lb = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) * inv;
            const float lc = 1.0f - la - lb;
            if (la < 0 || lb < 0 || lc < 0) {
                continue;
            }
            const float w = la * a.w + lb * b.w + lc * c.w;
            float& d = depth_[static_cast<std::size_t>(y - view_.y) * static_cast<std::size_t>(view_.w) +
                              static_cast<std::size_t>(x - view_.x)];
            if (w <= d) {
                continue;
            }
            // Perspective-correct texture place.
            const float u = (la * a.u * a.w + lb * b.u * b.w + lc * c.u * c.w) / w;
            const float v = (la * a.v * a.w + lb * b.v * b.w + lc * c.v * c.w) / w;
            const int tx = std::clamp(static_cast<int>((u - std::floor(u)) * tw), 0, texture.width - 1);
            const int ty = std::clamp(static_cast<int>((v - std::floor(v)) * th), 0, texture.height - 1);
            const Pixel t = texture.at(tx, ty);
            if ((t >> 24) == 0) {
                continue;
            }
            d = w;
            image_.at(x, y) = shade(t, light);
        }
    }
}

void Scene::ball(Vec3 at, float radius, Pixel colour) {
    const auto p = project(at);
    if (!p) {
        return;
    }
    const float r = radius * scale(p->depth);
    const float w = 1.0f / p->depth;
    // The light as seen from the camera.
    const float lx = dot(light_, right_);
    const float ly = dot(light_, up_);
    const float lz = -dot(light_, forward_);
    const int x0 = std::max(view_.x, static_cast<int>(p->x - r));
    const int x1 = std::min(view_.x + view_.w - 1, static_cast<int>(p->x + r));
    const int y0 = std::max(view_.y, static_cast<int>(p->y - r));
    const int y1 = std::min(view_.y + view_.h - 1, static_cast<int>(p->y + r));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f - p->x) / r;
            const float dy = (p->y - static_cast<float>(y) - 0.5f) / r;
            const float dd = dx * dx + dy * dy;
            if (dd > 1.0f) {
                continue;
            }
            float& d = depth_[static_cast<std::size_t>(y - view_.y) * static_cast<std::size_t>(view_.w) +
                              static_cast<std::size_t>(x - view_.x)];
            if (w <= d) {
                continue;
            }
            d = w;
            const float dz = std::sqrt(1.0f - dd);
            image_.at(x, y) = shade(colour, kAmbient + (1.0f - kAmbient) * std::max(0.0f, dx * lx + dy * ly + dz * lz));
        }
    }
}

void Scene::add(int x, int y, Pixel colour, float strength) {
    if (!view_.contains(x, y) || strength <= 0) {
        return;
    }
    Pixel& p = image_.at(x, y);
    const auto f = [&](int shift) {
        return std::min(255, channel(p, shift) + static_cast<int>(static_cast<float>(channel(colour, shift)) * strength));
    };
    p = rgb(f(16), f(8), f(0));
}

void Scene::beam(Vec3 a, Vec3 b, Pixel colour, int width) {
    const auto pa = project(a);
    const auto pb = project(b);
    if (!pa || !pb) {
        return;
    }
    const float dx = pb->x - pa->x;
    const float dy = pb->y - pa->y;
    const int steps = std::max(1, static_cast<int>(std::max(std::fabs(dx), std::fabs(dy))));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const int x = static_cast<int>(pa->x + dx * t);
        const int y = static_cast<int>(pa->y + dy * t);
        for (int oy = 0; oy < width; ++oy) {
            for (int ox = 0; ox < width; ++ox) {
                add(x + ox, y + oy, colour, 0.9f);
            }
        }
    }
}

void Scene::glow(Vec3 at, float radius, Pixel colour, float strength) {
    const auto p = project(at);
    if (!p) {
        return;
    }
    const float r = std::max(1.0f, radius * scale(p->depth));
    for (int y = static_cast<int>(p->y - r); y <= static_cast<int>(p->y + r); ++y) {
        for (int x = static_cast<int>(p->x - r); x <= static_cast<int>(p->x + r); ++x) {
            const float dx = (static_cast<float>(x) - p->x) / r;
            const float dy = (static_cast<float>(y) - p->y) / r;
            const float dd = dx * dx + dy * dy;
            if (dd < 1.0f) {
                add(x, y, colour, strength * (1.0f - dd));
            }
        }
    }
}

} // namespace nagus
