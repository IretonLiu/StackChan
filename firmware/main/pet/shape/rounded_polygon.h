/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "rounded_polygon_controls.h"
#include "shape_path.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace pet::shape {

/**
 * Canonical local-space source geometry for a convex rounded polygon. The
 * directed edge at index i runs from vertex i to vertex (i + 1) % count.
 * Corners own tangent rails; edge controls are derived from those rails.
 */
struct RoundedPolygonParameters {
    uint8_t vertex_count = 0;
    std::array<Vec2, kMaxRoundedPolygonVertices> vertices{};
    std::array<CornerControls, kMaxRoundedPolygonVertices> corners{};
    std::array<EdgeTension, kMaxRoundedPolygonVertices> edges{};
};

enum class RoundedPolygonSampleError : uint8_t {
    None,
    InvalidVertexCount,
    NonFiniteParameters,
    NonFiniteTransform,
    NonFiniteStyle,
    DegenerateOrNonConvex,
    NonFiniteGeometry,
};

struct RoundedPolygonEdgeGeometry {
    Vec2 start{};
    Vec2 first_control{};
    Vec2 second_control{};
    Vec2 end{};
};

struct RoundedPolygonCornerGeometry {
    Vec2 start{};
    Vec2 first_control{};
    Vec2 second_control{};
    Vec2 end{};
};

struct RoundedPolygonSample {
    ShapePath path{};
    std::array<Vec2, kMaxRoundedPolygonVertices> landmarks{};
    std::array<RoundedPolygonEdgeGeometry, kMaxRoundedPolygonVertices> edges{};
    std::array<RoundedPolygonCornerGeometry, kMaxRoundedPolygonVertices> corners{};
    RoundedPolygonParameters parameters{};
    bool parameters_were_clamped    = false;
    RoundedPolygonSampleError error = RoundedPolygonSampleError::None;
};

/**
 * Samples a convex, closed rounded polygon into alternating edge and corner
 * cubics. Invalid topology and non-finite values fail loudly; finite controls
 * are clamped to the same safe ranges used by the first ear template.
 */
[[nodiscard]] bool sampleRoundedPolygon(const RoundedPolygonParameters& parameters, const Affine2D& world,
                                        RoundedPolygonSample& sample, PathStyle style = {});

}  // namespace pet::shape
