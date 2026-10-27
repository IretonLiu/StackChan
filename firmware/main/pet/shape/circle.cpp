/*
 * SPDX-License-Identifier: MIT
 */
#include "circle.h"

#include <cmath>

namespace pet::shape {
namespace {

constexpr float kCircleControl = 0.5522847498307936f;

bool finite(Vec2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
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

void append(ShapePath& path, PathCommand command)
{
    path.commands[path.command_count++] = command;
}

void appendMove(ShapePath& path, const Affine2D& world, Vec2 point)
{
    const Vec2 transformed = transformPoint(world, point);
    append(path, {PathCommandType::Move, transformed, {}, {}});
    include(path.bounds, transformed);
}

void appendCubic(ShapePath& path, const Affine2D& world, Vec2 first, Vec2 second, Vec2 end)
{
    const Vec2 transformed_first = transformPoint(world, first);
    const Vec2 transformed_second = transformPoint(world, second);
    const Vec2 transformed_end = transformPoint(world, end);
    append(path, {PathCommandType::Cubic, transformed_first, transformed_second, transformed_end});
    include(path.bounds, transformed_first);
    include(path.bounds, transformed_second);
    include(path.bounds, transformed_end);
}

bool finite(const CircleSample& sample)
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

bool sampleCircle(const CircleParameters& parameters, const Affine2D& world, CircleSample& sample, PathStyle style)
{
    sample = {};
    if (!finite(parameters.center) || !std::isfinite(parameters.radius)) {
        sample.error = CircleSampleError::NonFiniteParameters;
        return false;
    }
    if (parameters.radius <= 0.0f) {
        sample.error = CircleSampleError::InvalidRadius;
        return false;
    }
    if (!finite(world)) {
        sample.error = CircleSampleError::NonFiniteTransform;
        return false;
    }
    if (!finite(style)) {
        sample.error = CircleSampleError::NonFiniteStyle;
        return false;
    }

    CircleSample result;
    result.parameters = parameters;
    result.path.style = style;
    result.path.style.opacity = std::fmin(1.0f, std::fmax(0.0f, result.path.style.opacity));
    result.path.style.stroke_width = std::fmax(0.0f, result.path.style.stroke_width);
    const float r = parameters.radius;
    const float c = r * kCircleControl;
    const Vec2 center = parameters.center;
    appendMove(result.path, world, {center.x + r, center.y});
    appendCubic(result.path, world, {center.x + r, center.y + c}, {center.x + c, center.y + r}, {center.x, center.y + r});
    appendCubic(result.path, world, {center.x - c, center.y + r}, {center.x - r, center.y + c}, {center.x - r, center.y});
    appendCubic(result.path, world, {center.x - r, center.y - c}, {center.x - c, center.y - r}, {center.x, center.y - r});
    appendCubic(result.path, world, {center.x + c, center.y - r}, {center.x + r, center.y - c}, {center.x + r, center.y});
    append(result.path, {PathCommandType::Close, {}, {}, {}});

    if (!finite(result)) {
        sample.error = CircleSampleError::NonFiniteGeometry;
        return false;
    }
    sample = result;
    return true;
}

}  // namespace pet::shape
