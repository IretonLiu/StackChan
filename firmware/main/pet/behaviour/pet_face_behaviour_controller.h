/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

namespace pet::behaviour {

/** Input preserved from the physical controls, without face-policy decisions. */
enum class PetFaceInput : uint8_t {
    ScreenTap,
    HeadPress,
    HeadStroke,
    HeadRelease,
    Shake,
};

/** Semantic command consumed by the compiled face-animation event router. */
enum class PetFaceAnimationCommand : uint8_t {
    None,
    ScreenTap,
    HeadStroke,
    Shake,
    SleepTimeout,
    Wake,
    HappyComplete,
    WakeComplete,
};

struct PetFaceBehaviourOutput {
    PetFaceAnimationCommand animation = PetFaceAnimationCommand::None;
    bool brightness_changed            = false;
    uint8_t brightness                 = 0;
};

enum class PetFaceBehaviourState : uint8_t {
    Awake,
    Happy,
    Sleepy,
    Waking,
};

/**
 * Fixed-state policy for the first pet expression collection.
 *
 * This class reads no clock and no hardware. Its caller supplies elapsed time
 * and current brightness, then forwards the returned semantic animation and
 * temporary brightness commands through the appropriate gateways.
 */
class PetFaceBehaviourController {
public:
    static constexpr uint32_t kSleepAfterMs = 120000;
    static constexpr uint32_t kHappyHoldMs  = 3000;
    static constexpr uint32_t kWakeDurationMs = 800;

    void initialize(uint32_t now_ms);
    [[nodiscard]] PetFaceBehaviourOutput handle(PetFaceInput input, uint32_t now_ms,
                                                uint8_t current_brightness);
    [[nodiscard]] PetFaceBehaviourOutput advance(uint32_t now_ms, uint8_t current_brightness);

    /** Restores a temporary sleepy brightness override before app teardown. */
    [[nodiscard]] PetFaceBehaviourOutput shutdown();

    [[nodiscard]] PetFaceBehaviourState state() const { return state_; }

private:
    [[nodiscard]] PetFaceBehaviourOutput beginWake(uint32_t now_ms);
    [[nodiscard]] static bool elapsed(uint32_t now_ms, uint32_t deadline_ms);

    PetFaceBehaviourState state_ = PetFaceBehaviourState::Awake;
    uint32_t last_interaction_ms_ = 0;
    uint32_t happy_until_ms_      = 0;
    uint32_t wake_until_ms_       = 0;
    uint8_t saved_brightness_     = 0;
    bool happy_pending_           = false;
    bool happy_return_armed_      = false;
    bool brightness_overridden_   = false;
};

}  // namespace pet::behaviour
