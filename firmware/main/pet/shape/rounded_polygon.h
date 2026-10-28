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

// Move + one edge cubic and one corner cubic per corner + Close must fit in
// ShapePath. Raise the shared path capacity deliberately before raising this.
constexpr size_t kMaxRoundedPolygonCorners = 5;

/**
 * Canonical local-space source geometry for a convex rounded polygon. A corner is
 * the smallest primitive: it owns rounding and tangent length, and its joint is the
 * point the edges run through. The directed edge at index i joins corner i to corner
 * (i + 1) % count. Edge controls are derived from the corner tangent rails.
 */
struct RoundedPolygonParameters {
    uint8_t corner_count = 0;
    // Derived view of the corner joints, written in lockstep with corners[].position by
    // the preset compiler and the studio. Vertices are not an authored primitive.
    std::array<Vec2, kMaxRoundedPolygonCorners> corner_positions{};
    std::array<CornerControls, kMaxRoundedPolygonCorners> corners{};
    std::array<EdgeTension, kMaxRoundedPolygonCorners> edges{};
};

enum class RoundedPolygonSampleError : uint8_t {
    None,
    InvalidCornerCount,
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
    std::array<Vec2, kMaxRoundedPolygonCorners> landmarks{};
    std::array<RoundedPolygonEdgeGeometry, kMaxRoundedPolygonCorners> edges{};
    std::array<RoundedPolygonCornerGeometry, kMaxRoundedPolygonCorners> corners{};
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
