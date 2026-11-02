/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "face_motion.h"
#include "face_pose_engine.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace pet::preset {

enum class FaceMotionSource : uint8_t {
    Keyframes,
    Sine,
};

struct FaceMotionKeyframe {
    float time_seconds = 0.0f;
    float value        = 0.0f;
};

/** A bounded scalar source selected by compiled preset data. */
struct FaceMotionTrackDefinition {
    uint8_t node_index      = 0;
    FacePoseChannel channel = FacePoseChannel::TransformTranslationX;
    uint8_t element_index   = 0;
    uint8_t priority        = 0;
    MotionResponseProfile response{};
    FaceMotionSource source             = FaceMotionSource::Keyframes;
    const FaceMotionKeyframe* keyframes = nullptr;
    uint8_t keyframe_count              = 0;
    MotionEasing easing                 = MotionEasing::Linear;
    // Parameters for FaceMotionSource::Sine.  They are ignored by keyframes.
    float center         = 0.0f;
    float amplitude      = 0.0f;
    float period_seconds = 1.0f;
    float phase_radians  = 0.0f;
};

struct FaceMotionProgramDefinition {
    const char* id                          = nullptr;
    const FaceMotionTrackDefinition* tracks = nullptr;
    uint8_t track_count                     = 0;
    float duration_seconds                  = 0.0f;
    bool loop                               = false;
    uint8_t priority                        = 0;
};

/** A named, event-selectable compiled animation goal. */
struct FaceAnimationGoalDefinition {
    const char* id                                  = nullptr;
    const char* pose_id                             = nullptr;
    FaceAnimationEvent event_mask                   = kNoFaceAnimationEvent;
    uint8_t priority                                = 0;
    uint32_t duration_ms                            = 0;
    const FaceMotionProgramDefinition* programs     = nullptr;
    uint8_t program_count                           = 0;
};

/** Fixed-capacity state for the generic event/program scheduler. */
struct FaceAnimationRuntime {
    FacePoseRuntime pose{};
    std::array<FacePoseRuntimeTarget, kMaxFacePoseTargets> pose_baseline{};
    const FaceAnimationGoalDefinition* active_goal  = nullptr;
    const FaceAnimationGoalDefinition* default_goal = nullptr;
    uint32_t elapsed_ms                             = 0;
    bool initialized                                = false;
};

/** Resolves a compiled preset event name without a firmware-side event enum. */
[[nodiscard]] FaceAnimationEvent findFaceAnimationEvent(const FacePresetDefinition& preset, std::string_view event_id);

[[nodiscard]] bool initializeFaceAnimation(const FacePresetDefinition& preset, const FacePresetEvaluation& base,
                                           FaceAnimationRuntime& runtime);
[[nodiscard]] bool dispatchFaceAnimation(const FacePresetDefinition& preset, FaceAnimationEvent event,
                                         const FacePresetEvaluation& current, FaceAnimationRuntime& runtime);
[[nodiscard]] bool advanceFaceAnimation(const FacePresetDefinition& preset, float delta_seconds,
                                        FacePresetEvaluation& evaluation, FaceAnimationRuntime& runtime,
                                        bool* changed = nullptr);

}  // namespace pet::preset
