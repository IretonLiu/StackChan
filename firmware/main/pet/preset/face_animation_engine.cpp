/*
 * SPDX-License-Identifier: MIT
 */
#include "face_animation_engine.h"

#include <cmath>
#include <string_view>

namespace pet::preset {
namespace {

bool finite(float value)
{
    return std::isfinite(value);
}

bool sameChannel(const FacePoseRuntimeTarget& target, const FaceMotionTrackDefinition& track)
{
    return target.node_index == track.node_index && target.channel == track.channel &&
           target.element_index == track.element_index;
}

bool eventMatches(FaceAnimationEvent mask, FaceAnimationEvent event)
{
    return event != FaceAnimationEvent::None &&
           (static_cast<uint8_t>(mask) & static_cast<uint8_t>(event)) != 0;
}

const FaceAnimationGoalDefinition* findGoal(const FacePresetDefinition& preset, const char* id)
{
    if (id == nullptr || preset.animation_goals == nullptr || preset.animation_goal_count == 0 ||
        preset.animation_goal_count > kMaxFaceAnimationGoals)
        return nullptr;
    for (uint8_t index = 0; index < preset.animation_goal_count; ++index) {
        const FaceAnimationGoalDefinition& goal = preset.animation_goals[index];
        if (goal.id != nullptr && std::string_view(goal.id) == id) return &goal;
    }
    return nullptr;
}

bool validGoal(const FaceAnimationGoalDefinition& goal)
{
    return goal.id != nullptr && goal.pose_id != nullptr && goal.program_count <= kMaxFaceMotionPrograms &&
           (goal.program_count == 0 || goal.programs != nullptr);
}

bool activateGoal(const FaceAnimationGoalDefinition& goal, const FacePresetDefinition& preset,
                  const FacePresetEvaluation& current, FaceAnimationRuntime& runtime)
{
    if (!validGoal(goal) || !selectFacePose(preset, goal.pose_id, current, runtime.pose)) return false;
    runtime.active_goal = &goal;
    runtime.elapsed_ms  = 0;
    for (uint8_t index = 0; index < runtime.pose.target_count; ++index) {
        runtime.pose_baseline[index] = runtime.pose.targets[index];
    }
    return true;
}

float smoothstep(float value)
{
    return value * value * (3.0f - 2.0f * value);
}

bool evaluateTrack(const FaceMotionTrackDefinition& track, float seconds, float& value)
{
    if (!finite(seconds) || seconds < 0.0f || !validMotionResponseProfile(track.response)) return false;
    if (track.source == FaceMotionSource::Sine) {
        if (!finite(track.center) || !finite(track.amplitude) || !finite(track.period_seconds) ||
            !finite(track.phase_radians) || track.period_seconds <= 0.0f)
            return false;
        value = track.center + track.amplitude * std::sin((seconds / track.period_seconds) * 6.28318530718f +
                                                           track.phase_radians);
        return finite(value);
    }
    if (track.source != FaceMotionSource::Keyframes || track.keyframes == nullptr || track.keyframe_count < 2 ||
        track.keyframe_count > kMaxFaceMotionKeyframes)
        return false;
    const FaceMotionKeyframe& first = track.keyframes[0];
    if (!finite(first.time_seconds) || !finite(first.value) || first.time_seconds != 0.0f) return false;
    if (seconds <= first.time_seconds) {
        value = first.value;
        return true;
    }
    for (uint8_t index = 1; index < track.keyframe_count; ++index) {
        const FaceMotionKeyframe& previous = track.keyframes[index - 1];
        const FaceMotionKeyframe& next     = track.keyframes[index];
        if (!finite(next.time_seconds) || !finite(next.value) || next.time_seconds <= previous.time_seconds)
            return false;
        if (seconds <= next.time_seconds) {
            float amount = (seconds - previous.time_seconds) / (next.time_seconds - previous.time_seconds);
            if (track.easing == MotionEasing::Smoothstep) amount = smoothstep(amount);
            value = previous.value + (next.value - previous.value) * amount;
            return finite(value);
        }
    }
    value = track.keyframes[track.keyframe_count - 1].value;
    return true;
}

bool resolvePrograms(FaceAnimationRuntime& runtime)
{
    if (runtime.active_goal == nullptr || runtime.pose.target_count > kMaxFacePoseTargets) return false;
    std::array<const FaceMotionTrackDefinition*, kMaxFacePoseTargets> selected{};
    std::array<uint16_t, kMaxFacePoseTargets> selected_priority{};

    for (uint8_t index = 0; index < runtime.pose.target_count; ++index) {
        // Restore the pose goal before overlaying active tracks. Keep the live response
        // state: replacing it would make every loop boundary visibly jump.
        runtime.pose.targets[index].target   = runtime.pose_baseline[index].target;
        runtime.pose.targets[index].response = runtime.pose_baseline[index].response;
    }

    const FaceAnimationGoalDefinition& goal = *runtime.active_goal;
    const float elapsed_seconds = static_cast<float>(runtime.elapsed_ms) / 1000.0f;
    for (uint8_t program_index = 0; program_index < goal.program_count; ++program_index) {
        const FaceMotionProgramDefinition& program = goal.programs[program_index];
        if (program.id == nullptr || program.tracks == nullptr || program.track_count == 0 ||
            program.track_count > kMaxFaceMotionTracks || !finite(program.duration_seconds) ||
            program.duration_seconds <= 0.0f)
            return false;
        if (!program.loop && elapsed_seconds > program.duration_seconds) continue;
        for (uint8_t track_index = 0; track_index < program.track_count; ++track_index) {
            const FaceMotionTrackDefinition& track = program.tracks[track_index];
            uint8_t target_index = runtime.pose.target_count;
            for (uint8_t candidate = 0; candidate < runtime.pose.target_count; ++candidate) {
                if (sameChannel(runtime.pose_baseline[candidate], track)) {
                    target_index = candidate;
                    break;
                }
            }
            // Every animated channel must be declared in the selected pose. This keeps
            // the scheduler generic while retaining the pose engine's geometry checks.
            if (target_index == runtime.pose.target_count) return false;
            const uint16_t priority = static_cast<uint16_t>(program.priority) * 256u + track.priority;
            if (selected[target_index] == nullptr || priority > selected_priority[target_index]) {
                selected[target_index]          = &track;
                selected_priority[target_index] = priority;
                // Store the current local time indirectly by evaluating after winner
                // selection; this avoids allocating per-track runtime state.
            }
        }
    }

    for (uint8_t index = 0; index < runtime.pose.target_count; ++index) {
        const FaceMotionTrackDefinition* track = selected[index];
        if (track == nullptr) continue;
        // Find its program again to recover its local clock. Program counts are bounded
        // and tiny; this makes track definitions immutable and runtime allocation-free.
        for (uint8_t program_index = 0; program_index < goal.program_count; ++program_index) {
            const FaceMotionProgramDefinition& program = goal.programs[program_index];
            bool belongs = false;
            for (uint8_t track_index = 0; track_index < program.track_count; ++track_index) {
                if (&program.tracks[track_index] == track) {
                    belongs = true;
                    break;
                }
            }
            if (!belongs) continue;
            const float elapsed = static_cast<float>(runtime.elapsed_ms) / 1000.0f;
            const float local   = program.loop ? std::fmod(elapsed, program.duration_seconds) : elapsed;
            float value         = 0.0f;
            if (!evaluateTrack(*track, local, value)) return false;
            runtime.pose.targets[index].target   = value;
            runtime.pose.targets[index].response = track->response;
            break;
        }
    }
    return true;
}

}  // namespace

bool initializeFaceAnimation(const FacePresetDefinition& preset, const FacePresetEvaluation& base,
                             FaceAnimationRuntime& runtime)
{
    const FaceAnimationGoalDefinition* default_goal = findGoal(preset, preset.default_animation_goal_id);
    if (default_goal == nullptr) return false;
    FaceAnimationRuntime result{};
    result.pose.response_function = runtime.pose.response_function;
    if (result.pose.response_function == nullptr || !activateGoal(*default_goal, preset, base, result)) return false;
    result.default_goal = default_goal;
    result.initialized  = true;
    runtime             = result;
    return true;
}

bool dispatchFaceAnimation(const FacePresetDefinition& preset, FaceAnimationEvent event,
                           const FacePresetEvaluation& current, FaceAnimationRuntime& runtime)
{
    if (!runtime.initialized || event == FaceAnimationEvent::None || preset.animation_goals == nullptr ||
        preset.animation_goal_count > kMaxFaceAnimationGoals)
        return false;
    const FaceAnimationGoalDefinition* selected = nullptr;
    for (uint8_t index = 0; index < preset.animation_goal_count; ++index) {
        const FaceAnimationGoalDefinition& goal = preset.animation_goals[index];
        if (eventMatches(goal.event_mask, event) && (selected == nullptr || goal.priority > selected->priority)) {
            selected = &goal;
        }
    }
    return selected != nullptr && activateGoal(*selected, preset, current, runtime);
}

bool advanceFaceAnimation(const FacePresetDefinition& preset, float delta_seconds, FacePresetEvaluation& evaluation,
                          FaceAnimationRuntime& runtime, bool* changed)
{
    if (changed != nullptr) *changed = false;
    if (!runtime.initialized || runtime.active_goal == nullptr || runtime.default_goal == nullptr || !finite(delta_seconds) ||
        delta_seconds < 0.0f)
        return false;
    const uint32_t delta_ms = static_cast<uint32_t>(delta_seconds * 1000.0f + 0.5f);
    runtime.elapsed_ms += delta_ms;
    if (runtime.active_goal != runtime.default_goal && runtime.active_goal->duration_ms > 0 &&
        runtime.elapsed_ms >= runtime.active_goal->duration_ms &&
        !activateGoal(*runtime.default_goal, preset, evaluation, runtime))
        return false;
    if (!resolvePrograms(runtime)) return false;
    return advanceFacePose(runtime.pose, delta_seconds, evaluation, changed);
}

}  // namespace pet::preset
