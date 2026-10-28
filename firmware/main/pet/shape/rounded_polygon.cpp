/*
 * SPDX-License-Identifier: MIT
 */
#include "rounded_polygon.h"

#include <algorithm>
#include <cmath>

namespace pet::shape {
namespace {

constexpr float kMinimumEdgeLength = 0.0001f;

bool finite(Vec2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool finite(const Affine2D& value)
{
    return std::isfinite(value.m00) && std::isfinite(value.m01) && std::isfinite(value.tx) &&
           std::isfinite(value.m10) && std::isfinite(value.m11) && std::isfinite(value.ty);
}

bool finite(const CornerControls& value)
{
    return std::isfinite(value.roundness) && std::isfinite(value.cap_tension);
}

bool finite(const EdgeTension& value)
{
    return std::isfinite(value.bulge) && std::isfinite(value.start) && std::isfinite(value.end);
}

bool finite(const PathStyle& value)
{
    return std::isfinite(value.opacity) && std::isfinite(value.stroke_width);
}

Vec2 add(Vec2 left, Vec2 right)
{
    return {left.x + right.x, left.y + right.y};
}

Vec2 subtract(Vec2 left, Vec2 right)
{
    return {left.x - right.x, left.y - right.y};
}

Vec2 scale(Vec2 value, float factor)
{
    return {value.x * factor, value.y * factor};
}

float cross(Vec2 left, Vec2 right)
{
    return left.x * right.y - left.y * right.x;
}

float dot(Vec2 left, Vec2 right)
{
    return left.x * right.x + left.y * right.y;
}

float length(Vec2 value)
{
    return std::sqrt(dot(value, value));
}

Vec2 normalized(Vec2 value)
{
    const float value_length = length(value);
    return value_length < kMinimumEdgeLength ? Vec2{} : scale(value, 1.0f / value_length);
}

void append(ShapePath& path, PathCommand command)
{
    if (path.command_count < path.commands.size()) {
        path.commands[path.command_count++] = command;
    }
}

void appendMove(ShapePath& path, const Affine2D& world, Vec2 point)
{
    const Vec2 transformed = transformPoint(world, point);
    append(path, {PathCommandType::Move, transformed, {}, {}});
    include(path.bounds, transformed);
}

void appendCubic(ShapePath& path, const Affine2D& world, Vec2 first, Vec2 second, Vec2 end)
{
    const Vec2 transformed_first  = transformPoint(world, first);
    const Vec2 transformed_second = transformPoint(world, second);
    const Vec2 transformed_end    = transformPoint(world, end);
    append(path, {PathCommandType::Cubic, transformed_first, transformed_second, transformed_end});
    include(path.bounds, transformed_first);
    include(path.bounds, transformed_second);
    include(path.bounds, transformed_end);
}

bool validTopology(const RoundedPolygonParameters& value)
{
    if (value.corner_count < 3 || value.corner_count > kMaxRoundedPolygonCorners) {
        return false;
    }

    // A single winding direction at every turn makes the v1 primitive convex,
    // avoiding ambiguous outward rails and corner-patch self-intersections.
    float winding = 0.0f;
    for (uint8_t index = 0; index < value.corner_count; ++index) {
        const size_t previous = (index + value.corner_count - 1) % value.corner_count;
        const size_t next     = (index + 1) % value.corner_count;
        const Vec2 incoming   = subtract(value.corner_positions[index], value.corner_positions[previous]);
        const Vec2 outgoing   = subtract(value.corner_positions[next], value.corner_positions[index]);
        if (length(incoming) < kMinimumEdgeLength || length(outgoing) < kMinimumEdgeLength) {
            return false;
        }
        const float turn = cross(incoming, outgoing);
        if (std::fabs(turn) < kMinimumEdgeLength) {
            return false;
        }
        if (winding == 0.0f) {
            winding = turn;
        } else if (winding * turn <= 0.0f) {
            return false;
        }
    }
    return true;
}

RoundedPolygonParameters clampParameters(const RoundedPolygonParameters& source, bool& was_clamped)
{
    RoundedPolygonParameters result = source;
    const auto clampParameter       = [&](float& value, float low, float high) {
        const float original = value;
        value                = std::clamp(value, low, high);
        was_clamped          = was_clamped || value != original;
    };
    for (uint8_t index = 0; index < result.corner_count; ++index) {
        clampParameter(result.corners[index].roundness, kMinCornerRoundness, kMaxCornerRoundness);
        clampParameter(result.corners[index].cap_tension, kMinCapTension, kMaxCapTension);
        clampParameter(result.edges[index].bulge, -kMaxEdgeBulge, kMaxEdgeBulge);
        clampParameter(result.edges[index].start, kMinEdgeHandle, kMaxEdgeHandle);
        clampParameter(result.edges[index].end, kMinEdgeHandle, kMaxEdgeHandle);
    }
    return result;
}

bool finite(const RoundedPolygonSample& sample)
{
    if (!finite(sample.path.bounds.minimum) || !finite(sample.path.bounds.maximum)) {
        return false;
    }
    for (uint8_t index = 0; index < sample.path.command_count; ++index) {
        const PathCommand& command = sample.path.commands[index];
        if (!finite(command.first) || !finite(command.second) || !finite(command.end)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool sampleRoundedPolygon(const RoundedPolygonParameters& source, const Affine2D& world, RoundedPolygonSample& sample,
                          PathStyle style)
{
    sample = {};
    if (source.corner_count < 3 || source.corner_count > kMaxRoundedPolygonCorners) {
        sample.error = RoundedPolygonSampleError::InvalidCornerCount;
        return false;
    }
    for (uint8_t index = 0; index < source.corner_count; ++index) {
        if (!finite(source.corner_positions[index]) || !finite(source.corners[index]) || !finite(source.edges[index])) {
            sample.error = RoundedPolygonSampleError::NonFiniteParameters;
            return false;
        }
    }
    if (!finite(world)) {
        sample.error = RoundedPolygonSampleError::NonFiniteTransform;
        return false;
    }
    if (!finite(style)) {
        sample.error = RoundedPolygonSampleError::NonFiniteStyle;
        return false;
    }
    if (!validTopology(source)) {
        sample.error = RoundedPolygonSampleError::DegenerateOrNonConvex;
        return false;
    }

    RoundedPolygonSample result;
    result.parameters              = clampParameters(source, result.parameters_were_clamped);
    result.path.style              = style;
    result.path.style.opacity      = std::clamp(result.path.style.opacity, 0.0f, 1.0f);
    result.path.style.stroke_width = std::max(0.0f, result.path.style.stroke_width);
    const uint8_t count            = result.parameters.corner_count;

    std::array<Vec2, kMaxRoundedPolygonCorners> incoming{};
    std::array<Vec2, kMaxRoundedPolygonCorners> outgoing{};
    std::array<float, kMaxRoundedPolygonCorners> corner_distances{};
    for (uint8_t index = 0; index < count; ++index) {
        const size_t previous   = (index + count - 1) % count;
        const size_t next       = (index + 1) % count;
        const Vec2 vertex       = result.parameters.corner_positions[index];
        corner_distances[index] = std::min(length(subtract(result.parameters.corner_positions[previous], vertex)),
                                           length(subtract(result.parameters.corner_positions[next], vertex))) *
                                  result.parameters.corners[index].roundness;
        incoming[index] = add(vertex, scale(normalized(subtract(result.parameters.corner_positions[previous], vertex)),
                                            corner_distances[index]));
        outgoing[index] = add(vertex, scale(normalized(subtract(result.parameters.corner_positions[next], vertex)),
                                            corner_distances[index]));
    }

    struct EdgeWork {
        Vec2 start{};
        Vec2 end{};
        Vec2 start_tangent{};
        Vec2 end_tangent{};
        float length = 0.0f;
    };
    std::array<EdgeWork, kMaxRoundedPolygonCorners> edge_work{};
    for (uint8_t index = 0; index < count; ++index) {
        const size_t next = (index + 1) % count;
        EdgeWork& edge    = edge_work[index];
        edge.start        = outgoing[index];
        edge.end          = incoming[next];
        edge.length       = length(subtract(edge.end, edge.start));
        if (edge.length < kMinimumEdgeLength) {
            sample.error = RoundedPolygonSampleError::DegenerateOrNonConvex;
            return false;
        }
        const Vec2 direction = normalized(subtract(edge.end, edge.start));
        const Vec2 outward{direction.y, -direction.x};
        const float bulge  = result.parameters.edges[index].bulge;
        edge.start_tangent = normalized(add(direction, scale(outward, bulge)));
        edge.end_tangent   = normalized(subtract(direction, scale(outward, bulge)));
    }

    for (uint8_t corner = 0; corner < count; ++corner) {
        const size_t previous_edge = (corner + count - 1) % count;
        const float handle_length  = corner_distances[corner] * result.parameters.corners[corner].cap_tension;
        result.corners[corner]     = {
            incoming[corner],
            add(incoming[corner], scale(edge_work[previous_edge].end_tangent, handle_length)),
            subtract(outgoing[corner], scale(edge_work[corner].start_tangent, handle_length)),
            outgoing[corner],
        };
    }

    for (uint8_t index = 0; index < count; ++index) {
        const size_t next_corner     = (index + 1) % count;
        const auto& previous_corner  = result.corners[index];
        const auto& following_corner = result.corners[next_corner];
        const auto& edge             = edge_work[index];
        const Vec2 start_tangent     = normalized(subtract(previous_corner.end, previous_corner.second_control));
        const Vec2 end_tangent       = normalized(subtract(following_corner.first_control, following_corner.start));
        result.edges[index]          = {
            edge.start,
            add(edge.start, scale(start_tangent, result.parameters.edges[index].start * edge.length)),
            subtract(edge.end, scale(end_tangent, result.parameters.edges[index].end * edge.length)),
            edge.end,
        };
    }

    appendMove(result.path, world, result.edges[0].start);
    for (uint8_t index = 0; index < count; ++index) {
        const size_t next_corner = (index + 1) % count;
        const auto& edge         = result.edges[index];
        const auto& corner       = result.corners[next_corner];
        appendCubic(result.path, world, edge.first_control, edge.second_control, edge.end);
        appendCubic(result.path, world, corner.first_control, corner.second_control, corner.end);
    }
    append(result.path, {PathCommandType::Close, {}, {}, {}});

    for (uint8_t index = 0; index < count; ++index) {
        result.landmarks[index] = transformPoint(world, result.parameters.corner_positions[index]);
    }
    if (!finite(result)) {
        sample.error = RoundedPolygonSampleError::NonFiniteGeometry;
        return false;
    }
    sample = result;
    return true;
}

}  // namespace pet::shape
