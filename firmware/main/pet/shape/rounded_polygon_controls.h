/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <pet/face_schema.generated.h>

namespace pet::shape {

// Authored parameter bounds. The generated schema is the single source of truth
// shared with the host compiler and the studio editor.

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
