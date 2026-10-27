/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "face_preset.h"

#include <string_view>

namespace pet::preset {

/** Returns the compiled preset selected as the Pet Base idle display. */
const FacePresetDefinition& defaultFacePreset();

/** Returns nullptr for an unknown host-compiled preset id. */
const FacePresetDefinition* findFacePreset(std::string_view id);

}  // namespace pet::preset
