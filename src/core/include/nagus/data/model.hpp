// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's 3D models (.hob): ships, stations and effects for the
// combat view. See docs/formats/models.md.
#pragma once

#include "nagus/util/bytes.hpp"

#include <vector>

namespace nagus {

struct Vec3 {
    float x = 0;
    float y = 0;
    float z = 0;
};

struct Model {
    // A corner of a face: its point and where it is on the texture (0..1).
    struct Corner {
        int vertex = 0;
        float u = 0;
        float v = 0;
    };
    struct Face {
        Vec3 normal;
        std::vector<Corner> corners;
    };
    std::vector<Vec3> vertices;
    std::vector<Face> faces;
    // The distance of the farthest point from the origin.
    float radius = 0;

    bool empty() const { return faces.empty(); }
};

// Reads a model; throws FormatError when it is damaged.
Model readModel(ByteView data);

} // namespace nagus
