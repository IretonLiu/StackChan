/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

namespace pet::shape {

/** Bounded rounding and tangent length for one polygon corner patch. */
struct CornerControls {
    float roundness = 0.08f;
    float cap_tension = 0.55f;
};

/** Bounded shared tangent-rail bend and derived edge-handle lengths. */
struct EdgeTension {
    float bulge = 0.0f;
    float start = 0.28f;
    float end = 0.28f;
};

}  // namespace pet::shape
