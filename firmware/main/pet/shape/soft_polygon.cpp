/*
 * SPDX-License-Identifier: MIT
 */
#include "soft_polygon.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace pet::shape {
namespace {

constexpr float kMinimumEdgeLength = 0.0001f;

float clamp(float value, float low, float high)
{
    return std::clamp(value, low, high);
}

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

bool finite(const SoftPolygonEarParameters& value)
{
    return std::isfinite(value.lean) && std::isfinite(value.tip_bend) && finite(value.tip_corner) &&
           finite(value.base_corners) && finite(value.left_side) && finite(value.right_side) && finite(value.base);
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
    if (value_length < kMinimumEdgeLength) {
        return {};
    }
    return scale(value, 1.0f / value_length);
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

SoftPolygonEarParameters clampParameters(const SoftPolygonEarParameters& source, bool& was_clamped)
{
    SoftPolygonEarParameters result = source;
    const auto clampParameter = [&](float& value, float low, float high) {
        const float original = value;
        value                = clamp(value, low, high);
        was_clamped          = was_clamped || value != original;
    };
    const auto clampCorner = [&](CornerControls& corner) {
        clampParameter(corner.roundness, 0.0f, 0.35f);
        clampParameter(corner.cap_tension, 0.10f, 0.90f);
    };
    const auto clampEdge = [&](EdgeTension& edge) {
        clampParameter(edge.bulge, -0.35f, 0.35f);
        clampParameter(edge.start, 0.08f, 0.45f);
        clampParameter(edge.end, 0.08f, 0.45f);
    };

    clampParameter(result.lean, -0.25f, 0.25f);
    clampParameter(result.tip_bend, -0.20f, 0.20f);
    clampCorner(result.tip_corner);
    clampCorner(result.base_corners);
    clampEdge(result.left_side);
    clampEdge(result.right_side);
    clampEdge(result.base);
    return result;
}

bool finite(const SoftPolygonEarSample& sample)
{
    if (!finite(sample.landmarks.base_left) || !finite(sample.landmarks.tip) || !finite(sample.landmarks.base_right) ||
        !finite(sample.path.bounds.minimum) || !finite(sample.path.bounds.maximum)) {
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

    SoftPolygonEarSample result;
    result.parameters = clampParameters(source, result.parameters_were_clamped);
    result.path.style = style;
    result.path.style.opacity = clamp(result.path.style.opacity, 0.0f, 1.0f);
    result.path.style.stroke_width = std::max(0.0f, result.path.style.stroke_width);

    const std::array<Vec2, 3> vertices = {
        Vec2{-0.5f, 0.0f},
        Vec2{result.parameters.lean + result.parameters.tip_bend * 0.5f, -1.0f},
        Vec2{0.5f, 0.0f},
    };
    const std::array<CornerControls, 3> corner_controls = {
        result.parameters.base_corners,
        result.parameters.tip_corner,
        result.parameters.base_corners,
    };
    const std::array<EdgeTension, 3> edge_tensions = {
        result.parameters.left_side,
        result.parameters.right_side,
        result.parameters.base,
    };

    std::array<Vec2, 3> incoming{};
    std::array<Vec2, 3> outgoing{};
    std::array<float, 3> corner_distances{};
    for (size_t index = 0; index < vertices.size(); ++index) {
        const size_t previous = (index + vertices.size() - 1) % vertices.size();
        const size_t next     = (index + 1) % vertices.size();
        corner_distances[index] = std::min(length(subtract(vertices[previous], vertices[index])),
                                           length(subtract(vertices[next], vertices[index]))) *
                                  corner_controls[index].roundness;
        incoming[index] = add(vertices[index],
                              scale(normalized(subtract(vertices[previous], vertices[index])), corner_distances[index]));
        outgoing[index] = add(vertices[index],
                              scale(normalized(subtract(vertices[next], vertices[index])), corner_distances[index]));
    }

    struct EdgeWork {
        Vec2 start{};
        Vec2 end{};
        Vec2 direction{};
        Vec2 outward{};
        Vec2 start_tangent{};
        Vec2 end_tangent{};
        float length = 0.0f;
    };
    std::array<EdgeWork, 3> edges{};
    for (size_t index = 0; index < edges.size(); ++index) {
        const size_t next = (index + 1) % edges.size();
        EdgeWork& edge    = edges[index];
        edge.start         = outgoing[index];
        edge.end           = incoming[next];
        edge.length        = length(subtract(edge.end, edge.start));
        edge.direction     = normalized(subtract(edge.end, edge.start));
        edge.outward       = {edge.direction.y, -edge.direction.x};
        // The two rails bend in opposite forward directions. Both the edge
        // and its adjoining corner consume these exact vectors.
        edge.start_tangent = normalized(add(edge.direction, scale(edge.outward, edge_tensions[index].bulge)));
        edge.end_tangent   = normalized(subtract(edge.direction, scale(edge.outward, edge_tensions[index].bulge)));
    }

    // Corner controls are the tangent authority. Every adjacent edge handle
    // is derived from these rays below, never the other way around.
    for (size_t corner = 0; corner < result.corners.size(); ++corner) {
        const size_t previous_edge = (corner + edges.size() - 1) % edges.size();
        const size_t next_edge     = corner;
        const float handle_length = corner_distances[corner] * corner_controls[corner].cap_tension;
        result.corners[corner] = {
            incoming[corner],
            add(incoming[corner], scale(edges[previous_edge].end_tangent, handle_length)),
            subtract(outgoing[corner], scale(edges[next_edge].start_tangent, handle_length)),
            outgoing[corner],
        };
    }

    for (size_t index = 0; index < result.edges.size(); ++index) {
        const size_t next_corner = (index + 1) % result.corners.size();
        const EarCornerGeometry& previous_corner = result.corners[index];
        const EarCornerGeometry& following_corner = result.corners[next_corner];
        const EdgeWork& edge = edges[index];
        const Vec2 start_tangent = normalized(subtract(previous_corner.end, previous_corner.second_control));
        const Vec2 end_tangent = normalized(subtract(following_corner.first_control, following_corner.start));
        result.edges[index] = {
            edge.start,
            add(edge.start, scale(start_tangent, edge_tensions[index].start * edge.length)),
            subtract(edge.end, scale(end_tangent, edge_tensions[index].end * edge.length)),
            edge.end,
        };
    }

    appendMove(result.path, world, result.edges[0].start);
    for (size_t index = 0; index < result.edges.size(); ++index) {
        const size_t next_corner = (index + 1) % result.corners.size();
        const EarEdgeGeometry& edge = result.edges[index];
        const EarCornerGeometry& corner = result.corners[next_corner];
        appendCubic(result.path, world, edge.first_control, edge.second_control, edge.end);
        appendCubic(result.path, world, corner.first_control, corner.second_control, corner.end);
    }
    append(result.path, {PathCommandType::Close, {}, {}, {}});

    result.landmarks = {
        transformPoint(world, vertices[0]),
        transformPoint(world, vertices[1]),
        transformPoint(world, vertices[2]),
    };
    if (!finite(result)) {
        sample.error = SoftPolygonEarSampleError::NonFiniteGeometry;
        return false;
    }

    sample = result;
    return true;
}

}  // namespace pet::shape
