/* SPDX-License-Identifier: MIT */
#pragma once

// Read-only view of the compiler-owned `poses` and `animations` blocks.
//
// Kiwi Studio does not rewrite these blocks yet: the document keeps the authored JSON
// verbatim (`SceneDoc::poses_json` / `animations_json`) and this module parses it into a
// bounded model so the editor can show what is loaded and preview it on the canvas.
// When the timeline editor lands this becomes the editable model.

#include "types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace kiwi::studio {

inline constexpr size_t kMaxAnimationGoals      = 8;
inline constexpr size_t kMaxAnimationPrograms   = 8;
inline constexpr size_t kMaxAnimationTracks     = 16;
inline constexpr size_t kMaxPoseTargets         = 32;
inline constexpr size_t kMaxPoses               = 8;
inline constexpr size_t kMaxAnimationCandidates = kMaxAnimationPrograms * kMaxAnimationTracks;

struct AnimationTrack {
    std::string node_id;
    int node_index = -1;
    std::string channel;  // raw channel id, shown verbatim
    MotionProperty property          = MotionProperty::Rotation;
    bool mappable                    = false;  // channel resolves to a studio field on this node
    uint8_t element                  = 0;
    uint8_t priority                 = 0;
    bool sine                        = false;
    pet::preset::MotionEasing easing = pet::preset::MotionEasing::Linear;
    std::array<pet::preset::MotionKeyframe, pet::preset::kMaxMotionKeyframes> keyframes{};
    uint8_t keyframe_count = 0;
    float center           = 0.0f;
    float amplitude        = 0.0f;
    float period_seconds   = 1.0f;
    float phase_radians    = 0.0f;
};

struct AnimationProgram {
    std::string id;
    float duration_seconds = 1.0f;
    bool loop              = false;
    uint8_t priority       = 0;
    std::array<AnimationTrack, kMaxAnimationTracks> tracks{};
    uint8_t track_count = 0;
};

struct AnimationGoal {
    std::string id;
    std::string pose_id;
    uint8_t priority     = 0;
    uint32_t duration_ms = 0;
    uint8_t event_mask   = 0;
    std::array<AnimationProgram, kMaxAnimationPrograms> programs{};
    uint8_t program_count = 0;
};

struct PoseTarget {
    std::string node_id;
    int node_index = -1;
    std::string channel;
    MotionProperty property = MotionProperty::Rotation;
    bool mappable           = false;
    uint8_t element         = 0;
    float value             = 0.0f;
};

struct Pose {
    std::string id;
    std::array<PoseTarget, kMaxPoseTargets> targets{};
    uint8_t target_count = 0;
};

struct AnimationDoc {
    std::string default_goal;
    std::array<AnimationGoal, kMaxAnimationGoals> goals{};
    uint8_t goal_count = 0;
    std::array<Pose, kMaxPoses> poses{};
    uint8_t pose_count = 0;

    bool empty() const
    {
        return goal_count == 0;
    }
    int findGoal(const std::string& id) const;
    const Pose* findPose(const std::string& id) const;
};

/**
 * Parses the document's opaque pose/animation strings into a bounded view. Returns
 * false and fills `error` on malformed data; `out` is left empty on failure.
 */
bool parseAnimations(const std::string& poses_json, const std::string& animations_json, const SceneDoc& document,
                     AnimationDoc& out, std::string& error);

/**
 * Applies the selected goal at `time_seconds` onto `nodes`, layering the goal's pose as a
 * baseline and then the highest-priority track per channel. Returns true when a field
 * changed. `nodes` is a scratch copy; the document is never mutated.
 */
bool applyAnimationFrame(const AnimationDoc& animations, int goal_index, float time_seconds,
                         std::array<Node, kMaxNodes>& nodes, size_t node_count);

/** Human-readable event mask, e.g. "pet, shake"; "none" when empty. */
std::string animationEventLabel(uint8_t event_mask);

}  // namespace kiwi::studio
