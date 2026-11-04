/* SPDX-License-Identifier: MIT */
#include "sdf_face_renderer.h"

#include <algorithm>
#include <cmath>

namespace pet::sdf {
namespace {

using shape::Affine2D;
using shape::Transform2D;
using shape::Vec2;

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

float length(Vec2 value)
{
    return std::sqrt(dot(value, value));
}

float sdEllipse(Vec2 point, Vec2 radii)
{
    if (radii.x <= 0.0f || radii.y <= 0.0f) return 1e9f;
    const float x = point.x / radii.x;
    const float y = point.y / radii.y;
    return (std::sqrt(x * x + y * y) - 1.0f) * std::min(radii.x, radii.y);
}

float sdRoundedBox(Vec2 point, Vec2 half, float radius)
{
    const float qx = std::fabs(point.x) - half.x + radius;
    const float qy = std::fabs(point.y) - half.y + radius;
    const float outside = length({std::max(qx, 0.0f), std::max(qy, 0.0f)});
    return outside + std::min(std::max(qx, qy), 0.0f) - radius;
}

float sdTriangle(Vec2 point, Vec2 a, Vec2 b, Vec2 c)
{
    const Vec2 e0 = {b.x - a.x, b.y - a.y};
    const Vec2 e1 = {c.x - b.x, c.y - b.y};
    const Vec2 e2 = {a.x - c.x, a.y - c.y};
    const Vec2 v0 = {point.x - a.x, point.y - a.y};
    const Vec2 v1 = {point.x - b.x, point.y - b.y};
    const Vec2 v2 = {point.x - c.x, point.y - c.y};
    const auto closest = [](Vec2 value, Vec2 edge) {
        const float denominator = dot(edge, edge);
        const float t = denominator > 1e-12f ? std::clamp(dot(value, edge) / denominator, 0.0f, 1.0f) : 0.0f;
        return Vec2{value.x - edge.x * t, value.y - edge.y * t};
    };
    const Vec2 q0 = closest(v0, e0);
    const Vec2 q1 = closest(v1, e1);
    const Vec2 q2 = closest(v2, e2);
    const float sign = (e0.x * e2.y - e0.y * e2.x) >= 0.0f ? 1.0f : -1.0f;
    const float distance2 = std::min({dot(q0, q0), dot(q1, q1), dot(q2, q2)});
    const float winding = std::min({sign * (v0.x * e0.y - v0.y * e0.x),
                                    sign * (v1.x * e1.y - v1.y * e1.x),
                                    sign * (v2.x * e2.y - v2.y * e2.x)});
    return -std::sqrt(distance2) * (winding >= 0.0f ? 1.0f : -1.0f);
}

float sdCapsule(Vec2 point, Vec2 a, Vec2 b, float radius)
{
    const Vec2 pa = {point.x - a.x, point.y - a.y};
    const Vec2 ba = {b.x - a.x, b.y - a.y};
    const float denominator = dot(ba, ba);
    const float h = denominator > 1e-12f ? std::clamp(dot(pa, ba) / denominator, 0.0f, 1.0f) : 0.0f;
    return length({pa.x - ba.x * h, pa.y - ba.y * h}) - radius;
}

Affine2D rigAffine(const PrimitiveRig& rig)
{
    Transform2D transform{};
    transform.translation = {rig.anchor.x + rig.offset.x, rig.anchor.y + rig.offset.y};
    transform.rotation_degrees = rig.rotation_degrees;
    transform.scale            = rig.scale;
    transform.pivot            = rig.anchor;
    return shape::makeAffine(transform);
}

float primitiveDistance(const Primitive& primitive, Vec2 point)
{
    Affine2D inverse{};
    if (!shape::invert(rigAffine(primitive.rig), inverse)) return 1e9f;
    point = shape::transformPoint(inverse, point);
    switch (primitive.kind) {
        case PrimitiveKind::Ellipse:
            return sdEllipse({point.x - primitive.center.x, point.y - primitive.center.y}, primitive.half);
        case PrimitiveKind::RoundedBox:
            return sdRoundedBox({point.x - primitive.center.x, point.y - primitive.center.y}, primitive.half,
                                primitive.radius);
        case PrimitiveKind::Triangle:
            return sdTriangle(point, primitive.center, primitive.b, primitive.c) - primitive.radius;
        case PrimitiveKind::Capsule:
            return sdCapsule(point, primitive.center, primitive.b, primitive.radius);
        case PrimitiveKind::HalfPlane:
            return dot(primitive.half, {point.x - primitive.center.x, point.y - primitive.center.y});
    }
    return 1e9f;
}

float smoothMinimum(float a, float b, float blend)
{
    if (blend <= 0.0f) return std::min(a, b);
    const float h = std::max(blend - std::fabs(a - b), 0.0f) / blend;
    return std::min(a, b) - h * h * blend * 0.25f;
}

float combine(float accumulated, float term, Combine operation, float blend)
{
    switch (operation) {
        case Combine::Union:
            return std::min(accumulated, term);
        case Combine::SmoothUnion:
            return smoothMinimum(accumulated, term, blend);
        case Combine::Subtract:
            return std::max(accumulated, -term);
        case Combine::Intersect:
            return std::max(accumulated, term);
    }
    return term;
}

float fieldDistance(const Node& node, Vec2 local_point)
{
    if (node.primitive_count == 0 || node.primitive_count > kMaxPrimitives) return 1e9f;
    float distance = primitiveDistance(node.primitives[0], local_point);
    for (uint8_t index = 1; index < node.primitive_count; ++index) {
        const Primitive& primitive = node.primitives[index];
        distance = combine(distance, primitiveDistance(primitive, local_point), primitive.combine, primitive.blend);
    }
    return distance;
}

uint8_t blendChannel(uint8_t source, uint8_t destination, uint8_t alpha)
{
    return static_cast<uint8_t>((uint16_t(source) * alpha + uint16_t(destination) * (255u - alpha) + 127u) / 255u);
}

}  // namespace

float nodeDistance(const Scene& scene, const std::array<Affine2D, kMaxNodes>& worlds,
                   const std::array<Affine2D, kMaxNodes>& inverse_worlds, uint8_t node_index, Vec2 world_point)
{
    if (node_index >= scene.node_count) return 1e9f;
    const Node& node = scene.nodes[node_index];
    float distance = fieldDistance(node, shape::transformPoint(inverse_worlds[node_index], world_point));
    if (node.clip_index >= 0 && node.clip_index < static_cast<int8_t>(node_index))
        distance = std::max(distance,
                            nodeDistance(scene, worlds, inverse_worlds, static_cast<uint8_t>(node.clip_index), world_point) +
                                node.clip_inset);
    return distance;
}

bool render(const Scene& scene, Argb8888Target target)
{
    if (target.pixels == nullptr || target.width == 0 || target.height == 0 || target.stride_pixels < target.width ||
        scene.node_count == 0 || scene.node_count > kMaxNodes)
        return false;

    std::array<Affine2D, kMaxNodes> worlds{};
    std::array<Affine2D, kMaxNodes> inverse_worlds{};
    for (uint8_t index = 0; index < scene.node_count; ++index) {
        const Node& node = scene.nodes[index];
        if (node.parent_index < -1 || node.parent_index >= static_cast<int8_t>(index)) return false;
        const Affine2D local = shape::makeAffine(node.transform);
        worlds[index] = node.parent_index < 0 ? local : shape::compose(worlds[uint8_t(node.parent_index)], local);
        if (!shape::invert(worlds[index], inverse_worlds[index])) return false;
        if (node.clip_index < -1 || node.clip_index >= static_cast<int8_t>(index)) return false;
        if (node.primitive_count > kMaxPrimitives) return false;
    }

    const uint32_t background = 0xff000000u | (uint32_t(scene.background.red) << 16u) |
                                (uint32_t(scene.background.green) << 8u) | uint32_t(scene.background.blue);
    for (uint16_t y = 0; y < target.height; ++y)
        for (uint16_t x = 0; x < target.width; ++x) target.pixels[size_t(y) * target.stride_pixels + x] = background;

    for (uint8_t index = 0; index < scene.node_count; ++index) {
        const Node& node = scene.nodes[index];
        if (!node.paint.visible || node.paint.opacity == 0 || node.primitive_count == 0) continue;
        for (uint16_t y = 0; y < target.height; ++y) {
            for (uint16_t x = 0; x < target.width; ++x) {
                const float distance = nodeDistance(scene, worlds, inverse_worlds, index,
                                                    {float(x) + 0.5f, float(y) + 0.5f});
                const float coverage = std::clamp(0.5f - distance, 0.0f, 1.0f);
                const uint8_t alpha = static_cast<uint8_t>(coverage * float(node.paint.opacity) + 0.5f);
                if (alpha == 0) continue;
                uint32_t& destination = target.pixels[size_t(y) * target.stride_pixels + x];
                const uint8_t red   = static_cast<uint8_t>((destination >> 16u) & 0xffu);
                const uint8_t green = static_cast<uint8_t>((destination >> 8u) & 0xffu);
                const uint8_t blue  = static_cast<uint8_t>(destination & 0xffu);
                destination = 0xff000000u | (uint32_t(blendChannel(node.paint.color.red, red, alpha)) << 16u) |
                              (uint32_t(blendChannel(node.paint.color.green, green, alpha)) << 8u) |
                              uint32_t(blendChannel(node.paint.color.blue, blue, alpha));
            }
        }
    }
    return true;
}

}  // namespace pet::sdf
