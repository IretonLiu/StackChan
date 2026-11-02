#include <pet/behaviour/pet_face_behaviour_controller.h>

#include <cassert>

namespace {

using pet::behaviour::PetFaceAnimationCommand;
using pet::behaviour::PetFaceBehaviourController;
using pet::behaviour::PetFaceBehaviourState;
using pet::behaviour::PetFaceInput;

void testScreenTapAlertsButDoesNotBecomeHappy()
{
    PetFaceBehaviourController controller;
    controller.initialize(100);
    const auto output = controller.handle(PetFaceInput::ScreenTap, 200, 80);
    assert(output.animation == PetFaceAnimationCommand::ScreenTap);
    assert(controller.state() == PetFaceBehaviourState::Awake);
}

void testStrokeAndReleaseKeepHappyForThreeSeconds()
{
    PetFaceBehaviourController controller;
    controller.initialize(0);
    assert(controller.handle(PetFaceInput::HeadStroke, 100, 80).animation == PetFaceAnimationCommand::HeadStroke);
    assert(controller.state() == PetFaceBehaviourState::Happy);
    static_cast<void>(controller.handle(PetFaceInput::HeadRelease, 200, 80));
    assert(controller.advance(3199, 80).animation == PetFaceAnimationCommand::None);
    assert(controller.advance(3200, 80).animation == PetFaceAnimationCommand::HappyComplete);
    assert(controller.state() == PetFaceBehaviourState::Awake);
}

void testSleepAndWakeRestoreBrightness()
{
    PetFaceBehaviourController controller;
    controller.initialize(0);
    const auto sleepy = controller.advance(PetFaceBehaviourController::kSleepAfterMs, 81);
    assert(sleepy.animation == PetFaceAnimationCommand::SleepTimeout);
    assert(sleepy.brightness_changed && sleepy.brightness == 40);
    assert(controller.state() == PetFaceBehaviourState::Sleepy);

    const auto waking = controller.handle(PetFaceInput::ScreenTap, PetFaceBehaviourController::kSleepAfterMs + 1, 40);
    assert(waking.animation == PetFaceAnimationCommand::Wake);
    assert(waking.brightness_changed && waking.brightness == 81);
    assert(controller.state() == PetFaceBehaviourState::Waking);
    assert(controller.advance(PetFaceBehaviourController::kSleepAfterMs + 801, 81).animation ==
           PetFaceAnimationCommand::WakeComplete);
    assert(controller.state() == PetFaceBehaviourState::Awake);
}

void testShutdownRestoresOnlyAnActiveSleepOverride()
{
    PetFaceBehaviourController controller;
    controller.initialize(0);
    assert(!controller.shutdown().brightness_changed);

    static_cast<void>(controller.advance(PetFaceBehaviourController::kSleepAfterMs, 81));
    const auto restore = controller.shutdown();
    assert(restore.brightness_changed && restore.brightness == 81);
    assert(!controller.shutdown().brightness_changed);
}

void testStrokeDuringWakeWaitsForWakeSequence()
{
    PetFaceBehaviourController controller;
    controller.initialize(0);
    static_cast<void>(controller.advance(PetFaceBehaviourController::kSleepAfterMs, 70));
    const uint32_t wake_started = PetFaceBehaviourController::kSleepAfterMs + 1;
    static_cast<void>(controller.handle(PetFaceInput::HeadPress, wake_started, 35));
    assert(controller.handle(PetFaceInput::HeadStroke, wake_started + 100, 35).animation ==
           PetFaceAnimationCommand::None);
    assert(controller.advance(wake_started + PetFaceBehaviourController::kWakeDurationMs, 70).animation ==
           PetFaceAnimationCommand::HeadStroke);
    assert(controller.state() == PetFaceBehaviourState::Happy);
}

}  // namespace

int main()
{
    testScreenTapAlertsButDoesNotBecomeHappy();
    testStrokeAndReleaseKeepHappyForThreeSeconds();
    testSleepAndWakeRestoreBrightness();
    testShutdownRestoresOnlyAnActiveSleepOverride();
    testStrokeDuringWakeWaitsForWakeSequence();
    return 0;
}
