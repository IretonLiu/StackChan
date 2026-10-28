/*
 * SPDX-License-Identifier: MIT
 */
#include "face_pose_engine.h"

#include <cmath>

namespace pet::preset {
namespace {

constexpr MotionResponseRange kCoordinateRange{-4096.0f, 4096.0f};
constexpr MotionResponseRange kRotationRange{-3600.0f, 3600.0f};
constexpr MotionResponseRange kScaleRange{0.01f, 16.0f};
constexpr MotionResponseRange kRadiusRange{0.01f, 4096.0f};

bool finite(float value)
{
    return std::isfinite(value);
}

bool sameChannel(const FacePoseRuntimeTarget& runtime, const FacePoseTargetDefinition& definition)
{
    return runtime.node_index == definition.node_index && runtime.channel == definition.channel &&
           runtime.element_index == definition.element_index;
}

bool locateChannel(FacePresetEvaluation& evaluation, uint8_t node_index, FacePoseChannel channel, uint8_t element_index,
                   float*& value, MotionResponseRange& range)
{
    if (node_index >= evaluation.node_count) return false;
    FacePresetNodeDefinition& node = evaluation.nodes[node_index];
    switch (channel) {
        case FacePoseChannel::TransformTranslationX:
            if (element_index != 0) return false;
            value = &node.transform.translation.x;
            range = kCoordinateRange;
            return true;
        case FacePoseChannel::TransformTranslationY:
            if (element_index != 0) return false;
            value = &node.transform.translation.y;
            range = kCoordinateRange;
            return true;
        case FacePoseChannel::TransformRotationDegrees:
            if (element_index != 0) return false;
            value = &node.transform.rotation_degrees;
            range = kRotationRange;
            return true;
        case FacePoseChannel::TransformScaleX:
            if (element_index != 0) return false;
            value = &node.transform.scale.x;
            range = kScaleRange;
            return true;
        case FacePoseChannel::TransformScaleY:
            if (element_index != 0) return false;
            value = &node.transform.scale.y;
            range = kScaleRange;
            return true;
        case FacePoseChannel::CircleCenterX:
            if (node.kind != ShapeKind::Circle || element_index != 0) return false;
            value = &node.circle.center.x;
            range = kCoordinateRange;
            return true;
        case FacePoseChannel::CircleCenterY:
            if (node.kind != ShapeKind::Circle || element_index != 0) return false;
            value = &node.circle.center.y;
            range = kCoordinateRange;
            return true;
        case FacePoseChannel::CircleRadius:
            if (node.kind != ShapeKind::Circle || element_index != 0) return false;
            value = &node.circle.radius;
            range = kRadiusRange;
            return true;
        case FacePoseChannel::PolygonCornerX:
            if (node.kind != ShapeKind::RoundedPolygon || element_index >= node.rounded_polygon.corner_count)
                return false;
            value = &node.rounded_polygon.corner_positions[element_index].x;
            range = kCoordinateRange;
            return true;
        case FacePoseChannel::PolygonCornerY:
            if (node.kind != ShapeKind::RoundedPolygon || element_index >= node.rounded_polygon.corner_count)
                return false;
            value = &node.rounded_polygon.corner_positions[element_index].y;
            range = kCoordinateRange;
            return true;
        case FacePoseChannel::PolygonCornerRoundness:
            if (node.kind != ShapeKind::RoundedPolygon || element_index >= node.rounded_polygon.corner_count)
                return false;
            value = &node.rounded_polygon.corners[element_index].roundness;
            range = {shape::kMinCornerRoundness, shape::kMaxCornerRoundness};
            return true;
        case FacePoseChannel::PolygonCornerCapTension:
            if (node.kind != ShapeKind::RoundedPolygon || element_index >= node.rounded_polygon.corner_count)
                return false;
            value = &node.rounded_polygon.corners[element_index].cap_tension;
            range = {shape::kMinCapTension, shape::kMaxCapTension};
            return true;
        case FacePoseChannel::PolygonEdgeBulge:
            if (node.kind != ShapeKind::RoundedPolygon || element_index >= node.rounded_polygon.corner_count)
                return false;
            value = &node.rounded_polygon.edges[element_index].bulge;
            range = {-shape::kMaxEdgeBulge, shape::kMaxEdgeBulge};
            return true;
        case FacePoseChannel::PolygonEdgeStart:
            if (node.kind != ShapeKind::RoundedPolygon || element_index >= node.rounded_polygon.corner_count)
                return false;
            value = &node.rounded_polygon.edges[element_index].start;
            range = {shape::kMinEdgeHandle, shape::kMaxEdgeHandle};
            return true;
        case FacePoseChannel::PolygonEdgeEnd:
            if (node.kind != ShapeKind::RoundedPolygon || element_index >= node.rounded_polygon.corner_count)
                return false;
            value = &node.rounded_polygon.edges[element_index].end;
            range = {shape::kMinEdgeHandle, shape::kMaxEdgeHandle};
            return true;
    }
    return false;
}

bool contains(MotionResponseRange range, float value)
{
    return finite(value) && value >= range.minimum && value <= range.maximum;
}

bool validGeometry(const FacePresetEvaluation& evaluation)
{
    for (uint8_t node_index = 0; node_index < evaluation.node_count; ++node_index) {
        const FacePresetNodeDefinition& node = evaluation.nodes[node_index];
        if (node.kind != ShapeKind::RoundedPolygon) continue;
        shape::RoundedPolygonSample sample;
        if (!shape::sampleRoundedPolygon(node.rounded_polygon, {}, sample, node.style)) return false;
    }
    return true;
}

}  // namespace

bool selectFacePose(const FacePresetDefinition& preset, std::string_view pose_id, const FacePresetEvaluation& base,
                    FacePoseRuntime& runtime)
{
    if (pose_id.empty() || preset.poses == nullptr || preset.pose_count > kMaxFacePresetPoses || base.node_count == 0 ||
        base.node_count > kMaxFacePresetNodes)
        return false;

    const FacePoseDefinition* pose = nullptr;
    for (uint8_t index = 0; index < preset.pose_count; ++index) {
        if (preset.poses[index].id != nullptr && pose_id == preset.poses[index].id) {
            pose = &preset.poses[index];
            break;
        }
    }
    if (pose == nullptr || pose->target_count > kMaxFacePoseTargets ||
        (pose->target_count > 0 && pose->targets == nullptr))
        return false;

    FacePresetEvaluation base_copy = base;
    FacePoseRuntime result{};
    result.response_function = runtime.response_function;
    if (result.response_function == nullptr) return false;
    for (uint8_t index = 0; index < pose->target_count; ++index) {
        const FacePoseTargetDefinition& definition = pose->targets[index];
        float* current                             = nullptr;
        MotionResponseRange range;
        if (!locateChannel(base_copy, definition.node_index, definition.channel, definition.element_index, current,
                           range) ||
            !contains(range, definition.value) || !validMotionResponseProfile(definition.response))
            return false;

        for (uint8_t previous = 0; previous < index; ++previous) {
            if (sameChannel(result.targets[previous], definition)) return false;
        }

        MotionResponseState state{*current, 0.0f};
        for (uint8_t previous = 0; previous < runtime.target_count && previous < kMaxFacePoseTargets; ++previous) {
            if (sameChannel(runtime.targets[previous], definition)) {
                state = runtime.targets[previous].state;
                break;
            }
        }
        result.targets[index] = {definition.node_index, definition.channel,  definition.element_index,
                                 definition.value,      definition.response, state};
    }
    result.target_count = pose->target_count;
    runtime             = result;
    return true;
}

bool advanceFacePose(FacePoseRuntime& runtime, float delta_seconds, FacePresetEvaluation& evaluation)
{
    if (runtime.target_count > kMaxFacePoseTargets || runtime.response_function == nullptr || !finite(delta_seconds) ||
        delta_seconds < 0.0f)
        return false;

    // Validate every target before mutating so a rejected definition cannot leave a
    // half-advanced pose behind. Advance in place while saving only the touched fields;
    // copying FacePresetEvaluation here would cost ~4.5 KB of stack per frame, which is
    // more headroom than an ESP32 task stack should be assumed to have.
    struct SavedField {
        float* location = nullptr;
        float original  = 0.0f;
    };
    std::array<SavedField, kMaxFacePoseTargets> saved{};
    std::array<MotionResponseState, kMaxFacePoseTargets> saved_states{};
    uint8_t saved_count = 0;
    bool failed         = false;

    for (uint8_t index = 0; index < runtime.target_count; ++index) {
        FacePoseRuntimeTarget& target = runtime.targets[index];
        float* destination            = nullptr;
        MotionResponseRange range;
        if (!locateChannel(evaluation, target.node_index, target.channel, target.element_index, destination, range) ||
            !validMotionResponseProfile(target.response) || !finite(target.state.value) ||
            !finite(target.state.velocity)) {
            failed = true;
            break;
        }
        saved[saved_count].location = destination;
        saved[saved_count].original = *destination;
        saved_states[saved_count]   = target.state;
        ++saved_count;
        if (!advanceMotionResponse(target.state, target.target, delta_seconds, range, target.response,
                                   runtime.response_function)) {
            failed = true;
            break;
        }
        *destination = target.state.value;
    }

    if (!failed && !validGeometry(evaluation)) failed = true;

    if (failed) {
        // Unwind in reverse so repeated targets on the same channel restore the
        // original value rather than an intermediate one.
        for (uint8_t index = saved_count; index > 0; --index) {
            *saved[index - 1].location       = saved[index - 1].original;
            runtime.targets[index - 1].state = saved_states[index - 1];
        }
        return false;
    }
    return true;
}

}  // namespace pet::preset
