/* SPDX-License-Identifier: MIT */
#pragma once
#include "face_preset.h"
#include <pet/face/glyph_paths.h>
namespace pet::preset {
[[nodiscard]] bool buildFaceDrawing(const FacePresetDefinition& preset, face::FaceDrawing& drawing);
}  // namespace pet::preset
