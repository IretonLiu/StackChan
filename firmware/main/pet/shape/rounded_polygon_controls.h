/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

namespace pet::shape {

// Authored parameter bounds. The engine clamp, the host preset compiler and the
// studio editor all share these values so an out-of-range drag cannot produce a
// document the firmware would clamp differently from what the editor displayed.
inline constexpr float kMinCornerRoundness = 0.0f;
inline constexpr float kMaxCornerRoundness = 0.35f;
inline constexpr float kMinCapTension      = 0.10f;
inline constexpr float kMaxCapTension      = 0.90f;
inline constexpr float kMaxEdgeBulge       = 0.35f;
inline constexpr float kMinEdgeHandle      = 0.08f;
inline constexpr float kMaxEdgeHandle      = 0.45f;

// Neutral authoring defaults used by "reset to default" affordances.
inline constexpr float kDefaultCornerRoundness = 0.08f;
inline constexpr float kDefaultCapTension      = 0.55f;
inline constexpr float kDefaultEdgeBulge       = 0.0f;
inline constexpr float kDefaultEdgeHandle      = 0.28f;

/** Bounded rounding and tangent length for one polygon corner patch. */
struct CornerControls {
    float roundness   = kDefaultCornerRoundness;
    float cap_tension = kDefaultCapTension;
};

/** Bounded shared tangent-rail bend and derived edge-handle lengths. */
struct EdgeTension {
    float bulge = kDefaultEdgeBulge;
    float start = kDefaultEdgeHandle;
    float end   = kDefaultEdgeHandle;
};

}  // namespace pet::shape
