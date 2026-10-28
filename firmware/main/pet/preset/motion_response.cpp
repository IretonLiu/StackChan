/*
 * SPDX-License-Identifier: MIT
 */
#include "motion_response.h"

#include <cmath>

namespace pet::preset {
namespace {

constexpr float kPi                = 3.14159265358979323846f;
constexpr uint8_t kMaximumSubsteps = 8;

bool finite(float value)
{
    return std::isfinite(value);
}

bool finite(const MotionResponseState& state)
{
    return finite(state.value) && finite(state.velocity);
}

bool validRange(MotionResponseRange range)
{
    return finite(range.minimum) && finite(range.maximum) && range.minimum <= range.maximum;
}

float clamp(float value, MotionResponseRange range)
{
    return value < range.minimum ? range.minimum : (value > range.maximum ? range.maximum : value);
}

}  // namespace

bool advanceCriticallyDampedSpring(MotionResponseState& state, float target, float delta_seconds,
                                   const MotionResponseProfile& profile)
{
    // Semi-implicit Euler is only stable while the step stays under the profile's
    // integration limit. Reject a larger step instead of returning true for a
    // diverging update; use advanceMotionResponse() for variable frame times, as it
    // substeps within the same bound.
    if (!finite(state) || !finite(target) || !finite(delta_seconds) || delta_seconds < 0.0f ||
        !validMotionResponseProfile(profile) || delta_seconds > profile.maximum_step_seconds)
        return false;

    // Semi-implicit Euler preserves velocity for an interruptible, natural response.
    const float angular_frequency = 2.0f * kPi * profile.frequency_hz;
    const float acceleration      = angular_frequency * angular_frequency * (target - state.value) -
                                    2.0f * profile.damping_ratio * angular_frequency * state.velocity;
    state.velocity += acceleration * delta_seconds;
    state.value += state.velocity * delta_seconds;
    return finite(state);
}

bool advanceMotionResponse(MotionResponseState& state, float target, float delta_seconds, MotionResponseRange range,
                           const MotionResponseProfile& profile, MotionResponseFunction response)
{
    if (!finite(state) || !finite(target) || !finite(delta_seconds) || delta_seconds < 0.0f || !validRange(range) ||
        !validMotionResponseProfile(profile) || response == nullptr)
        return false;

    MotionResponseState result = state;
    const float clamped_target = clamp(target, range);
    float remaining_seconds    = delta_seconds;
    for (uint8_t step = 0; step < kMaximumSubsteps && remaining_seconds > 0.0f; ++step) {
        const float step_seconds =
            remaining_seconds < profile.maximum_step_seconds ? remaining_seconds : profile.maximum_step_seconds;
        if (!response(result, clamped_target, step_seconds, profile) || !finite(result)) return false;
        remaining_seconds -= step_seconds;
    }

    // A long scheduling pause is intentionally truncated after the fixed work
    // budget. The next frame continues converging instead of causing a spike.
    result.value = clamp(result.value, range);
    state        = result;
    return true;
}

}  // namespace pet::preset
