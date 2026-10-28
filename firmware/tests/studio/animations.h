/* SPDX-License-Identifier: MIT */
#pragma once

// Animation model operations shared by validation, the canvas preview and the authoring
// UI. JSON intake/emission lives in animation_io.h so this header stays free of
// ArduinoJson and can be included by UI-free modules.

#include "animation_model.h"
#include "channels.h"
#include "types.h"

#include <array>
#include <cstddef>
#include <string>

namespace kiwi::studio {

inline constexpr size_t kMaxAnimationCandidates = kMaxAnimationPrograms * kMaxAnimationTracks;

/** Node index for `id`, or -1. */
int findNodeIndex(const SceneDoc& document, const std::string& id);

/** Current scalar value of `channel`/`element` on `node`; 0 when not addressable. */
float channelValue(const Node& node, const ChannelInfo& channel, uint8_t element);

/** Writes a scalar channel value, clamped to the channel range. Returns false if not addressable. */
bool setChannelValue(Node& node, const ChannelInfo& channel, uint8_t element, float value);

/** Index of a matching target in `pose`, or -1. */
int findPoseTarget(const Pose& pose, const std::string& node_id, const char* channel, uint8_t element);

/**
 * Ensures `pose` declares `channel` for `node`, seeding it from the node's current value.
 * Returns false only when the pose is full or the channel is not addressable.
 */
bool ensurePoseTarget(Pose& pose, const Node& node, const ChannelInfo& channel, uint8_t element);

/** Compiler-valid empty scaffold: pose "idle" with no targets and goal "idle" using it. */
void makeDefaultAnimation(AnimationDoc& out);

/** Human-readable event mask, e.g. "pet, shake"; "none" when empty. */
std::string animationEventLabel(uint8_t event_mask);

/**
 * Applies the selected goal at `time_seconds` onto `nodes`, layering the goal's pose as a
 * baseline and then the highest-priority track per channel. Returns true when a field
 * changed. `nodes` is a scratch copy; the document is never mutated.
 */
bool applyAnimationFrame(const SceneDoc& document, int goal_index, float time_seconds,
                         std::array<Node, kMaxNodes>& nodes, size_t node_count);

/**
 * Checks every animation invariant the preset compiler enforces for `animations`. Fills
 * `error` with a "path: message" on failure. Empty animations are rejected because the
 * compiler requires at least one goal.
 */
bool validateAnimationDoc(const SceneDoc& document, const AnimationDoc& animations, std::string& error);

/** Convenience wrapper validating `document.animations`. */
bool validateAnimations(const SceneDoc& document, std::string& error);

}  // namespace kiwi::studio
