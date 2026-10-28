/*
 * SPDX-License-Identifier: MIT
 */
#include "face_preset_evaluator.h"

#include <cmath>

namespace pet::preset {
namespace {

bool finite(float value)
{
    return std::isfinite(value);
}

bool finite(shape::Vec2 value)
{
    return finite(value.x) && finite(value.y);
}

bool finite(const shape::Transform2D& transform)
{
    return finite(transform.translation) && finite(transform.rotation_degrees) && finite(transform.scale) &&
           finite(transform.pivot);
}

bool finite(const shape::PathStyle& style)
{
    return finite(style.opacity) && finite(style.stroke_width);
}

FacePresetEvaluationError validateNode(const FacePresetNodeDefinition& node, uint8_t index)
{
    if (node.parent_index < -1 || node.parent_index >= static_cast<int8_t>(index))
        return FacePresetEvaluationError::InvalidParent;
    if (!finite(node.transform)) return FacePresetEvaluationError::NonFiniteValue;
    if (node.transform.scale.x == 0.0f || node.transform.scale.y == 0.0f)
        return FacePresetEvaluationError::InvalidTransform;
    if (!finite(node.style)) return FacePresetEvaluationError::NonFiniteValue;
    if (node.style.opacity < 0.0f || node.style.opacity > 1.0f || node.style.stroke_width < 0.0f)
        return FacePresetEvaluationError::InvalidStyle;

    if (node.kind == ShapeKind::Circle) {
        if (!finite(node.circle.center) || !finite(node.circle.radius))
            return FacePresetEvaluationError::NonFiniteValue;
        return node.circle.radius > 0.0f ? FacePresetEvaluationError::None : FacePresetEvaluationError::InvalidCircle;
    }
    if (node.kind != ShapeKind::RoundedPolygon) return FacePresetEvaluationError::InvalidNodeKind;

    const shape::RoundedPolygonParameters& polygon = node.rounded_polygon;
    if (polygon.corner_count < 3 || polygon.corner_count > shape::kMaxRoundedPolygonCorners)
        return FacePresetEvaluationError::InvalidPolygon;
    for (uint8_t index = 0; index < polygon.corner_count; ++index) {
        const shape::CornerControls& corner = polygon.corners[index];
        const shape::EdgeTension& edge      = polygon.edges[index];
        if (!finite(polygon.corner_positions[index]) || !finite(corner.roundness) || !finite(corner.cap_tension) ||
            !finite(edge.bulge) || !finite(edge.start) || !finite(edge.end))
            return FacePresetEvaluationError::NonFiniteValue;
    }
    return FacePresetEvaluationError::None;
}

void setError(FacePresetEvaluationError* target, FacePresetEvaluationError value)
{
    if (target != nullptr) *target = value;
}

}  // namespace

bool evaluateFacePreset(const FacePresetDefinition& preset, uint32_t elapsed_ms, FacePresetEvaluation& evaluation,
                        FacePresetEvaluationError* error)
{
    (void)elapsed_ms;
    if (preset.nodes == nullptr || preset.node_count == 0) {
        setError(error, FacePresetEvaluationError::EmptyPreset);
        return false;
    }
    if (preset.node_count > kMaxFacePresetNodes) {
        setError(error, FacePresetEvaluationError::TooManyNodes);
        return false;
    }

    FacePresetEvaluation result{};
    result.background = preset.background;
    result.node_count = preset.node_count;
    for (uint8_t index = 0; index < preset.node_count; ++index) {
        const FacePresetEvaluationError node_error = validateNode(preset.nodes[index], index);
        if (node_error != FacePresetEvaluationError::None) {
            setError(error, node_error);
            return false;
        }
        result.nodes[index] = preset.nodes[index];
    }

    evaluation = result;
    setError(error, FacePresetEvaluationError::None);
    return true;
}

}  // namespace pet::preset
