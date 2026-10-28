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
        {"idle", "idle", FaceAnimationEvent::None, 0, 0, programs, 1},
        {"alert", "alert", FaceAnimationEvent::Pet, 10, 100, nullptr, 0},
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

    assert(dispatchFaceAnimation(preset, FaceAnimationEvent::Pet, evaluation, runtime));
    assert(advanceFaceAnimation(preset, 0.001f, evaluation, runtime));
    assert(std::fabs(evaluation.nodes[0].transform.scale.y - 1.4f) < 0.0001f);
    assert(advanceFaceAnimation(preset, 0.11f, evaluation, runtime));
    assert(runtime.active_goal == runtime.default_goal);
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
    const FaceAnimationGoalDefinition goals[] = {{"idle", "idle", FaceAnimationEvent::None, 0, 0, programs, 1}};
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
    testProceduralSourceUsesTheSameChannelPath();
    return 0;
}
