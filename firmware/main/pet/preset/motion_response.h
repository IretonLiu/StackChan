/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cmath>
#include <cstdint>

namespace pet::preset {

/** Scalar state owned by the caller for one animated property. */
struct MotionResponseState {
    float value    = 0.0f;
    float velocity = 0.0f;
};

/**
 * Response tuning shared by the host preview and firmware evaluator.
 *
 * `maximum_step_seconds` bounds integration work after a delayed frame. The
 * response implementation may use the other fields or deliberately ignore
 * them, allowing future response models to share the same evaluator boundary.
 */
struct MotionResponseProfile {
    float frequency_hz         = 6.0f;
    float damping_ratio        = 1.0f;
    float maximum_step_seconds = 1.0f / 120.0f;
};

struct MotionResponseRange {
    float minimum = 0.0f;
    float maximum = 0.0f;
};

/**
 * Validates tuning before integration. Shared by the wrapper and the pose engine
 * so the two call sites cannot drift apart. A negative damping ratio or a
 * non-positive frequency would produce an unstable or non-physical response.
 */
[[nodiscard]] inline bool validMotionResponseProfile(const MotionResponseProfile& profile)
{
    return std::isfinite(profile.frequency_hz) && std::isfinite(profile.damping_ratio) &&
           std::isfinite(profile.maximum_step_seconds) && profile.frequency_hz > 0.0f &&
           profile.damping_ratio >= 0.0f && profile.maximum_step_seconds > 0.0f;
}

/**
 * A replaceable, allocation-free scalar response implementation.
 *
 * The wrapper validates all inputs and supplies a finite, range-clamped target.
 * Implementations return false instead of mutating a state with invalid output.
 */
using MotionResponseFunction = bool (*)(MotionResponseState& state, float target, float delta_seconds,
                                        const MotionResponseProfile& profile);

/** Initial built-in response; callers are not required to use it. */
[[nodiscard]] bool advanceCriticallyDampedSpring(MotionResponseState& state, float target, float delta_seconds,
                                                 const MotionResponseProfile& profile);

/**
 * Advances a state through an injected response implementation.
 *
 * Work is bounded to eight substeps, target/value are clamped to `range`, and
 * `state` remains unchanged when validation or the response function fails.
 */
[[nodiscard]] bool advanceMotionResponse(MotionResponseState& state, float target, float delta_seconds,
                                         MotionResponseRange range, const MotionResponseProfile& profile,
                                         MotionResponseFunction response);

}  // namespace pet::preset
