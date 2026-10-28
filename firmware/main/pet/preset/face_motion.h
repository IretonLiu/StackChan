/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

namespace pet::preset {

/** Fixed capacity for the shared keyframe vocabulary used by animation tracks. */
constexpr uint8_t kMaxMotionKeyframes = 8;

struct MotionKeyframe {
    float time_seconds = 0.0f;
    float value        = 0.0f;
};

enum class MotionEasing : uint8_t {
    Linear,
    Smoothstep,
};

}  // namespace pet::preset
