/* SPDX-License-Identifier: MIT */
#pragma once

// Renderer-neutral shape queries shared by the canvas, SVG export and validation.

#include "types.h"

#include <pet/shape/shape_path.h>

#include <array>

namespace kiwi::studio {

/** Signed turn at `b` along a -> b -> c; near zero means collinear. */
inline float cross(pet::shape::Vec2 a, pet::shape::Vec2 b, pet::shape::Vec2 c)
{
    return (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
}

/** Samples a node into a drawable path in world space. */
bool sampleNode(const Node& node, const pet::shape::Affine2D& world, pet::shape::ShapePath& path);

/** Resolves parent-relative transforms. Returns false on invalid parent ordering. */
bool resolveWorlds(const SceneDoc& document, std::array<pet::shape::Affine2D, kMaxNodes>& worlds);

/** Human-readable reason the node cannot be sampled, or nullptr when it is valid. */
const char* shapeProblem(const Node& node, const pet::shape::Affine2D& world);

}  // namespace kiwi::studio
