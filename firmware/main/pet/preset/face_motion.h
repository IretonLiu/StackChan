/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

namespace pet::preset {

/** Fixed capacity for host/device-shared keyframe motion. */
constexpr uint8_t kMaxMotionKeyframes = 8;

struct MotionRange {
    float minimum = 0.0f;
    float maximum = 0.0f;
};

/** Additive sinusoidal loop around an authored base value. */
struct LoopMotion {
    float amplitude      = 0.0f;
    float period_seconds = 1.0f;
    float phase_radians  = 0.0f;
};

struct MotionKeyframe {
    float time_seconds = 0.0f;
    float value        = 0.0f;
};

/** One motion source per property. keyframes must be ordered by time. */
struct KeyframeMotion {
    MotionKeyframe keyframes[kMaxMotionKeyframes]{};
    uint8_t count          = 0;
    bool loop              = false;
    float duration_seconds = 0.0f;
};

enum class MotionEasing : uint8_t {
    Linear,
    Smoothstep,
};

/**
 * Evaluates `clamp(base + amplitude * sin(2*pi*t/period + phase))`.
 * Returns false for invalid ranges or non-finite/zero-period motion settings.
 */
[[nodiscard]] bool evaluateLoopMotion(float base, const LoopMotion& motion, uint32_t elapsed_ms, MotionRange range,
                                      float& output);

/**
 * Evaluates ordered keyframes with the selected easing. A looping motion wraps
 * elapsed time by duration_seconds; a non-looping motion holds the last value.
 * Returns false for invalid ranges, non-finite values, or unordered keyframes.
 */
[[nodiscard]] bool evaluateKeyframeMotion(const KeyframeMotion& motion, uint32_t elapsed_ms, MotionRange range,
                                          MotionEasing easing, float& output);

}  // namespace pet::preset
