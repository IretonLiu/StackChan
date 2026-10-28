/* SPDX-License-Identifier: MIT */
#pragma once

// Bounded, editable animation model shared by persistence, validation and the editor.
//
// This is the authoring shape of the .petface.json `poses` / `animations` blocks. It is
// deliberately free of Node/SceneDoc so types.h can embed it without a cycle. Node and
// channel references are stored by id; resolvers live in animations.h.

#include <pet/preset/face_motion.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace kiwi::studio {

inline constexpr size_t kMaxAnimationGoals    = 8;
inline constexpr size_t kMaxAnimationPrograms = 8;
inline constexpr size_t kMaxAnimationTracks   = 16;
inline constexpr size_t kMaxPoseTargets       = 32;
inline constexpr size_t kMaxPoses             = 8;

struct AnimationTrack {
    std::string node_id;
    std::string channel;  // schema channel id, e.g. "transform.scale_y"
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
    // Optional spring override. Bit 1 = frequency, 2 = damping, 4 = max step.
    uint8_t response_fields             = 0;
    float response_frequency_hz         = 6.0f;
    float response_damping_ratio        = 1.0f;
    float response_maximum_step_seconds = 1.0f / 120.0f;
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
    uint8_t event_mask   = 0;  // 1 = pet, 2 = shake
    std::array<AnimationProgram, kMaxAnimationPrograms> programs{};
    uint8_t program_count = 0;
};

struct PoseTarget {
    std::string node_id;
    std::string channel;
    uint8_t element = 0;
    float value     = 0.0f;
    // Optional spring override. Bit 1 = frequency, 2 = damping, 4 = max step.
    uint8_t response_fields             = 0;
    float response_frequency_hz         = 6.0f;
    float response_damping_ratio        = 1.0f;
    float response_maximum_step_seconds = 1.0f / 120.0f;
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
        return goal_count == 0 && pose_count == 0;
    }
    int findGoal(const std::string& id) const;
    int findPose(const std::string& id) const;
};

}  // namespace kiwi::studio
