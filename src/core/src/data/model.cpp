// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/data/model.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <set>

namespace nagus {

namespace {

// The counts of corners and points follow each other, as do the offsets of
// the root node, the corners and the points.
constexpr std::size_t kCornerCount = 0x10;
constexpr std::size_t kRoot = 0x3c;
constexpr std::size_t kCornerSize = 40;
constexpr std::size_t kVertexSize = 20;
constexpr std::size_t kNodeSize = 16;
// Faces name their corners by their place in a table of 68-byte entries.
constexpr std::uint32_t kCornerStep = 68;
constexpr std::uint32_t kFaceNode = 4;
constexpr std::size_t kMaxNodes = 65536;
constexpr std::uint32_t kMaxCorners = 64;

float number(Reader& r) {
    const float f = std::bit_cast<float>(r.u32());
    if (!std::isfinite(f) || std::fabs(f) > 1e7f) {
        throw FormatError("bad number in model");
    }
    return f;
}

// Texture places and face normals; untextured models leave them unset
// (not numbers), which count as 0.
float loose(Reader& r) {
    const float f = std::bit_cast<float>(r.u32());
    return std::isfinite(f) && std::fabs(f) < 1e7f ? f : 0.0f;
}

} // namespace

Model readModel(ByteView data) {
    Reader head(data);
    head.seek(kCornerCount);
    const std::size_t cornerCount = head.u32();
    const std::size_t vertexCount = head.u32();
    head.seek(kRoot);
    const std::size_t root = head.u32();
    const std::size_t cornersAt = head.u32();
    const std::size_t verticesAt = head.u32();
    if (cornersAt > data.size() || cornerCount > (data.size() - cornersAt) / kCornerSize || verticesAt > data.size() ||
        vertexCount > (data.size() - verticesAt) / kVertexSize) {
        throw FormatError("bad model tables");
    }
    Model m;
    m.vertices.resize(vertexCount);
    for (std::size_t i = 0; i < vertexCount; ++i) {
        Reader r(data, verticesAt + i * kVertexSize);
        Vec3& v = m.vertices[i];
        v.x = number(r);
        v.y = number(r);
        v.z = number(r);
        m.radius = std::max(m.radius, std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
    }
    std::vector<Model::Corner> corners(cornerCount);
    for (std::size_t i = 0; i < cornerCount; ++i) {
        Reader r(data, cornersAt + i * kCornerSize + 16);
        Model::Corner& c = corners[i];
        c.u = loose(r);
        c.v = loose(r);
        r.skip(4);
        const std::uint32_t vertex = r.u32();
        if (vertex >= vertexCount) {
            throw FormatError("bad model corner");
        }
        c.vertex = static_cast<int>(vertex);
    }
    // The faces hang in a tree of 16-byte nodes: a type, a pointer, and two
    // links. Type 4 nodes carry a face to draw; the faces of other nodes
    // only split space.
    std::set<std::size_t> seen;
    std::vector<std::size_t> todo{root};
    std::vector<std::size_t> faces;
    while (!todo.empty()) {
        const std::size_t at = todo.back();
        todo.pop_back();
        if (at == 0 || !seen.insert(at).second) {
            continue;
        }
        if (seen.size() > kMaxNodes || at > data.size() || data.size() - at < kNodeSize) {
            throw FormatError("bad model node");
        }
        Reader r(data, at);
        const std::uint32_t type = r.u32();
        const std::uint32_t face = r.u32();
        todo.push_back(r.u32());
        todo.push_back(r.u32());
        if (type == kFaceNode) {
            faces.push_back(face);
        }
    }
    std::sort(faces.begin(), faces.end());
    faces.erase(std::unique(faces.begin(), faces.end()), faces.end());
    for (const std::size_t at : faces) {
        Reader r(data);
        r.seek(at);
        r.skip(16);
        Model::Face f;
        f.normal.x = loose(r);
        f.normal.y = loose(r);
        f.normal.z = loose(r);
        r.skip(16);
        const std::uint32_t n = r.u32();
        if (n > kMaxCorners) {
            throw FormatError("bad model face");
        }
        for (std::uint32_t i = 0; i < n; ++i) {
            const std::uint32_t place = r.u32();
            if (place % kCornerStep != 0 || place / kCornerStep >= cornerCount) {
                throw FormatError("bad model face corner");
            }
            f.corners.push_back(corners[place / kCornerStep]);
        }
        // Points and lines (lights) are not drawn.
        if (f.corners.size() >= 3) {
            m.faces.push_back(std::move(f));
        }
    }
    return m;
}

} // namespace nagus
