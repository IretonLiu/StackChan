/*
 * SPDX-License-Identifier: MIT
 */
#include <pet/preset/motion_response.h>

#include <cassert>
#include <cmath>
#include <limits>

namespace {

using pet::preset::MotionResponseProfile;
using pet::preset::MotionResponseRange;
using pet::preset::MotionResponseState;

bool near(float left, float right, float epsilon = 0.002f)
{
    return std::fabs(left - right) <= epsilon;
}

uint8_t response_calls = 0;

bool snapToTarget(MotionResponseState& state, float target, float, const MotionResponseProfile&)
{
    ++response_calls;
    state.value    = target;
    state.velocity = 42.0f;
    return true;
}

bool emitNonFinite(MotionResponseState& state, float, float, const MotionResponseProfile&)
{
    state.value = std::numeric_limits<float>::quiet_NaN();
    return true;
}

void testInjectedResponseIsUsedAndBounded()
{
    MotionResponseState state{0.0f, 0.0f};
    MotionResponseProfile profile;
    profile.maximum_step_seconds = 0.01f;
    response_calls               = 0;
    assert(pet::preset::advanceMotionResponse(state, 20.0f, 0.2f, {0.0f, 10.0f}, profile, snapToTarget));
    assert(response_calls == 8);
    assert(near(state.value, 10.0f));
    assert(near(state.velocity, 42.0f));
}

void testSpringConvergesToGoal()
{
    MotionResponseState state{0.0f, 0.0f};
    MotionResponseProfile profile;
    profile.frequency_hz         = 3.0f;
    profile.damping_ratio        = 1.0f;
    profile.maximum_step_seconds = 1.0f / 120.0f;
    for (int frame = 0; frame < 720; ++frame) {
        assert(pet::preset::advanceMotionResponse(state, 10.0f, 1.0f / 120.0f, {-20.0f, 20.0f}, profile,
                                                  pet::preset::advanceCriticallyDampedSpring));
    }
    assert(near(state.value, 10.0f, 0.01f));
    assert(near(state.velocity, 0.0f, 0.05f));
}

void testTargetsAndOutputsAreClamped()
{
    MotionResponseState state{0.0f, 0.0f};
    MotionResponseProfile profile;
    assert(pet::preset::advanceMotionResponse(state, 100.0f, 0.01f, {-2.0f, 3.0f}, profile, snapToTarget));
    assert(near(state.value, 3.0f));

    state = {9.0f, 0.0f};
    assert(pet::preset::advanceMotionResponse(state, 0.0f, 0.0f, {-2.0f, 3.0f}, profile, snapToTarget));
    assert(near(state.value, 3.0f));
}

void testFailureDoesNotMutateState()
{
    MotionResponseState state{2.0f, -1.0f};
    const MotionResponseState preserved = state;
    MotionResponseProfile profile;
    assert(!pet::preset::advanceMotionResponse(state, 3.0f, 0.01f, {0.0f, 10.0f}, profile, emitNonFinite));
    assert(near(state.value, preserved.value));
    assert(near(state.velocity, preserved.velocity));

    profile.maximum_step_seconds = 0.0f;
    assert(!pet::preset::advanceMotionResponse(state, 3.0f, 0.01f, {0.0f, 10.0f}, profile, snapToTarget));
    assert(near(state.value, preserved.value));
    assert(near(state.velocity, preserved.velocity));
}

}  // namespace

int main()
{
    testInjectedResponseIsUsedAndBounded();
    testSpringConvergesToGoal();
    testTargetsAndOutputsAreClamped();
    testFailureDoesNotMutateState();
    return 0;
}
