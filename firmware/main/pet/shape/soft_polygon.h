/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "shape_path.h"

#include <array>

namespace pet::shape {

/**
 * A rounded-corner patch. Its two cubic controls are authored from this
 * bounded shape data; adjoining edge controls are derived from its tangents.
 */
struct CornerControls {
    float roundness = 0.08f;
    float cap_tension = 0.55f;
};

/**
 * Handle lengths for one directed edge. Directions are never independently
 * editable: the preceding/following corner patches own those tangent rails.
 */
struct EdgeTension {
    // Shared tangent-rail bend: positive values bow the directed edge outward.
    float bulge = 0.0f;
    float start = 0.28f;
    float end = 0.28f;
};

/**
 * Semantic controls for the first `soft_polygon` template: a three-landmark
 * cat ear. Geometry is normalized around its base attachment pivot:
 *
 *   base_left (-0.5, 0) -- tip (lean, -1) -- base_right (0.5, 0)
 *
 * Pixel dimensions belong to Transform2D::scale, not to this template.
 */
struct SoftPolygonEarParameters {
    float lean = 0.0f;
    float tip_bend = 0.0f;
    CornerControls tip_corner{0.08f, 0.55f};
    CornerControls base_corners{0.06f, 0.55f};
    // Directed landmark order: base_left -> tip -> base_right -> base_left.
    EdgeTension left_side{};
    EdgeTension right_side{};
    EdgeTension base{};
};

struct EarLandmarks {
    Vec2 base_left{};
    Vec2 tip{};
    Vec2 base_right{};
};

enum class EarEdge : uint8_t {
    LeftSide,
    RightSide,
    Base,
};

enum class EarCorner : uint8_t {
    BaseLeft,
    Tip,
    BaseRight,
};

/** Template-local data for one edge cubic, primarily for host inspection. */
struct EarEdgeGeometry {
    Vec2 start{};
    Vec2 first_control{};
    Vec2 second_control{};
    Vec2 end{};
};

/** Template-local data for one corner cubic, primarily for host inspection. */
struct EarCornerGeometry {
    Vec2 start{};
    Vec2 first_control{};
    Vec2 second_control{};
    Vec2 end{};
};

enum class SoftPolygonEarSampleError : uint8_t {
    None,
    NonFiniteParameters,
    NonFiniteTransform,
    NonFiniteStyle,
    NonFiniteGeometry,
};

struct SoftPolygonEarSample {
    ShapePath path{};
    EarLandmarks landmarks{};
    std::array<EarEdgeGeometry, 3> edges{};
    std::array<EarCornerGeometry, 3> corners{};
    SoftPolygonEarParameters parameters{};
    bool parameters_were_clamped = false;
    SoftPolygonEarSampleError error = SoftPolygonEarSampleError::None;
};

/**
 * Samples a closed six-cubic ear loop in design-pixel coordinates.
 *
 * Public controls are clamped at this boundary. Non-finite parameters,
 * transforms, and styles are rejected rather than silently converted into
 * geometry; on failure `sample.error` identifies the invalid input. At every
 * edge/corner join, the edge's adjacent handle is regenerated from the corner
 * tangent rail.
 */
[[nodiscard]] bool sampleSoftPolygonEar(const SoftPolygonEarParameters& parameters, const Affine2D& world,
                                        SoftPolygonEarSample& sample, PathStyle style = {});

}  // namespace pet::shape
