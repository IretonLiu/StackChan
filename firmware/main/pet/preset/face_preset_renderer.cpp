/* SPDX-License-Identifier: MIT */
#include "face_preset_renderer.h"
#include <pet/shape/shape_face_adapter.h>

namespace pet::preset {
bool buildFaceDrawing(const FacePresetDefinition& preset, face::FaceDrawing& drawing)
{
    FacePresetEvaluation evaluation;
    return evaluateFacePreset(preset, 0, evaluation) && buildFaceDrawing(evaluation, drawing);
}

bool buildFaceDrawing(const FacePresetEvaluation& evaluation, face::FaceDrawing& drawing)
{
    if (evaluation.node_count == 0 || evaluation.node_count > drawing.paths.size()) return false;
    face::FaceDrawing result{};
    result.background = {evaluation.background.red, evaluation.background.green, evaluation.background.blue};
    std::array<shape::Affine2D, face::kMaxFacePaths> worlds{};
    for (uint8_t index = 0; index < evaluation.node_count; ++index) {
        const auto& node            = evaluation.nodes[index];
        const shape::Affine2D local = shape::makeAffine(node.transform);
        if (node.parent_index < -1 || node.parent_index >= static_cast<int8_t>(index)) return false;
        worlds[index] = node.parent_index < 0 ? local : shape::compose(worlds[node.parent_index], local);
        shape::ShapePath path;
        bool sampled = false;
        if (node.kind == ShapeKind::Circle) {
            shape::CircleSample sample;
            sampled = shape::sampleCircle(node.circle, worlds[index], sample, node.style);
            if (sampled) path = sample.path;
        } else if (node.kind == ShapeKind::RoundedPolygon) {
            shape::RoundedPolygonSample sample;
            sampled = shape::sampleRoundedPolygon(node.rounded_polygon, worlds[index], sample, node.style);
            if (sampled) path = sample.path;
        }
        if (!sampled || !shape::appendToFaceDrawing(path, result)) return false;
    }
    drawing = result;
    return true;
}
}  // namespace pet::preset
