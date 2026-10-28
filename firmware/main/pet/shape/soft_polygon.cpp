/*
 * SPDX-License-Identifier: MIT
 */
#include "soft_polygon.h"

#include "rounded_polygon.h"

#include <algorithm>
#include <cmath>

namespace pet::shape {
namespace {

bool finite(const CornerControls& value)
{
    return std::isfinite(value.roundness) && std::isfinite(value.cap_tension);
}

bool finite(const EdgeTension& value)
{
    return std::isfinite(value.bulge) && std::isfinite(value.start) && std::isfinite(value.end);
}

bool finite(const SoftPolygonEarParameters& value)
{
    return std::isfinite(value.lean) && std::isfinite(value.tip_bend) && finite(value.tip_corner) &&
           finite(value.base_corners) && finite(value.left_side) && finite(value.right_side) && finite(value.base);
}

bool finite(const Affine2D& value)
{
    return std::isfinite(value.m00) && std::isfinite(value.m01) && std::isfinite(value.tx) &&
           std::isfinite(value.m10) && std::isfinite(value.m11) && std::isfinite(value.ty);
}

bool finite(const PathStyle& value)
{
    return std::isfinite(value.opacity) && std::isfinite(value.stroke_width);
}

SoftPolygonEarParameters clampEarControls(SoftPolygonEarParameters result, bool& was_clamped)
{
    const auto clamp = [&](float& value, float low, float high) {
        const float source = value;
        value = std::clamp(value, low, high);
        was_clamped = was_clamped || value != source;
    };
    clamp(result.lean, -0.25f, 0.25f);
    clamp(result.tip_bend, -0.20f, 0.20f);
    return result;
}

RoundedPolygonParameters toRoundedPolygon(const SoftPolygonEarParameters& parameters)
{
    RoundedPolygonParameters result;
    result.corner_count = 3;
    result.corner_positions = {{{-0.5f, 0.0f}, {parameters.lean + parameters.tip_bend * 0.5f, -1.0f}, {0.5f, 0.0f}}};
    result.corners[0] = parameters.base_corners;
    result.corners[1] = parameters.tip_corner;
    result.corners[2] = parameters.base_corners;
    result.edges[0] = parameters.left_side;
    result.edges[1] = parameters.right_side;
    result.edges[2] = parameters.base;
    return result;
}

SoftPolygonEarParameters fromRoundedPolygon(const SoftPolygonEarParameters& source,
                                            const RoundedPolygonParameters& rounded)
{
    SoftPolygonEarParameters result = source;
    result.tip_corner = rounded.corners[1];
    result.base_corners = rounded.corners[0];
    result.left_side = rounded.edges[0];
    result.right_side = rounded.edges[1];
    result.base = rounded.edges[2];
    return result;
}

}  // namespace

bool sampleSoftPolygonEar(const SoftPolygonEarParameters& source, const Affine2D& world,
                          SoftPolygonEarSample& sample, PathStyle style)
{
    sample = {};
    if (!finite(source)) {
        sample.error = SoftPolygonEarSampleError::NonFiniteParameters;
        return false;
    }
    if (!finite(world)) {
        sample.error = SoftPolygonEarSampleError::NonFiniteTransform;
        return false;
    }
    if (!finite(style)) {
        sample.error = SoftPolygonEarSampleError::NonFiniteStyle;
        return false;
    }

    bool ear_was_clamped = false;
    const SoftPolygonEarParameters ear = clampEarControls(source, ear_was_clamped);
    RoundedPolygonSample polygon;
    if (!sampleRoundedPolygon(toRoundedPolygon(ear), world, polygon, style)) {
        sample.error = SoftPolygonEarSampleError::NonFiniteGeometry;
        return false;
    }

    SoftPolygonEarSample result;
    result.path = polygon.path;
    result.parameters = fromRoundedPolygon(ear, polygon.parameters);
    result.parameters_were_clamped = ear_was_clamped || polygon.parameters_were_clamped;
    result.landmarks = {polygon.landmarks[0], polygon.landmarks[1], polygon.landmarks[2]};
    for (size_t index = 0; index < result.edges.size(); ++index) {
        result.edges[index] = {polygon.edges[index].start, polygon.edges[index].first_control,
                               polygon.edges[index].second_control, polygon.edges[index].end};
        result.corners[index] = {polygon.corners[index].start, polygon.corners[index].first_control,
                                 polygon.corners[index].second_control, polygon.corners[index].end};
    }
    sample = result;
    return true;
}

}  // namespace pet::shape
