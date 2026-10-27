/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pet::shape {

/** A point in normalized template-local or final design-pixel coordinates. */
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Bounds2D {
    Vec2 minimum{};
    Vec2 maximum{};
    bool valid = false;
};

/**
 * A 2D affine transform stored in its direct, point-transform form:
 *
 * x' = m00*x + m01*y + tx
 * y' = m10*x + m11*y + ty
 *
 * The representation is intentionally renderer-neutral and makes parent/local
 * composition a fixed 12-multiply operation.
 */
struct Affine2D {
    float m00 = 1.0f;
    float m01 = 0.0f;
    float tx = 0.0f;
    float m10 = 0.0f;
    float m11 = 1.0f;
    float ty = 0.0f;
};

/**
 * Author-facing pose. `translation` is the world position of `pivot` after
 * scale and rotation; `pivot` remains in normalized template-local space.
 */
struct Transform2D {
    Vec2 translation{};
    float rotation_degrees = 0.0f;
    Vec2 scale{1.0f, 1.0f};
    Vec2 pivot{};
};

constexpr Vec2 transformPoint(const Affine2D& transform, Vec2 point)
{
    return {
        transform.m00 * point.x + transform.m01 * point.y + transform.tx,
        transform.m10 * point.x + transform.m11 * point.y + transform.ty,
    };
}

/** Returns parent * local: apply local first, then parent. */
constexpr Affine2D compose(const Affine2D& parent, const Affine2D& local)
{
    return {
        parent.m00 * local.m00 + parent.m01 * local.m10,
        parent.m00 * local.m01 + parent.m01 * local.m11,
        parent.m00 * local.tx + parent.m01 * local.ty + parent.tx,
        parent.m10 * local.m00 + parent.m11 * local.m10,
        parent.m10 * local.m01 + parent.m11 * local.m11,
        parent.m10 * local.tx + parent.m11 * local.ty + parent.ty,
    };
}

Affine2D makeAffine(const Transform2D& transform);

/** Returns false for a singular transform (for example, zero scale). */
bool invert(const Affine2D& transform, Affine2D& inverse);

void include(Bounds2D& bounds, Vec2 point);

struct Color {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
};

struct PathStyle {
    Color color{248, 245, 237};
    float opacity = 1.0f;
    float stroke_width = 0.0f;
    bool fill = true;
    bool stroke = false;
};

enum class PathCommandType : uint8_t {
    Move,
    Cubic,
    Close,
};

struct PathCommand {
    PathCommandType type = PathCommandType::Move;
    Vec2 first{};
    Vec2 second{};
    Vec2 end{};
};

constexpr size_t kMaxPathCommands = 12;

/**
 * Fixed-capacity, closed cubic Bézier output. It is sampled geometry rather
 * than authored state: templates retain semantic landmarks and parameters.
 */
struct ShapePath {
    PathStyle style{};
    std::array<PathCommand, kMaxPathCommands> commands{};
    uint8_t command_count = 0;
    Bounds2D bounds{};
};

}  // namespace pet::shape
