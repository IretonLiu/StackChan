/*
 * SPDX-License-Identifier: MIT
 */
#include <pet/preset/face_motion.h>

#include <cassert>
#include <cmath>

namespace {
using pet::preset::MotionEasing;
using pet::preset::MotionKeyframe;
using pet::preset::MotionRange;

bool near(float left, float right, float epsilon = 0.0005f)
{
    return std::fabs(left - right) <= epsilon;
}

void testLoopQuarterPeriod()
{
    pet::preset::LoopMotion motion;
    motion.amplitude      = 4.0f;
    motion.period_seconds = 2.0f;
    motion.phase_radians  = 0.0f;
    float output          = 0.0f;
    assert(pet::preset::evaluateLoopMotion(10.0f, motion, 0, {-100.0f, 100.0f}, output));
    assert(near(output, 10.0f));
    assert(pet::preset::evaluateLoopMotion(10.0f, motion, 500, {-100.0f, 100.0f}, output));
    assert(near(output, 14.0f));
    assert(pet::preset::evaluateLoopMotion(10.0f, motion, 1000, {-100.0f, 100.0f}, output));
    assert(near(output, 10.0f));
    assert(pet::preset::evaluateLoopMotion(10.0f, motion, 1500, {-100.0f, 100.0f}, output));
    assert(near(output, 6.0f));
}

void testLoopClamps()
{
    pet::preset::LoopMotion motion;
    motion.amplitude      = 40.0f;
    motion.period_seconds = 4.0f;
    motion.phase_radians  = 1.57079632679f;
    float output          = 0.0f;
    assert(pet::preset::evaluateLoopMotion(90.0f, motion, 0, {0.0f, 100.0f}, output));
    assert(near(output, 100.0f));
}

void testLoopRejectsInvalid()
{
    pet::preset::LoopMotion motion;
    float output = 0.0f;
    assert(!pet::preset::evaluateLoopMotion(0.0f, motion, 0, {1.0f, 0.0f}, output));
    motion.period_seconds = 0.0f;
    assert(!pet::preset::evaluateLoopMotion(0.0f, motion, 0, {0.0f, 1.0f}, output));
    motion.period_seconds = 1.0f;
    motion.amplitude      = INFINITY;
    assert(!pet::preset::evaluateLoopMotion(0.0f, motion, 0, {0.0f, 1.0f}, output));
}

void testKeyframeInterpolationAndEasing()
{
    pet::preset::KeyframeMotion motion;
    motion.count        = 2;
    motion.keyframes[0] = {0.0f, 0.0f};
    motion.keyframes[1] = {1.0f, 10.0f};
    float output        = 0.0f;
    assert(pet::preset::evaluateKeyframeMotion(motion, 0, {0.0f, 20.0f}, MotionEasing::Linear, output));
    assert(near(output, 0.0f));
    assert(pet::preset::evaluateKeyframeMotion(motion, 250, {0.0f, 20.0f}, MotionEasing::Linear, output));
    assert(near(output, 2.5f));
    assert(pet::preset::evaluateKeyframeMotion(motion, 250, {0.0f, 20.0f}, MotionEasing::Smoothstep, output));
    assert(near(output, 1.5625f));
    assert(pet::preset::evaluateKeyframeMotion(motion, 1000, {0.0f, 20.0f}, MotionEasing::Linear, output));
    assert(near(output, 10.0f));
    assert(pet::preset::evaluateKeyframeMotion(motion, 5000, {0.0f, 20.0f}, MotionEasing::Linear, output));
    assert(near(output, 10.0f));
}

void testKeyframeLoopAndReset()
{
    pet::preset::KeyframeMotion motion;
    motion.count            = 2;
    motion.loop             = true;
    motion.duration_seconds = 1.0f;
    motion.keyframes[0]     = {0.0f, 2.0f};
    motion.keyframes[1]     = {1.0f, 6.0f};
    float output            = 0.0f;
    assert(pet::preset::evaluateKeyframeMotion(motion, 0, {0.0f, 10.0f}, MotionEasing::Linear, output));
    assert(near(output, 2.0f));
    assert(pet::preset::evaluateKeyframeMotion(motion, 1500, {0.0f, 10.0f}, MotionEasing::Linear, output));
    assert(near(output, 4.0f));
}

void testKeyframeRejectsInvalid()
{
    pet::preset::KeyframeMotion motion;
    float output = 0.0f;
    assert(!pet::preset::evaluateKeyframeMotion(motion, 0, {0.0f, 1.0f}, MotionEasing::Linear, output));
    motion.count        = 2;
    motion.keyframes[0] = {1.0f, 0.0f};
    motion.keyframes[1] = {0.5f, 1.0f};
    assert(!pet::preset::evaluateKeyframeMotion(motion, 0, {0.0f, 1.0f}, MotionEasing::Linear, output));
    motion.keyframes[0] = {0.0f, INFINITY};
    motion.keyframes[1] = {1.0f, 1.0f};
    assert(!pet::preset::evaluateKeyframeMotion(motion, 0, {0.0f, 1.0f}, MotionEasing::Linear, output));
}

}  // namespace

int main()
{
    testLoopQuarterPeriod();
    testLoopClamps();
    testLoopRejectsInvalid();
    testKeyframeInterpolationAndEasing();
    testKeyframeLoopAndReset();
    testKeyframeRejectsInvalid();
    return 0;
}
