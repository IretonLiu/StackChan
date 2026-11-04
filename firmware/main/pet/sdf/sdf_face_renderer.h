/* SPDX-License-Identifier: MIT */
#pragma once

// Standalone implicit-face renderer.
//
// This is intentionally separate from pet/preset and pet/face: authored scenes are
// compiled into these bounded C++ structures, then rasterised directly into an
// ARGB8888 target. No JSON parsing, path tessellation, or heap allocation occurs on
// the device.

#include <pet/shape/shape_path.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace pet::sdf {

inline constexpr uint8_t kMaxNodes      = 16;
inline constexpr uint8_t kMaxPrimitives = 8;

enum class PrimitiveKind : uint8_t { Ellipse, RoundedBox, Triangle, Capsule, HalfPlane };
enum class Combine : uint8_t { Union, SmoothUnion, Subtract, Intersect };

struct PrimitiveRig {
    // Rest pose is explicit and unambiguous: anchor is fixed field-space geometry;
    // offset/rotation/scale are the animatable local deltas.
    shape::Vec2 anchor{};
    shape::Vec2 offset{};
    float rotation_degrees = 0.0f;
    shape::Vec2 scale{1.0f, 1.0f};
};

struct Primitive {
    PrimitiveKind kind = PrimitiveKind::Ellipse;
    Combine combine    = Combine::Union;
    float blend         = 2.0f;
    // Ellipse: center/radii(half). RoundedBox: center/half/radius.
    // Triangle: center=A, b=B, c=C, radius=corner rounding.
    // Capsule: center=A, b=B, radius=tube radius.
    // HalfPlane: center=point on line, half=inside normal.
    shape::Vec2 center{};
    shape::Vec2 half{};
    shape::Vec2 b{};
    shape::Vec2 c{};
    float radius = 0.0f;
    PrimitiveRig rig{};
};

struct Paint {
    shape::Color color{};
    uint8_t opacity = 255;
    bool visible    = true;
};

struct Node {
    std::array<Primitive, kMaxPrimitives> primitives{};
    uint8_t primitive_count = 0;
    // Earlier node whose final field clips this node, or -1.
    int8_t clip_index = -1;
    float clip_inset  = 0.0f;
    int8_t parent_index = -1;
    shape::Transform2D transform{};
    Paint paint{};
};

struct Scene {
    shape::Color background{};
    std::array<Node, kMaxNodes> nodes{};
    uint8_t node_count = 0;
};

struct Argb8888Target {
    uint32_t* pixels      = nullptr;
    uint16_t width        = 0;
    uint16_t height       = 0;
    uint16_t stride_pixels = 0;
};

/** Evaluates a node's final field in world/design coordinates; negative is inside. */
float nodeDistance(const Scene& scene, const std::array<shape::Affine2D, kMaxNodes>& worlds,
                   const std::array<shape::Affine2D, kMaxNodes>& inverse_worlds, uint8_t node_index,
                   shape::Vec2 world_point);

/** Resolves parent transforms and directly composites the scene into `target`. */
[[nodiscard]] bool render(const Scene& scene, Argb8888Target target);

}  // namespace pet::sdf
