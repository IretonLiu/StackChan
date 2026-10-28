/* SPDX-License-Identifier: MIT */
#pragma once

// Single source of truth for the animatable per-node motion channels.
//
// Adding a channel means adding one row to motionDescriptors(); the resolver, the
// inspector combo, the tooltip and the JSON round trip all derive from that row.

#include "types.h"

#include <pet/preset/face_motion.h>

#include <cstddef>

namespace kiwi::studio {

struct MotionDescriptor {
    MotionProperty property;
    const char* label;
    pet::preset::MotionRange range;
    bool needs_circle;
    bool needs_polygon;
};

/** Ordered by MotionProperty value; motionDescriptorCount() entries. */
const MotionDescriptor* motionDescriptors();
size_t motionDescriptorCount();

/** Descriptor for `property`, or nullptr when out of range. */
const MotionDescriptor* findMotionDescriptor(MotionProperty property);

/** Safe clamp range for `property`; zero range when unknown. */
pet::preset::MotionRange motionRange(MotionProperty property);

/** User-facing label for `property`; "unknown" when out of range. */
const char* motionPropertyLabel(MotionProperty property);

/** True when `property` can drive a node of `kind`. */
bool motionPropertyApplies(MotionProperty property, Kind kind);

/** True when `index` names a valid MotionProperty (for JSON intake). */
bool motionPropertyValid(int index);

/**
 * Returns the addressable field for `property`, or nullptr when the property does not
 * apply to this node. `range` receives the channel's safe clamp range.
 */
float* resolveMotionTarget(Node& node, MotionProperty property, pet::preset::MotionRange& range);

/**
 * Applies one frame of the node's own motion settings. Returns true when the value
 * changed. Does not consider the editor's playback clock; callers supply elapsed_ms.
 */
bool applyMotion(Node& node, const MotionPreview& motion, uint32_t elapsed_ms);

}  // namespace kiwi::studio
