/* SPDX-License-Identifier: MIT */
#pragma once
#include "face_preset_evaluator.h"
#include <pet/face/glyph_paths.h>
namespace pet::preset {
[[nodiscard]] bool buildFaceDrawing(const FacePresetDefinition& preset, face::FaceDrawing& drawing);
[[nodiscard]] bool buildFaceDrawing(const FacePresetEvaluation& evaluation, face::FaceDrawing& drawing);
}  // namespace pet::preset
