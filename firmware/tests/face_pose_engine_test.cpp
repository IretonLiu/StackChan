/*
 * SPDX-License-Identifier: MIT
 */
#include <pet/preset/face_pose_engine.h>

#include <cassert>
#include <cmath>

namespace {

using pet::preset::FacePoseChannel;
using pet::preset::FacePoseDefinition;
using pet::preset::FacePoseRuntime;
using pet::preset::FacePoseTargetDefinition;
using pet::preset::FacePresetDefinition;
using pet::preset::FacePresetEvaluation;
using pet::preset::MotionResponseProfile;
using pet::preset::MotionResponseState;
using pet::preset::ShapeKind;

bool near(float left, float right, float epsilon = 0.0005f)
{
    return std::fabs(left - right) <= epsilon;
}

bool snapToTarget(MotionResponseState& state, float target, float, const MotionResponseProfile&)
{
    state.value    = target;
    state.velocity = 0.0f;
    return true;
}

FacePresetEvaluation baseEvaluation()
{
    FacePresetEvaluation result;
    result.node_count                                = 2;
    result.nodes[0].kind                             = ShapeKind::Circle;
    result.nodes[0].circle                           = {{120.0f, 120.0f}, 6.0f};
    result.nodes[1].kind                             = ShapeKind::RoundedPolygon;
    result.nodes[1].rounded_polygon.corner_count     = 4;
    result.nodes[1].rounded_polygon.corner_positions = {{{0.0f, 0.0f}, {40.0f, 0.0f}, {40.0f, 40.0f}, {0.0f, 40.0f}}};
    for (uint8_t index = 0; index < 4; ++index) {
        result.nodes[1].rounded_polygon.corners[index] = {0.08f, 0.55f};
        result.nodes[1].rounded_polygon.edges[index]   = {0.0f, 0.28f, 0.28f};
    }
    return result;
}

MotionResponseProfile profile()
{
    MotionResponseProfile result;
    result.frequency_hz         = 3.0f;
    result.damping_ratio        = 1.0f;
    result.maximum_step_seconds = 1.0f / 120.0f;
    return result;
}

void testTypedTargetsApplyThroughInjectedResponse()
{
    const FacePoseTargetDefinition targets[] = {
        {0, FacePoseChannel::CircleRadius, 0, 14.0f, profile()},
        {1, FacePoseChannel::PolygonEdgeBulge, 2, 0.20f, profile()},
    };
    const FacePoseDefinition poses[] = {{"wide", targets, 2}};
    const FacePresetDefinition preset{"test", {}, nullptr, 0, poses, 1};
    const FacePresetEvaluation base = baseEvaluation();
    FacePresetEvaluation evaluation = base;
    FacePoseRuntime runtime;
    runtime.response_function = snapToTarget;

    assert(pet::preset::selectFacePose(preset, "wide", base, runtime));
    assert(pet::preset::advanceFacePose(runtime, 0.02f, evaluation));
    assert(near(evaluation.nodes[0].circle.radius, 14.0f));
    assert(near(evaluation.nodes[1].rounded_polygon.edges[2].bulge, 0.20f));
}

void testChangingGoalRetainsCurrentState()
{
    const FacePoseTargetDefinition wide_targets[]   = {{0, FacePoseChannel::CircleRadius, 0, 14.0f, profile()}};
    const FacePoseTargetDefinition narrow_targets[] = {{0, FacePoseChannel::CircleRadius, 0, 2.0f, profile()}};
    const FacePoseDefinition poses[]                = {{"wide", wide_targets, 1}, {"narrow", narrow_targets, 1}};
    const FacePresetDefinition preset{"test", {}, nullptr, 0, poses, 2};
    const FacePresetEvaluation base = baseEvaluation();
    FacePresetEvaluation evaluation = base;
    FacePoseRuntime runtime;

    assert(pet::preset::selectFacePose(preset, "wide", base, runtime));
    assert(pet::preset::advanceFacePose(runtime, 0.08f, evaluation));
    const float current_radius = evaluation.nodes[0].circle.radius;
    assert(current_radius > base.nodes[0].circle.radius && current_radius < 14.0f);

    assert(pet::preset::selectFacePose(preset, "narrow", base, runtime));
    evaluation = base;
    assert(pet::preset::advanceFacePose(runtime, 0.0f, evaluation));
    assert(near(evaluation.nodes[0].circle.radius, current_radius));
    assert(pet::preset::advanceFacePose(runtime, 0.08f, evaluation));
    assert(evaluation.nodes[0].circle.radius < current_radius);
    assert(evaluation.nodes[0].circle.radius > 2.0f);
}

void testInvalidDefinitionsDoNotReplaceRuntime()
{
    const FacePoseTargetDefinition valid_targets[]     = {{0, FacePoseChannel::CircleRadius, 0, 10.0f, profile()}};
    const FacePoseTargetDefinition invalid_targets[]   = {{1, FacePoseChannel::CircleRadius, 0, 10.0f, profile()}};
    const FacePoseTargetDefinition duplicate_targets[] = {
        {0, FacePoseChannel::CircleRadius, 0, 10.0f, profile()},
        {0, FacePoseChannel::CircleRadius, 0, 8.0f, profile()},
    };
    const FacePoseDefinition poses[] = {
        {"valid", valid_targets, 1},
        {"invalid", invalid_targets, 1},
        {"duplicate", duplicate_targets, 2},
    };
    const FacePresetDefinition preset{"test", {}, nullptr, 0, poses, 3};
    const FacePresetEvaluation base = baseEvaluation();
    FacePoseRuntime runtime;

    assert(pet::preset::selectFacePose(preset, "valid", base, runtime));
    const FacePoseRuntime preserved = runtime;
    assert(!pet::preset::selectFacePose(preset, "invalid", base, runtime));
    assert(runtime.target_count == preserved.target_count);
    assert(near(runtime.targets[0].target, preserved.targets[0].target));
    assert(!pet::preset::selectFacePose(preset, "duplicate", base, runtime));
    assert(runtime.target_count == preserved.target_count);
}

}  // namespace

int main()
{
    testTypedTargetsApplyThroughInjectedResponse();
    testChangingGoalRetainsCurrentState();
    testInvalidDefinitionsDoNotReplaceRuntime();
    return 0;
}
