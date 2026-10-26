/*
 * SPDX-License-Identifier: MIT
 */
#include <pet/face/face.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using pet::face::ExpressionController;
using pet::face::ExpressionId;
using pet::face::FacePose;
using pet::face::Transition;

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "Face expression contract failed: " << message << '\n';
        std::exit(1);
    }
}

float difference(const FacePose& left, const FacePose& right)
{
    return std::fabs(left.left_eye.center.x - right.left_eye.center.x) +
           std::fabs(left.left_eye.center.y - right.left_eye.center.y) +
           std::fabs(left.left_eye.openness - right.left_eye.openness) +
           std::fabs(left.right_eye.center.x - right.right_eye.center.x) +
           std::fabs(left.right_eye.center.y - right.right_eye.center.y) +
           std::fabs(left.right_eye.openness - right.right_eye.openness) +
           std::fabs(left.mouth.width - right.mouth.width) +
           std::fabs(left.mouth.curve_depth - right.mouth.curve_depth) +
           std::fabs(left.mouth.openness - right.mouth.openness) +
           std::fabs(left.accent_intensity - right.accent_intensity) + std::fabs(left.visual_tilt - right.visual_tilt);
}

void testAllSemanticPresetsProduceSafeGeometry()
{
    constexpr ExpressionId expressions[] = {
        ExpressionId::Idle,      ExpressionId::Curious, ExpressionId::Happy,   ExpressionId::Excited,
        ExpressionId::Hungry,    ExpressionId::Lonely,  ExpressionId::Sleepy,  ExpressionId::Sleeping,
        ExpressionId::Waking,    ExpressionId::Petted,  ExpressionId::Fed,     ExpressionId::Startled,
        ExpressionId::Paired,
    };

    for (ExpressionId expression : expressions) {
        for (float intensity : {-1.0f, 0.0f, 0.5f, 1.0f, 2.0f}) {
            expect(pet::face::isValid(pet::face::preset(expression, intensity)),
                   "every semantic preset remains renderable for bounded or unbounded intensity input");
        }
    }
}

void testInterruptedTransitionStartsFromCurrentPose()
{
    ExpressionController controller(42);
    controller.request(ExpressionId::Happy, 1.0f, Transition{240}, 0);
    const FacePose before_interrupt = controller.sample(100);

    controller.request(ExpressionId::Startled, 1.0f, Transition{240}, 100);
    const FacePose after_interrupt = controller.sample(100);

    expect(difference(before_interrupt, after_interrupt) < 0.001f,
           "an interrupted expression transition has no geometry discontinuity");
    expect(pet::face::isValid(controller.sample(220)), "transition midpoint remains renderable");
    expect(pet::face::isValid(controller.sample(400)), "transition completion remains renderable");
}

void testSameSeedAndTimelineAreReproducible()
{
    ExpressionController first(0xC0FFEE);
    ExpressionController second(0xC0FFEE);
    first.request(ExpressionId::Curious, 0.8f, Transition{180}, 0);
    second.request(ExpressionId::Curious, 0.8f, Transition{180}, 0);

    for (uint32_t now = 0; now <= 6000; now += 40) {
        expect(difference(first.sample(now), second.sample(now)) < 0.001f,
               "a fixed seed and timeline produce the same face sequence");
    }
}

void testSleepingFaceCannotBeOpenedByBlinkOverlay()
{
    ExpressionController controller(7);
    controller.request(ExpressionId::Sleeping, 1.0f, Transition{0}, 0);
    for (uint32_t now = 0; now <= 8000; now += 50) {
        const FacePose pose = controller.sample(now);
        expect(pose.left_eye.openness == 0.0f && pose.right_eye.openness == 0.0f,
               "the blink overlay does not open a sleeping face");
    }
}

}  // namespace

int main()
{
    testAllSemanticPresetsProduceSafeGeometry();
    testInterruptedTransitionStartsFromCurrentPose();
    testSameSeedAndTimelineAreReproducible();
    testSleepingFaceCannotBeOpenedByBlinkOverlay();
    return 0;
}
