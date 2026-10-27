/*
 * SPDX-License-Identifier: MIT
 */
#include "face_preset_registry.h"

#include <pet_generated/face_presets.generated.h>

namespace pet::preset {

const FacePresetDefinition* findFacePreset(std::string_view id)
{
    for (uint8_t index = 0; index < generated::kFacePresetCount; ++index) {
        const FacePresetDefinition* preset = generated::kFacePresets[index];
        if (preset != nullptr && preset->id != nullptr && id == preset->id) {
            return preset;
        }
    }
    return nullptr;
}

const FacePresetDefinition& defaultFacePreset()
{
    // The compiler refuses an empty input set, so index zero is always safe.
    return *generated::kFacePresets[0];
}

}  // namespace pet::preset
