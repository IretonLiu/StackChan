/*
 * SPDX-License-Identifier: MIT
 */
#include "face_motion.h"

#include <cmath>

namespace pet::preset {
namespace {

constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

bool validRange(MotionRange range)
{
    return std::isfinite(range.minimum) && std::isfinite(range.maximum) && range.minimum <= range.maximum;
}

float clampToRange(float value, MotionRange range)
{
    return value < range.minimum ? range.minimum : (value > range.maximum ? range.maximum : value);
}

float smoothstep(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

bool evaluateLoopMotion(float base, const LoopMotion& motion, uint32_t elapsed_ms, MotionRange range, float& output)
{
    if (!std::isfinite(base) || !std::isfinite(motion.amplitude) || !std::isfinite(motion.period_seconds) ||
        !std::isfinite(motion.phase_radians) || motion.period_seconds <= 0.0f || !validRange(range))
        return false;
    const float seconds = static_cast<float>(elapsed_ms) / 1000.0f;
    const float phase   = kTwoPi * seconds / motion.period_seconds + motion.phase_radians;
    output              = clampToRange(base + motion.amplitude * std::sin(phase), range);
    return true;
}

bool evaluateKeyframeMotion(const KeyframeMotion& motion, uint32_t elapsed_ms, MotionRange range, MotionEasing easing,
                            float& output)
{
    if (!validRange(range) || motion.count == 0) return false;
    for (uint8_t index = 0; index < motion.count; ++index) {
        if (!std::isfinite(motion.keyframes[index].time_seconds) || !std::isfinite(motion.keyframes[index].value))
            return false;
        if (index > 0 && motion.keyframes[index].time_seconds < motion.keyframes[index - 1].time_seconds) return false;
    }
    if (motion.loop && (!std::isfinite(motion.duration_seconds) || motion.duration_seconds <= 0.0f)) return false;

    float seconds = static_cast<float>(elapsed_ms) / 1000.0f;
    if (motion.loop)
        seconds = std::fmod(seconds, motion.duration_seconds);
    else if (seconds > motion.keyframes[motion.count - 1].time_seconds)
        seconds = motion.keyframes[motion.count - 1].time_seconds;

    if (seconds <= motion.keyframes[0].time_seconds) {
        output = clampToRange(motion.keyframes[0].value, range);
        return true;
    }

    for (uint8_t index = 1; index < motion.count; ++index) {
        const MotionKeyframe& previous = motion.keyframes[index - 1];
        const MotionKeyframe& next     = motion.keyframes[index];
        if (seconds > next.time_seconds) continue;
        const float span = next.time_seconds - previous.time_seconds;
        float blend      = span <= 0.0f ? 1.0f : (seconds - previous.time_seconds) / span;
        if (easing == MotionEasing::Smoothstep) blend = smoothstep(blend);
        output = clampToRange(previous.value + (next.value - previous.value) * blend, range);
        return true;
    }

    output = clampToRange(motion.keyframes[motion.count - 1].value, range);
    return true;
}

}  // namespace pet::preset
