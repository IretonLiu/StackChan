/*
 * SPDX-License-Identifier: MIT
 */
#include "pet_face_behaviour_controller.h"

namespace pet::behaviour {
namespace {

PetFaceBehaviourOutput animation(PetFaceAnimationCommand command)
{
    PetFaceBehaviourOutput output;
    output.animation = command;
    return output;
}

}  // namespace

void PetFaceBehaviourController::initialize(uint32_t now_ms)
{
    state_               = PetFaceBehaviourState::Awake;
    last_interaction_ms_ = now_ms;
    happy_until_ms_      = 0;
    wake_until_ms_       = 0;
    saved_brightness_    = 0;
    happy_pending_       = false;
    happy_return_armed_  = false;
    brightness_overridden_ = false;
}

PetFaceBehaviourOutput PetFaceBehaviourController::handle(PetFaceInput input, uint32_t now_ms,
                                                           uint8_t current_brightness)
{
    // Every direct control interaction postpones drowsiness, even where it does
    // not select an expression itself (for example, a head press before a stroke).
    last_interaction_ms_ = now_ms;

    if (state_ == PetFaceBehaviourState::Sleepy) {
        PetFaceBehaviourOutput output = beginWake(now_ms);
        if (input == PetFaceInput::HeadStroke) happy_pending_ = true;
        return output;
    }

    if (state_ == PetFaceBehaviourState::Waking) {
        // Do not skip the visible wake sequence. A stroke selects Happy when it
        // finishes, preserving both the wake acknowledgement and pet response.
        if (input == PetFaceInput::HeadStroke) happy_pending_ = true;
        return {};
    }

    switch (input) {
        case PetFaceInput::ScreenTap:
            // A screen tap is acknowledgement only; it intentionally does not
            // replace the real head-stroke route with the Happy expression.
            return state_ == PetFaceBehaviourState::Awake ? animation(PetFaceAnimationCommand::ScreenTap)
                                                           : PetFaceBehaviourOutput{};
        case PetFaceInput::HeadStroke:
            state_              = PetFaceBehaviourState::Happy;
            happy_return_armed_ = false;
            return animation(PetFaceAnimationCommand::HeadStroke);
        case PetFaceInput::HeadRelease:
            if (state_ == PetFaceBehaviourState::Happy) {
                happy_until_ms_      = now_ms + kHappyHoldMs;
                happy_return_armed_ = true;
            }
            return {};
        case PetFaceInput::Shake:
            return animation(PetFaceAnimationCommand::Shake);
        case PetFaceInput::HeadPress:
            return {};
    }
    (void)current_brightness;
    return {};
}

PetFaceBehaviourOutput PetFaceBehaviourController::advance(uint32_t now_ms, uint8_t current_brightness)
{
    if (state_ == PetFaceBehaviourState::Waking && elapsed(now_ms, wake_until_ms_)) {
        if (happy_pending_) {
            state_         = PetFaceBehaviourState::Happy;
            happy_pending_ = false;
            return animation(PetFaceAnimationCommand::HeadStroke);
        }
        state_ = PetFaceBehaviourState::Awake;
        return animation(PetFaceAnimationCommand::WakeComplete);
    }

    if (state_ == PetFaceBehaviourState::Happy && happy_return_armed_ && elapsed(now_ms, happy_until_ms_)) {
        state_              = PetFaceBehaviourState::Awake;
        happy_return_armed_ = false;
        return animation(PetFaceAnimationCommand::HappyComplete);
    }

    if (state_ == PetFaceBehaviourState::Awake && elapsed(now_ms, last_interaction_ms_ + kSleepAfterMs)) {
        state_            = PetFaceBehaviourState::Sleepy;
        saved_brightness_ = current_brightness;
        PetFaceBehaviourOutput output = animation(PetFaceAnimationCommand::SleepTimeout);
        output.brightness_changed     = true;
        output.brightness             = static_cast<uint8_t>(saved_brightness_ / 2u);
        brightness_overridden_        = true;
        return output;
    }
    return {};
}

PetFaceBehaviourOutput PetFaceBehaviourController::beginWake(uint32_t now_ms)
{
    state_         = PetFaceBehaviourState::Waking;
    wake_until_ms_ = now_ms + kWakeDurationMs;
    PetFaceBehaviourOutput output = animation(PetFaceAnimationCommand::Wake);
    output.brightness_changed     = true;
    output.brightness             = saved_brightness_;
    brightness_overridden_        = false;
    return output;
}

PetFaceBehaviourOutput PetFaceBehaviourController::shutdown()
{
    PetFaceBehaviourOutput output;
    if (brightness_overridden_) {
        output.brightness_changed = true;
        output.brightness         = saved_brightness_;
        brightness_overridden_    = false;
    }
    return output;
}

bool PetFaceBehaviourController::elapsed(uint32_t now_ms, uint32_t deadline_ms)
{
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

}  // namespace pet::behaviour
