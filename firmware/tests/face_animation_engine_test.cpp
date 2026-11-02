/*
 * SPDX-License-Identifier: MIT
 */
#include <pet/preset/face_animation_engine.h>

#include <cassert>
#include <cmath>

namespace {

using namespace pet::preset;

bool snap(MotionResponseState& state, float target, float, const MotionResponseProfile&)
{
    state.value = target;
    state.velocity = 0.0f;
    return true;
}

MotionResponseProfile response()
{
    return {6.0f, 1.0f, 1.0f / 120.0f};
}

FacePresetEvaluation base()
{
    FacePresetEvaluation evaluation;
    evaluation.node_count = 1;
    evaluation.nodes[0].kind = ShapeKind::Circle;
    evaluation.nodes[0].transform.scale = {1.0f, 1.0f};
    evaluation.nodes[0].circle = {{160.0f, 120.0f}, 8.0f};
    return evaluation;
}

void testCompiledTrackAndEventExpiry()
{
    const FacePoseTargetDefinition idle_targets[] = {{0, FacePoseChannel::TransformScaleY, 0, 1.0f, response()}};
    const FacePoseTargetDefinition alert_targets[] = {{0, FacePoseChannel::TransformScaleY, 0, 1.4f, response()}};
    const FacePoseDefinition poses[] = {{"idle", idle_targets, 1}, {"alert", alert_targets, 1}};
    const FaceMotionKeyframe frames[] = {{0.0f, 1.0f}, {0.5f, 0.1f}, {1.0f, 1.0f}};
    const FaceMotionTrackDefinition tracks[] = {
        {0, FacePoseChannel::TransformScaleY, 0, 0, response(), FaceMotionSource::Keyframes, frames, 3,
         MotionEasing::Linear},
    };
    const FaceMotionProgramDefinition programs[] = {{"blink", tracks, 1, 1.0f, true, 0}};
    const FaceAnimationGoalDefinition goals[] = {
        {"idle", "idle", kNoFaceAnimationEvent, 0, 0, programs, 1},
        {"alert", "alert", faceAnimationEventMask(FaceAnimationEvent::ScreenTap), 10, 100, nullptr, 0},
    };
    const FacePresetDefinition preset{"test", {}, nullptr, 0, poses, 2, goals, 2, "idle"};
    FacePresetEvaluation evaluation = base();
    FaceAnimationRuntime runtime;
    runtime.pose.response_function = snap;

    assert(initializeFaceAnimation(preset, evaluation, runtime));
    bool changed = false;
    assert(advanceFaceAnimation(preset, 0.5f, evaluation, runtime, &changed));
    assert(changed);
    assert(std::fabs(evaluation.nodes[0].transform.scale.y - 0.1f) < 0.0001f);

    assert(dispatchFaceAnimation(preset, FaceAnimationEvent::ScreenTap, evaluation, runtime));
    assert(advanceFaceAnimation(preset, 0.001f, evaluation, runtime));
    assert(std::fabs(evaluation.nodes[0].transform.scale.y - 1.4f) < 0.0001f);
    assert(advanceFaceAnimation(preset, 0.11f, evaluation, runtime));
    assert(runtime.active_goal == runtime.default_goal);
}

void testSchemaEventsSelectHighestPriorityAndLatch()
{
    const FacePoseTargetDefinition idle_targets[] = {{0, FacePoseChannel::TransformScaleY, 0, 1.0f, response()}};
    const FacePoseTargetDefinition low_targets[] = {{0, FacePoseChannel::TransformScaleY, 0, 1.1f, response()}};
    const FacePoseTargetDefinition high_targets[] = {{0, FacePoseChannel::TransformScaleY, 0, 1.3f, response()}};
    const FacePoseDefinition poses[] = {{"idle", idle_targets, 1}, {"low", low_targets, 1}, {"high", high_targets, 1}};
    const FaceAnimationEvent events[] = {FaceAnimationEvent::ScreenTap, FaceAnimationEvent::PetStroke,
                                         FaceAnimationEvent::Shake, FaceAnimationEvent::SleepTimeout,
                                         FaceAnimationEvent::Wake, FaceAnimationEvent::WakeComplete,
                                         FaceAnimationEvent::HappyComplete};
    const FaceAnimationEventMask all_events = faceAnimationEventMask(FaceAnimationEvent::ScreenTap) |
                                              faceAnimationEventMask(FaceAnimationEvent::PetStroke) |
                                              faceAnimationEventMask(FaceAnimationEvent::Shake) |
                                              faceAnimationEventMask(FaceAnimationEvent::SleepTimeout) |
                                              faceAnimationEventMask(FaceAnimationEvent::Wake) |
                                              faceAnimationEventMask(FaceAnimationEvent::WakeComplete) |
                                              faceAnimationEventMask(FaceAnimationEvent::HappyComplete);
    const FaceAnimationGoalDefinition goals[] = {
        {"idle", "idle", kNoFaceAnimationEvent, 0, 0, nullptr, 0},
        {"low", "low", all_events, 2, 0, nullptr, 0},
        {"high", "high", all_events, 3, 0, nullptr, 0},
    };
    const FacePresetDefinition preset{"test", {}, nullptr, 0, poses, 3, goals, 3, "idle"};
    FacePresetEvaluation evaluation = base();
    FaceAnimationRuntime runtime;
    runtime.pose.response_function = snap;

    assert(initializeFaceAnimation(preset, evaluation, runtime));
    for (uint8_t index = 0; index < 7; ++index) {
        assert(faceAnimationEventMask(events[index]) == (1u << index));
        assert(dispatchFaceAnimation(preset, events[index], evaluation, runtime));
        assert(runtime.active_goal == &goals[2]);
    }
    assert(advanceFaceAnimation(preset, 60.0f, evaluation, runtime));
    assert(runtime.active_goal == &goals[2]);
}

void testProceduralSourceUsesTheSameChannelPath()
{
    const FacePoseTargetDefinition targets[] = {{0, FacePoseChannel::TransformScaleY, 0, 1.0f, response()}};
    const FacePoseDefinition poses[] = {{"idle", targets, 1}};
    const FaceMotionTrackDefinition tracks[] = {
        {0, FacePoseChannel::TransformScaleY, 0, 0, response(), FaceMotionSource::Sine, nullptr, 0,
         MotionEasing::Linear, 1.2f, 0.3f, 1.0f, 0.0f},
    };
    const FaceMotionProgramDefinition programs[] = {{"drift", tracks, 1, 1.0f, true, 0}};
    const FaceAnimationGoalDefinition goals[] = {{"idle", "idle", kNoFaceAnimationEvent, 0, 0, programs, 1}};
    const FacePresetDefinition preset{"test", {}, nullptr, 0, poses, 1, goals, 1, "idle"};
    FacePresetEvaluation evaluation = base();
    FaceAnimationRuntime runtime;
    runtime.pose.response_function = snap;

    assert(initializeFaceAnimation(preset, evaluation, runtime));
    assert(advanceFaceAnimation(preset, 0.25f, evaluation, runtime));
    assert(std::fabs(evaluation.nodes[0].transform.scale.y - 1.5f) < 0.0001f);
}

}  // namespace

int main()
{
    testCompiledTrackAndEventExpiry();
    testSchemaEventsSelectHighestPriorityAndLatch();
    testProceduralSourceUsesTheSameChannelPath();
    return 0;
}
